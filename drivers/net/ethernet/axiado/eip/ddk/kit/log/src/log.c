/* log.c
 *
 * Log implementation for specific environment
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

#define LOG_SEVERITY_MAX  LOG_SEVERITY_NO_OUTPUT

// Logging API
#include "log.h"            // the API to implement


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
void
log_hex_dump(
        const char * sz_prefix_p,
        const unsigned int print_offset,
        const u8 * buffer_p,
        const unsigned int byte_count)
{
    unsigned int i;

    for(i = 0; i < byte_count; i += 16)
    {
        unsigned int j, limit;

        // if we do not have enough data for a full line
        if (i + 16 > byte_count)
            limit = byte_count - i;
        else
            limit = 16;

        log_formatted_message("%s %08d:", sz_prefix_p, print_offset + i);

        for (j = 0; j < limit; j++)
            log_formatted_message(" %02X", buffer_p[i+j]);

        log_formatted_message("\n");
    } // for
}


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
void
log_hex_dump32(
        const char * sz_prefix_p,
        const unsigned int print_offset,
        const u32 * buffer_p,
        const unsigned int word32_count)
{
    unsigned int i;

    for(i = 0; i < word32_count; i += 4)
    {
        unsigned int j, limit;

        // if we do not have enough data for a full line
        if (i + 4 > word32_count)
            limit = word32_count - i;
        else
            limit = 4;

        log_formatted_message("%s %08d:", sz_prefix_p, print_offset + i*4);

        for (j = 0; j < limit; j++)
            log_formatted_message(" %08X", buffer_p[i+j]);

        log_formatted_message("\n");
    } // for
}

/* end of file log.c */
