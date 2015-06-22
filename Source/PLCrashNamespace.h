/*
 * Author: Landon Fuller <landonf@plausible.coop>
 *
 * Copyright (c) 2012-2013 Plausible Labs Cooperative, Inc.
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

/*
 * For external library integrators:
 *
 * Set this value to any valid C symbol prefix. This will automatically
 * prepend the given prefix to all external symbols in the library.
 *
 * This may be used to avoid symbol conflicts between multiple libraries
 * that may both incorporate PLCrashReporter.
 */
// #define PLCRASHREPORTER_PREFIX AcmeCo


// We need two extra layers of indirection to make CPP substitute
// the PLCRASHREPORTER_PREFIX define.
#define PLNS_impl2(prefix, symbol) prefix ## symbol
#define PLNS_impl(prefix, symbol) PLNS_impl2(prefix, symbol)
#define PLNS(symbol) PLNS_impl(PLCRASHREPORTER_PREFIX, symbol)

#ifdef PLCRASHREPORTER_PREFIX

#define PLCrashMachExceptionServer          PLNS(PLCrashMachExceptionServer)
#define PLCrashReport                       PLNS(PLCrashReport)
#define PLCrashReportApplicationInfo        PLNS(PLCrashReportApplicationInfo)
#define PLCrashReportBinaryImageInfo        PLNS(PLCrashReportBinaryImageInfo)
#define PLCrashReportExceptionInfo          PLNS(PLCrashReportExceptionInfo)
#define PLCrashReportMachExceptionInfo      PLNS(PLCrashReportMachExceptionInfo)
#define PLCrashReportMachineInfo            PLNS(PLCrashReportMachineInfo)
#define PLCrashReportProcessInfo            PLNS(PLCrashReportProcessInfo)
#define PLCrashReportProcessorInfo          PLNS(PLCrashReportProcessorInfo)
#define PLCrashReportRegisterInfo           PLNS(PLCrashReportRegisterInfo)
#define PLCrashReportSignalInfo             PLNS(PLCrashReportSignalInfo)
#define PLCrashReportStackFrameInfo         PLNS(PLCrashReportStackFrameInfo)
#define PLCrashReportSymbolInfo             PLNS(PLCrashReportSymbolInfo)
#define PLCrashReportSystemInfo             PLNS(PLCrashReportSystemInfo)
#define PLCrashReportTextFormatter          PLNS(PLCrashReportTextFormatter)
#define PLCrashReportThreadInfo             PLNS(PLCrashReportThreadInfo)
#define PLCrashReporter                     PLNS(PLCrashReporter)
#define PLCrashSignalHandler                PLNS(PLCrashSignalHandler)
#define PLCrashReportHostArchitecture       PLNS(PLCrashReportHostArchitecture)
#define PLCrashReportHostOperatingSystem    PLNS(PLCrashReportHostOperatingSystem)
#define PLCrashReporterErrorDomain          PLNS(PLCrashReporterErrorDomain)
#define PLCrashReporterException            PLNS(PLCrashReporterException)
#define PLCrashHostInfo                     PLNS(PLCrashHostInfo)
#define PLCrashMachExceptionPort            PLNS(PLCrashMachExceptionPort)
#define PLCrashMachExceptionPortSet         PLNS(PLCrashMachExceptionPortSet)
#define PLCrashProcessInfo                  PLNS(PLCrashProcessInfo)
#define PLCrashReporterConfig               PLNS(PLCrashReporterConfig)
#define PLCrashUncaughtExceptionHandler     PLNS(PLCrashUncaughtExceptionHandler)
#define PLCrashMachExceptionForward         PLNS(PLCrashMachExceptionForward)
#define PLCrashSignalHandlerForward         PLNS(PLCrashSignalHandlerForward)
#define plcrash_signal_handler              PLNS(plcrash_signal_handler)

#endif

/*
 * The following symbols are exported by the protobuf-c library. When building
 * a shared library, we can hide these as private symbols.
 *
 * However, when building a static library, we can only do so if we use
 * MH_OBJECT "single object prelink". The MH_OBJECT approach allows us to apply
 * symbol hiding/aliasing/etc similar to that supported by dylibs, but because it is
 * seemingly unused within Apple, the use thereof regularly introduces linking bugs
 * and errors in new Xcode releases.
 *
 * Rather than fighting the linker, we use the namespacing machinery to rewrite these
 * symbols, but only when explicitly compiling PLCrashReporter. Since protobuf-c is a library
 * that may be used elsewhere, we don't want to rewrite these symbols if they're used
 * independently by PLCrashReporter API clients.
 */
#ifdef PLCR_PRIVATE
   /* If no prefix has been defined, we need to specify our own private prefix */
#  ifndef PLCRASHREPORTER_PREFIX
#    define PLCRASHREPORTER_PREFIX PL_
#  endif

#  define protobuf_c_buffer_simple_append                   PLNS(protobuf_c_buffer_simple_append)
#  define protobuf_c_default_allocator                      PLNS(protobuf_c_default_allocator)
#  define protobuf_c_enum_descriptor_get_value              PLNS(protobuf_c_enum_descriptor_get_value)
#  define protobuf_c_enum_descriptor_get_value_by_name      PLNS(protobuf_c_enum_descriptor_get_value_by_name)
#  define protobuf_c_message_descriptor_get_field           PLNS(protobuf_c_message_descriptor_get_field)
#  define protobuf_c_message_descriptor_get_field_by_name   PLNS(protobuf_c_message_descriptor_get_field_by_name)
#  define protobuf_c_message_free_unpacked                  PLNS(protobuf_c_message_free_unpacked)
#  define protobuf_c_message_get_packed_size                PLNS(protobuf_c_message_get_packed_size)
#  define protobuf_c_message_pack                           PLNS(protobuf_c_message_pack)
#  define protobuf_c_message_pack_to_buffer                 PLNS(protobuf_c_message_pack_to_buffer)
#  define protobuf_c_message_unpack                         PLNS(protobuf_c_message_unpack)
#  define protobuf_c_out_of_memory                          PLNS(protobuf_c_out_of_memory)
#  define protobuf_c_service_descriptor_get_method_by_name  PLNS(protobuf_c_service_descriptor_get_method_by_name)
#  define protobuf_c_service_destroy                        PLNS(protobuf_c_service_destroy)
#  define protobuf_c_service_generated_init                 PLNS(protobuf_c_service_generated_init)
#  define protobuf_c_system_allocator                       PLNS(protobuf_c_system_allocator)
#endif /* PLCR_PRIVATE */
