/*
 * Author: Landon Fuller <landonf@plausiblelabs.com>
 *
 * Copyright (c) 2008 Plausible Labs Cooperative, Inc.
 * All rights reserved.
 */


/**
 * @defgroup plcrash_async_signal_info Signal Information
 * @ingroup plcrash_async
 *
 * Provides mapping of signal number and code to strings.
 *
 * @{
 */

const char *plcrash_async_signal_signame (int signal);
const char *plcrash_async_signal_sigcode (int signal, int si_code);

/**
 * @} plcrash_async_signal_info
 */