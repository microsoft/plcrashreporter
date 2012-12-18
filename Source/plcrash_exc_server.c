//
//  plcrash_exc_server.c
//  CrashReporter
//
//  Created by Landon Fuller on 12/18/12.
//
//

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
#include <mach/mig.h>
#include <mach/mach_types.h>

#define MIG_RETURN_ERROR(X, code)	{\
    ((mig_reply_error_t *)X)->RetCode = code;\
    ((mig_reply_error_t *)X)->NDR = NDR_record;\
    return;\
}

extern kern_return_t catch_mach_exception_raise (mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception,
                                                 integer_t code[2], mach_msg_type_number_t codeCnt);

extern kern_return_t catch_mach_exception_raise_state (mach_port_t exception_port, exception_type_t exception,
                                                       const integer_t code[2], mach_msg_type_number_t codeCnt,
                                                       int *flavor, const thread_state_t old_state,
                                                       mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
                                                       mach_msg_type_number_t *new_stateCnt);

extern kern_return_t catch_mach_exception_raise_state_identity (mach_port_t exception_port, mach_port_t thread, mach_port_t task,
                                                                exception_type_t exception, mach_exception_data_t code,
                                                                mach_msg_type_number_t codeCnt, int *flavor, thread_state_t old_state,
                                                                mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
                                                                mach_msg_type_number_t *new_stateCnt);

static mig_routine_t mach_exc_server_routine (mach_msg_header_t *InHeadP);

static void _Xmach_exception_raise (mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);
static void _Xmach_exception_raise_state (mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);
static void _Xmach_exception_raise_state_identity (mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP);

/* Description of this subsystem, for use in direct RPC */
const struct catch_mach_exc_subsystem {
	mig_server_routine_t 	server;	/* Server routine */
	mach_msg_id_t	start;	/* Min routine number */
	mach_msg_id_t	end;	/* Max routine number + 1 */
	unsigned int	maxsize;	/* Max msg size */
	vm_address_t	reserved;	/* Reserved */
	struct routine_descriptor	/*Array of routine descriptors */
    routine[3];
} catch_mach_exc_subsystem = {
	mach_exc_server_routine,
	2405,
	2408,
	(mach_msg_size_t)sizeof(union __RequestUnion__exc_subsystem),
	(vm_address_t)0,
	{
        { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_exception_raise, 6, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__exception_raise_t)},
        { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_exception_raise_state, 9, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__exception_raise_state_t)},
        { (mig_impl_routine_t) 0,
            (mig_stub_routine_t) _Xmach_exception_raise_state_identity, 11, 0, (routine_arg_descriptor_t)0, (mach_msg_size_t)sizeof(__Reply__exception_raise_state_identity_t)},
	}
};

boolean_t plcrash_exc_server (mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP) {
	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */
    
	register mig_routine_t routine;
    
	OutHeadP->msgh_bits = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(InHeadP->msgh_bits), 0);
	OutHeadP->msgh_remote_port = InHeadP->msgh_remote_port;
	/* Minimal size: routine() will update it if different */
	OutHeadP->msgh_size = (mach_msg_size_t)sizeof(mig_reply_error_t);
	OutHeadP->msgh_local_port = MACH_PORT_NULL;
	OutHeadP->msgh_id = InHeadP->msgh_id + 100;
    
	if ((InHeadP->msgh_id > 2407) || (InHeadP->msgh_id < 2405) ||
	    ((routine = catch_mach_exc_subsystem.routine[InHeadP->msgh_id - 2405].stub_routine) == 0)) {
		((mig_reply_error_t *)OutHeadP)->NDR = NDR_record;
		((mig_reply_error_t *)OutHeadP)->RetCode = MIG_BAD_ID;
		return FALSE;
	}
	(*routine) (InHeadP, OutHeadP);
	return TRUE;
}

static mig_routine_t mach_exc_server_routine (mach_msg_header_t *InHeadP) {
	register int msgh_id;
    
	msgh_id = InHeadP->msgh_id - 2405;
    
	if ((msgh_id > 2) || (msgh_id < 0))
		return 0;
    
	return catch_mach_exc_subsystem.routine[msgh_id].stub_routine;
}

