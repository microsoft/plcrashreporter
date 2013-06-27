/*
 * Copyright (c) 2013 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "PLCrashFrameDWARFUnwind.h"
#include "PLCrashAsyncMachOImage.h"

#include "PLCrashAsyncDwarfCFA.hpp"

#include <inttypes.h>

/**
 * Attempt to fetch next frame using compact frame unwinding data from @a image_list.
 *
 * @param task The task containing the target frame stack.
 * @param image_list The list of images loaded in the target @a task.
 * @param current_frame The current stack frame.
 * @param previous_frame The previous stack frame, or NULL if this is the first frame.
 * @param next_frame The new frame to be initialized.
 *
 * @return Returns PLFRAME_ESUCCESS on success, PLFRAME_ENOFRAME is no additional frames are available, or a standard plframe_error_t code if an error occurs.
 */
plframe_error_t plframe_cursor_read_dwarf_unwind (task_t task,
                                                  plcrash_async_image_list_t *image_list,
                                                  const plframe_stackframe_t *current_frame,
                                                  const plframe_stackframe_t *previous_frame,
                                                  plframe_stackframe_t *next_frame)
{
    plcrash_async_mobject_t eh_frame;
    plcrash_async_mobject_t debug_frame;
    plcrash_async_mobject_t *dwarf_section = NULL;
    bool is_debug_frame = false;

    plcrash::async::dwarf_cfa_state state;
    
    plframe_error_t result;
    plcrash_error_t err;

    /* Fetch the IP. It should always be available */
    if (!plcrash_async_thread_state_has_reg(&current_frame->thread_state, PLCRASH_REG_IP)) {
        PLCF_DEBUG("Frame is missing a valid IP register, skipping compact unwind encoding");
        return PLFRAME_EBADFRAME;
    }
    plcrash_greg_t pc = plcrash_async_thread_state_get_reg(&current_frame->thread_state, PLCRASH_REG_IP);
    
    /* Find the corresponding image */
    plcrash_async_image_list_set_reading(image_list, true);
    plcrash_async_image_t *image = plcrash_async_image_containing_address(image_list, pc);
    if (image == NULL) {
        PLCF_DEBUG("Could not find a loaded image for the current frame pc: 0x%" PRIx64, (uint64_t) pc);
        result = PLFRAME_ENOTSUP;
        goto cleanup;
    }
    
    /*
     * Map the eh_frame or debug_frame DWARF sections. Apple doesn't seem to use debug_frame at all;
     * as such, we prefer eh_frame, but allow falling back on debug_frame.
     */
    err = plcrash_async_macho_map_section(&image->macho_image, "__DWARF", "__eh_frame", &eh_frame);
    if (err == PLCRASH_ESUCCESS) {
        dwarf_section = &eh_frame;
    }

    if (dwarf_section == NULL) {
        err = plcrash_async_macho_map_section(&image->macho_image, "__DWARF", "__debug_frame", &debug_frame);
        if (err == PLCRASH_ESUCCESS) {
            dwarf_section = &debug_frame;
            is_debug_frame = true;
        }
    }

    /* If neither, there's nothing to do */
    if (dwarf_section == NULL) {
        PLCF_DEBUG("Could not find a debug_frame or eh_frame section for the current frame pc: 0x%" PRIx64, (uint64_t) pc);
        result = PLFRAME_ENOFRAME;
        goto cleanup;
    }

    /* Initialize the reader. */
    plcrash_async_dwarf_frame_reader_t reader;
    uint8_t address_size;

    address_size = image->macho_image.m64 ? 8 : 4;
    if ((err = plcrash_async_dwarf_frame_reader_init(&reader, dwarf_section, image->macho_image.byteorder, address_size, is_debug_frame)) != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Could not initialize a %s DWARF parser for the current frame pc: 0x%" PRIx64 " %d", (is_debug_frame ? "debug_frame" : "eh_frame"), (uint64_t) pc, err);
        result = PLFRAME_EINVAL;
        goto cleanup;
    }
    
    /* Find the FDE (if any) and free the reader */
    plcrash_async_dwarf_fde_info_t fde_info;
    err = plcrash_async_dwarf_frame_reader_find_fde(&reader, 0x0 /* offset hint */, pc - image->macho_image.header_addr, &fde_info);
    plcrash_async_dwarf_frame_reader_free(&reader);

    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Did not find CFE entry for PC 0x%" PRIx64 ": %d", (uint64_t) pc, err);
        result = PLFRAME_ENOTSUP;
        goto cleanup;
    }

    /* Initialize pointer state */
    plcrash_async_dwarf_gnueh_ptr_state_t ptr_state;
    plcrash_async_dwarf_gnueh_ptr_state_init(&ptr_state, address_size);
    // TODO - configure the pointer state */

    /* Parse CIE info */
    plcrash_async_dwarf_cie_info_t cie_info;
    err = plcrash_async_dwarf_cie_info_init(&cie_info, dwarf_section, image->macho_image.byteorder, &ptr_state, plcrash_async_mobject_base_address(dwarf_section) + fde_info.cie_offset);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to parse CIE at offset of 0x%" PRIx64 ": %d", (uint64_t) fde_info.cie_offset, err);
        result = PLFRAME_ENOTSUP;
        
        plcrash_async_dwarf_fde_info_free(&fde_info);
        plcrash_async_dwarf_gnueh_ptr_state_free(&ptr_state);
        goto cleanup;
    }

    err = plcrash_async_dwarf_cfa_eval_program(dwarf_section, pc - image->macho_image.header_addr, &cie_info, &ptr_state, image->macho_image.byteorder, plcrash_async_mobject_base_address(dwarf_section), fde_info.instruction_offset, fde_info.fde_length, &state);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Failed to evaluate CFA at offset of 0x%" PRIx64 ": %d", (uint64_t) fde_info.instruction_offset, err);
        result = PLFRAME_ENOTSUP;
        
        plcrash_async_dwarf_fde_info_free(&fde_info);
        plcrash_async_dwarf_gnueh_ptr_state_free(&ptr_state);
        goto cleanup;
    }

    

    /* Clean up the FDE record */
    plcrash_async_dwarf_fde_info_free(&fde_info);
    plcrash_async_dwarf_gnueh_ptr_state_free(&ptr_state);

    // TODO
    result = PLFRAME_ESUCCESS;

cleanup:
    if (dwarf_section != NULL)
        plcrash_async_mobject_free(dwarf_section);

    return result;
}