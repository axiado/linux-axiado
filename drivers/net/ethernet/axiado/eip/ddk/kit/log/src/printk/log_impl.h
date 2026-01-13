/* log_impl.h
 *
 * Log Module, implementation for Linux Kernel Mode
 *
 * This version uses proper Linux kernel log levels (pr_crit, pr_warn, pr_info, pr_debug)
 * instead of plain printk for all messages.
 */

/*****************************************************************************
* Copyright (c) 2008-2020 by Rambus, Inc. and/or its subsidiaries.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#ifndef INCLUDE_GUARD_LOG_IMPL_H
#define INCLUDE_GUARD_LOG_IMPL_H

#include <linux/kernel.h>   // printk
#include <linux/printk.h>   // pr_* macros

/*
 * Define prefix for all log messages
 * Empty prefix, kernel already adds timestamp and module info
 * just add KBUILD_MODNAME to automatically set to the module name by the kernel build system
 * #define LOG_PREFIX KBUILD_MODNAME ": "
 */
#define LOG_PREFIX ""

/*
 * Map DDK logging to Linux kernel logging with proper log levels
 */
#define log_formatted_message_crit(fmt, ...)  pr_crit(LOG_PREFIX fmt, ##__VA_ARGS__)
#define log_formatted_message_warn(fmt, ...)  pr_warn(LOG_PREFIX fmt, ##__VA_ARGS__)
#define log_formatted_message_info(fmt, ...)  pr_info(LOG_PREFIX fmt, ##__VA_ARGS__)

/*
 * Debug logging - controlled by:
 * - CONFIG_DYNAMIC_DEBUG (runtime control via debugfs)
 * - DEBUG define (compile-time)
 */
#define Log_FormattedMessageDEBG(fmt, ...)    pr_debug(LOG_PREFIX fmt, ##__VA_ARGS__)

/*
 * Generic logging defaults to info level
 */
#define log_formatted_message(fmt, ...)       pr_info(LOG_PREFIX fmt, ##__VA_ARGS__)
#define log_message(msg)                      pr_info(LOG_PREFIX "%s", msg)

#endif /* Include Guard */

/* end of file log_impl.h */
