//
//  plcrash_exc_server.h
//  CrashReporter
//
//  Created by Landon Fuller on 12/18/12.
//
//

#ifndef CrashReporter_plcrash_exc_server_h
#define CrashReporter_plcrash_exc_server_h

#include <mach/ndr.h>
#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/notify.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/port.h>
#include <mach/exc.h>

#include <mach/std_types.h>
#include <mach/mig.h>
#include <mach/mach_types.h>

/* Routine exception_raise */
extern kern_return_t plcrash_exception_raise (
   mach_port_t exception_port,
   mach_port_t thread,
   mach_port_t task,
   exception_type_t exception,
   exception_data_t code, mach_msg_type_number_t codeCnt
);

/* Routine exception_raise_state */
extern kern_return_t plcrash_exception_raise_state (
   mach_port_t exception_port,
   exception_type_t exception,
   const exception_data_t code,
   mach_msg_type_number_t codeCnt,
   int *flavor,
   const thread_state_t old_state,
   mach_msg_type_number_t old_stateCnt,
   thread_state_t new_state,
   mach_msg_type_number_t *new_stateCnt
);

/* Routine exception_raise_state_identity */
extern kern_return_t plcrash_exception_raise_state_identity (
   mach_port_t exception_port,
   mach_port_t thread,
   mach_port_t task,
   exception_type_t exception,
   exception_data_t code,
   mach_msg_type_number_t codeCnt,
   int *flavor,
   thread_state_t old_state,
   mach_msg_type_number_t old_stateCnt,
   thread_state_t new_state,
   mach_msg_type_number_t *new_stateCnt
);

#endif
