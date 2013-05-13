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

#include "PLCrashFrameCompactUnwind.h"
#include "PLCrashAsyncCompactUnwindEncoding.h"

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
plframe_error_t plframe_cursor_read_compact_unwind (task_t task,
                                                    plcrash_async_image_list_t *image_list,
                                                    const plframe_stackframe_t *current_frame,
                                                    const plframe_stackframe_t *previous_frame,
                                                    plframe_stackframe_t *next_frame)
{
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
    
    /* Map the unwind section */
    plcrash_async_mobject_t unwind_mobj;
    err = plcrash_async_macho_map_section(&image->macho_image, SEG_TEXT, "__unwind_info", &unwind_mobj);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Could not map the compact unwind info section for image %s: %d", image->macho_image.name, err);
        result = PLFRAME_ENOTSUP;
        goto cleanup;
    }

    /* Initialize the CFE reader. */
    cpu_type_t cputype = image->macho_image.byteorder->swap32(image->macho_image.header.cputype);
    plcrash_async_cfe_reader_t reader;

    err = plcrash_async_cfe_reader_init(&reader, &unwind_mobj, cputype);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Could not parse the compact unwind info section for image '%s': %d", image->macho_image.name, err);
        result = PLFRAME_EINVAL;
        goto cleanup;
    }

    /* Find the encoding entry (if any) and free the reader */
    uint32_t encoding;
    err = plcrash_async_cfe_reader_find_pc(&reader, pc, &encoding);
    plcrash_async_cfe_reader_free(&reader);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Did not find CFE entry for PC 0x%" PRIx64 ": %d", (uint64_t) pc, err);
        result = PLFRAME_ENOTSUP;
        goto cleanup;
    }
    
    /* Decode the entry */
    plcrash_async_cfe_entry_t entry;
    err = plcrash_async_cfe_entry_init(&entry, cputype, encoding);
    if (err != PLCRASH_ESUCCESS) {
        PLCF_DEBUG("Could not decode CFE encoding 0x%" PRIx32 " for PC 0x%" PRIx64 ": %d", encoding, (uint64_t) pc, err);
        result = PLFRAME_ENOTSUP;
        goto cleanup;
    }

    /* Handle the case where no unwind data is available from a valid entry */
    plcrash_async_cfe_entry_type_t entry_type = plcrash_async_cfe_entry_type(&entry);
    if (entry_type == PLCRASH_ASYNC_CFE_ENTRY_TYPE_NONE) {
        result = PLFRAME_ENOTSUP;
        goto cleanup;
    }

    /* Apply the frame delta */
    // TODO
    result = PLFRAME_EUNKNOWN;
    uint32_t register_count = plcrash_async_cfe_entry_register_count(&entry);
    plcrash_regnum_t register_list[PLCRASH_ASYNC_CFE_SAVED_REGISTER_MAX];
    
    plcrash_async_cfe_entry_register_list(&entry, register_list);
    for (uint32_t i = 0; i < register_count; i++) {
        const char *name = plcrash_async_thread_state_get_reg_name(&current_frame->thread_state, register_list[i]);
        PLCF_DEBUG("Register name saved: %s", name);
    }
    
    plcrash_async_cfe_entry_free(&entry);

cleanup:
    plcrash_async_image_list_set_reading(image_list, false);
    return result;
}