static kern_return_t __MIG_check__Request__mach_exception_raise_state_t(__attribute__((__unused__)) __Request__exception_raise_state_t *In0P, __attribute__((__unused__)) __Request__exception_raise_state_t **In1PP)
{
    
	typedef __Request__exception_raise_state_t __Request;
	__Request *In1P;
	unsigned int msgh_size;
	unsigned int msgh_size_delta;
    
	msgh_size = In0P->Head.msgh_size;
	if ((In0P->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
	    (msgh_size < (mach_msg_size_t)(sizeof(__Request) - 912)) ||  (msgh_size > (mach_msg_size_t)sizeof(__Request)))
		return MIG_BAD_ARGUMENTS;
    
	if (In0P->NDR.int_rep != NDR_record.int_rep)
		__NDR_convert__int_rep__Request__mach_exception_raise_state_t__codeCnt(&In0P->codeCnt, In0P->NDR.int_rep);
	msgh_size_delta = (8 * In0P->codeCnt);

	if ( In0P->codeCnt > 2 )
		return MIG_BAD_ARGUMENTS;
	if (((msgh_size - (mach_msg_size_t)(sizeof(__Request) - 912)) / 8 < In0P->codeCnt) ||
	    (msgh_size < (mach_msg_size_t)(sizeof(__Request) - 912) + (8 * In0P->codeCnt)))
		return MIG_BAD_ARGUMENTS;
	msgh_size -= msgh_size_delta;
    
	*In1PP = In1P = (__Request *) ((pointer_t) In0P + msgh_size_delta - 16);
    
	if (In0P->NDR.int_rep != NDR_record.int_rep)
		__NDR_convert__int_rep__Request__mach_exception_raise_state_t__old_stateCnt(&In1P->old_stateCnt, In1P->NDR.int_rep);

	if ( In1P->old_stateCnt > 224 )
		return MIG_BAD_ARGUMENTS;
	if (((msgh_size - (mach_msg_size_t)(sizeof(__Request) - 912)) / 4 < In1P->old_stateCnt) ||
	    (msgh_size != (mach_msg_size_t)(sizeof(__Request) - 912) + (4 * In1P->old_stateCnt)))
		return MIG_BAD_ARGUMENTS;
    
	return MACH_MSG_SUCCESS;
}

/* Routine mach_exception_raise */
static void _Xmach_exception_raise (mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP) {
	typedef __Request__exception_raise_t Request;
	typedef __Reply__exception_raise_t Reply;
    
	Request *In0P = (Request *) InHeadP;
	Reply *OutP = (Reply *) OutHeadP;
	kern_return_t check_result;
    
	check_result = __MIG_check__Request__mach_exception_raise_t((Request *)In0P);
	if (check_result != MACH_MSG_SUCCESS)
        { MIG_RETURN_ERROR(OutP, check_result); }

	OutP->RetCode = catch_mach_exception_raise(In0P->Head.msgh_local_port, In0P->thread.name, In0P->task.name, In0P->exception, In0P->code, In0P->codeCnt);
	OutP->NDR = NDR_record;
}

/* Routine mach_exception_raise_state */
static void _Xmach_exception_raise_state (mach_msg_header_t *InHeadP, mach_msg_header_t *OutHeadP) {
	typedef __Request__exception_raise_state_t Request;
	typedef __Reply__exception_raise_state_t Reply;

	/*
	 * typedef struct {
	 * 	mach_msg_header_t Head;
	 * 	NDR_record_t NDR;
	 * 	kern_return_t RetCode;
	 * } mig_reply_error_t;
	 */
    
	Request *In0P = (Request *) InHeadP;
	Request *In1P;
	Reply *OutP = (Reply *) OutHeadP;
	kern_return_t check_result;

	check_result = __MIG_check__Request__mach_exception_raise_state_t((Request *)In0P, (Request **)&In1P);
	if (check_result != MACH_MSG_SUCCESS)
    { MIG_RETURN_ERROR(OutP, check_result); }

	OutP->new_stateCnt = 224;
    
	OutP->RetCode = catch_mach_exception_raise_state(In0P->Head.msgh_local_port, In0P->exception, In0P->code, In0P->codeCnt, &In1P->flavor, In1P->old_state, In1P->old_stateCnt, OutP->new_state, &OutP->new_stateCnt);
	if (OutP->RetCode != KERN_SUCCESS) {
		MIG_RETURN_ERROR(OutP, OutP->RetCode);
	}
    
	OutP->NDR = NDR_record;
    
    
	OutP->flavor = In1P->flavor;
	OutP->Head.msgh_size = (mach_msg_size_t)(sizeof(Reply) - 896) + (((4 * OutP->new_stateCnt)));    
}