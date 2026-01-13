/* log.h
 *
 * Logging API
 *
 * The service provided by this interface allows the caller to output trace
 * messages. The implementation can use whatever output channel is available
 * in a specific environment.
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

#ifndef INCLUDEGUARD_LOG_H
#define INCLUDEGUARD_LOG_H

// Driver Framework Basic Defs API
#include "basic_defs.h"

// Logging API
#include "log_impl.h"           // implementation specifics


/*----------------------------------------------------------------------------
 * LOG_SEVERITY_MAX
 *
 * This preprocessor symbol is used to control the definition of three macros
 * that can be used to selectively compile three classes of log messages:
 * Informational, Warnings and Critical. Define this symbol before including
 * this header file. When absent, full logging is assumed.
 */

#define LOG_SEVERITY_NO_OUTPUT  0
#define LOG_SEVERITY_CRIT       1
#define LOG_SEVERITY_CRITICAL   1
#define LOG_SEVERITY_WARN       2
#define LOG_SEVERITY_WARNING    2
#define LOG_SEVERITY_INFO       3
#define LOG_SEVERITY_DEBUG      4

#ifndef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX  LOG_SEVERITY_INFO
#endif

#if LOG_SEVERITY_MAX == LOG_SEVERITY_NO_OUTPUT
    #define IDENTIFIER_NOT_USED_LOG_OFF(_x) IDENTIFIER_NOT_USED(_x)
#else
    #define IDENTIFIER_NOT_USED_LOG_OFF(_x)
#endif


/*----------------------------------------------------------------------------
 * LOG_CRIT_ENABLED
 * LOG_WARN_ENABLED
 * LOG_INFO_ENABLED
 * LOG_DEBG_ENABLED
 *
 * This preprocessor symbols can be used to test if a specific class of log
 * message has been enabled by the LOG_SEVERITY_MAX selection.
 *
 * Example usage:
 *
 * #ifdef LOG_SEVERITY_INFO
 * // dump command descriptor details to log
 * #endif
 */

#if LOG_SEVERITY_MAX >= LOG_SEVERITY_CRITICAL
#define LOG_CRIT_ENABLED
#endif

#if LOG_SEVERITY_MAX >= LOG_SEVERITY_WARNING
#define LOG_WARN_ENABLED
#endif

#if LOG_SEVERITY_MAX >= LOG_SEVERITY_INFO
#define LOG_INFO_ENABLED
#endif

#if LOG_SEVERITY_MAX >= LOG_SEVERITY_DEBUG
#define LOG_DEBG_ENABLED
#endif

/*----------------------------------------------------------------------------
 * log_message
 *
 * This function adds a simple constant message to the log buffer.
 *
 * Message_p
 *     Pointer to the zero-terminated log message. The message must be
 *     complete and terminated with a newline character ("\n"). This avoids
 *     blending of partial messages.
 *
 * Return value
 *     None.
 */
#ifndef log_message
void
log_message(
        const char * sz_message_p);
#endif


/*----------------------------------------------------------------------------
 * log_hex_dump
 *
 * This function logs Hex Dump of a Buffer
 *
 * szPrefix
 *     Prefix to be printed on every row.
 *
 * print_offset
 *     offset value that is printed at the start of every row. Can be used
 *     when the byte printed are located at some offset in another buffer.
 *
 * buffer_p
 *     Pointer to the start of the array of bytes to hex dump.
 *
 * byte_count
 *     number of bytes to include in the hex dump from buffer_p.
 *
 * Return value
 *     None.
 */
#ifndef log_hex_dump
void
log_hex_dump(
        const char * sz_prefix_p,
        const unsigned int print_offset,
        const u8 * buffer_p,
        const unsigned int byte_count);
#endif


/*----------------------------------------------------------------------------
 * log_hex_dump32
 *
 * This function logs Hex Dump of an array of 32-bit words
 *
 * szPrefix
 *     Prefix to be printed on every row.
 *
 * print_offset
 *     offset value that is printed at the start of every row. Can be used
 *     when the byte printed are located at some offset in another buffer.
 *
 * buffer_p
 *     Pointer to the start of the array of 32-bit words to hex dump.
 *
 * word32_count
 *     number of 32-bit words to include in the hex dump from buffer_p.
 *
 * Return value
 *     None.
 */
#ifndef log_hex_dump32
void
log_hex_dump32(
        const char * sz_prefix_p,
        const unsigned int print_offset,
        const u32 * buffer_p,
        const unsigned int word32_count);
#endif


/*----------------------------------------------------------------------------
 * log_formatted_message
 *
 * This function allows a message to be composed and output using a format
 * specifier in line with printf. Caller should be restrictive in the format
 * options used since not all platforms support all exotic variants.
 *
 * sz_format_p
 *     Pointer to the zero-terminated format specifier string.
 *
 * ...
 *     Variable number of additional arguments. These will be processed
 *     according to the specifiers found in the format specifier string.
 *
 * Return value
 *     None.
 */
#ifndef log_formatted_message
void
log_formatted_message(
        const char * sz_format_p,
        ...);
#endif


/*----------------------------------------------------------------------------
 * LOG_CRIT
 * LOG_WARN
 * LOG_INFO
 * LOG_DEBG
 *
 * These three helper macros can be used to conditionally compile code that
 * outputs log messages and make the actual log line more compact.
 * Each macro is enabled when the class of messages is activated with the
 * LOG_SEVERITY_MAX setting.
 *
 * Example usage:
 *
 * LOG_INFO("MyFunc: selected mode %u (%s)\n", mode, Mode2Str[mode]);
 *
 * LOG_INFO(
 *      "MyFunc: "
 *      "selected mode %u (%s)\n",
 *      mode,
 *      Mode2Str[mode]);
 *
 * LOG_WARN("MyFunc: Unexpected return value %d\n", res);
 */

#undef LOG_CRIT

#ifdef LOG_CRIT_ENABLED
#define LOG_CRIT log_formatted_message_crit
#else
#define LOG_CRIT(...)
#endif

#undef LOG_WARN
#ifdef LOG_WARN_ENABLED
#define LOG_WARN log_formatted_message_warn
#else
#define LOG_WARN(...)
#endif

#undef LOG_INFO
#ifdef LOG_INFO_ENABLED
#define LOG_INFO log_formatted_message_info
#else
#define LOG_INFO(...)
#endif

#undef LOG_DEBG
#ifdef LOG_DEBG_ENABLED
#define LOG_DEBG log_formatted_message_debg
#else
#define LOG_DEBG(...)
#endif

#endif /* Include Guard */

/* end of file log.h */
