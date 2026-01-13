/* iotoken.h
 *
 * IO Token API
 *
 * Standard interface for the EIP-96 Crypto Engine.
 *
 * This interface specifies features common to all implementations
 * of the IOToken API and its extensions.
 *
 * All the functions in this API are re-entrant for different tokens.
 * All the functions in this API can be used concurrently for different tokens.
 *
 */

/*****************************************************************************
* Copyright (c) 2016-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef IOTOKEN_H_
#define IOTOKEN_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */



/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"                // u32
#include "api_dmabuf.h"

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// Physical address
typedef struct
{
    u32 lo; // Low 32 bits of the 64-bit address
    u32 hi; // High 32 bits of the 64-bit address, set to 0 if not used
} io_token_phys_addr_t;

// Standard EIP-96 Input Token header word options
typedef struct
{
    // true - Use Context (SA) buffer 64-bit addresses, otherwise 32-bit
    bool f64bit_sa;

    // true - there have been already packets processed for this SA
    bool f_reuse_sa;

} io_token_options_t;

// Standard EIP-96 Input Token descriptor
typedef struct
{
    // Input packet length (in bytes)
    unsigned int in_packet_byte_count;

    // Initialization value for the Token Header Word
    unsigned int tkn_hdr_word_init;

    io_token_options_t options;

    // Packet flow type
    unsigned int type;

    // Application-specific identifier that will be passed to Output Token
    unsigned int app_id;

    // Context record (SA) physical address
    io_token_phys_addr_t sa_phys_addr;

    // Length (in 32-bit words) of optional bypass data
    unsigned int bypass_data_word_count;

    // Input Token extension
    // Optional pointer to the token descriptor extension.
    // This must be a pointer to the data structure of type
    // io_token_input_dscr_ext_t. Set to NULL if not used.
    void * ext_p;

} io_token_input_dscr_t;

// Standard EIP-96 Output Token descriptor
typedef struct
{
    // Output packet length (in bytes)
    unsigned int out_packet_byte_count;

    // EIP-96 and EIP-202 packet processing error codes
    unsigned int error_code;

    // Bypass data length (in bytes)
    unsigned int bypass_data_byte_count;

    // true - Hash byte(s) is/are appended at the end of packet data
    bool f_hash_appended;

    // If f_hash_appended is true then hash_byte_count contains a number of
    // appended bytes
    unsigned int hash_byte_count;

    // true - Generic Bytes are appended at the end of packet data
    bool f_bytes_appended;

    // true - Checksum field is appended at the end of packet data
    bool f_checksum_appended;

    // true - Next Header field is appended at the end of packet data
    bool f_next_header_appended;

    // true - Length field is appended at the end of packet data
    bool f_length_appended;

    // Application-specific identifier that will be passed to Output Token
    unsigned int app_id;

    // Next Header result value retrieved from the IPsec trailer
    // Only for packets with IPsec padding
    unsigned int next_header;

    // number of detected (and removed) padding bytes
    // Only for Inbound processing PKCS#7, RTP, IPSec, TLS and SSL
    unsigned int pad_byte_count;

    // Output Token descriptor extension
    // Optional pointer to the Output Token descriptor extension where
    // the parsed data from the token will be stored.
    // This must be a pointer to the data structure of type
    // io_token_output_dscr_ext_t. Set to NULL if not used.
    void * ext_p;

} io_token_output_dscr_t;

// Note: IOTOKEN_IN_WORD_COUNT and IOTOKEN_OUT_WORD_COUNT are
//       implementation-specific parameters and hence must be defined by
//       the implementation instead of this generic IOToken interface.
//
//       The io_token_in_word_count_get() and io_token_out_word_count_get()
//       functions can also be used to obtain these values.
//
//       These values are constant per implementation and do not change
//       at run-time.


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * io_token_in_word_count_get
 *
 * This function returns the size (in 32-bit words) of the Input Token.
 *
 * Return value
 *     >0  : Input Token size (IOTOKEN_IN_WORD_COUNT)
 *
 */
unsigned int
io_token_in_word_count_get(void);


/*----------------------------------------------------------------------------
 * io_token_out_word_count_get
 *
 * This function returns the size (in 32-bit words) of the Output Token.
 *
 * Return value
 *     >0  : Output Token size (IOTOKEN_OUT_WORD_COUNT)
 *
 */
unsigned int
io_token_out_word_count_get(void);


/*----------------------------------------------------------------------------
 * io_token_create
 *
 * This function creates an Input Token.
 *
 * dscr_p (input)
 *     Pointer to the Input Token descriptor.
 *
 * data_p (output)
 *     Pointer to the memory location where the Input Token data will be stored.
 *     The size of this buffer must be IOTOKEN_IN_WORD_COUNT 32-bit words
 *     at least.
 *     Each word in this buffer will be written using values from token_p,
 *     not specified (reserved) fields will be set to 0.
 *
 * Return value
 *     <0  : error, token not created
 *      0  : reserved
 *     >0  : Success, number of created 32-bit Input Token words
 *
 */
int
io_token_create(
        const io_token_input_dscr_t * const dscr_p,
        u32 * data_p);

/*----------------------------------------------------------------------------
 * io_token_token_convert
 *
 * This function converts a command descriptor for LIP/IIP/IDT/LAB flow
 * to an equivalent command descriptor for a LAC flow, while generating
 * the EIP96 processing token required for that LAC flow.
 *
 * data_p (input, output)
 *     Pointer to packet token data, will be updated.
 *
 * packet_handle (input()
 *     DMABuf handle representing the packet. Can be a single buffer or
 *     a gather list.
 *
 * token_context_p (input)
 *     Pointer to the Token Context used by the Token Builder.
 *
 * token_buf_p (output)
 *     Buffer where the EIP96 processing token instructions will be stored.
 *
 * Return value:
 *    <0 : error.
 *     0 : No token created, but not required (original packet flow is LAC or
 *         INV-TR)
 *    >0 : Token successfully created, length of EIP96 processing token
 *         instructions in word.
 */
int
io_token_token_convert(
        u32 * const data_p,
        dma_buf_handle_t packet_handle,
        void * const token_context_p,
        u32 * const token_buf_p);


/*----------------------------------------------------------------------------
 * io_token_fixup
 *
 * Fix-up the packet after processing. Some fields cannot be inserted into
 * the packet by the hardware because the packet is longer than will fit
 * in the internal buffers and the part of the packet where the fields should
 * be inserted, is already streamed out. Those fields are appended to the
 * packet instead. This function inserts the appended data back into the packet.
 *
 * data_p (input, output)
 *     Result token data.
 *
 * packet_handle (input, output).
 *     DMABuf handle that represents the packet to fix up. Can be a scatter
 *     list.
 *
 * Return value:
 *    <0 : error.
 *     0 : No fixup performed (not required).
 *    >0 : Fixup performed.
 */
int
io_token_fixup(
        u32 * const data_p,
        const dma_buf_handle_t packet_handle);

/*----------------------------------------------------------------------------
 * io_token_sa_addr_update
 *
 * This function updates the SA physical address in
 * an already existing Input Token.
 *
 * sa_phys_addr_p (input)
 *     Pointer to the SA physical address.
 *
 * in_token_data_p (output)
 *     Pointer to the memory location where the Input Token data will be updated.
 *     The size of this buffer must be IOTOKEN_IN_WORD_COUNT 32-bit words
 *     at least.
 *
 * Return value
 *     <0  : error, token not updated
 *      0  : reserved
 *     >0  : Success, number of updated 32-bit token words
 *
 */
int
io_token_sa_addr_update(
        const io_token_phys_addr_t * const sa_phys_addr_p,
        u32 * in_token_data_p);


/*----------------------------------------------------------------------------
 * io_token_sa_reuse_update
 *
 * This function updates the SA reuse flag in
 * an already existing Input Token.
 *
 * f_reuse_sa (input)
 *     True - enable Context Reuse auto detect, otherwise disable.
 *
 * in_token_data_p (output)
 *     Pointer to the memory location where the Input Token data will be updated.
 *     The size of this buffer must be IOTOKEN_IN_WORD_COUNT 32-bit words
 *     at least.
 *
 * Return value
 *     <0  : error, token not updated
 *      0  : reserved
 *     >0  : Success, number of updated 32-bit token words
 *
 */
int
io_token_sa_reuse_update(
        const bool f_reuse_sa,
        u32 * in_token_data_p);


/*----------------------------------------------------------------------------
 * io_token_mark_set
 *
 * This function sets a mark in an already existing Input Token. This mark will
 * be passed from the Input Token to the Output Token where its presence can
 * be checked with the io_token_mark_check() function.
 *
 * The function implementation determines which field in the Input Token can
 * be used for this.
 *
 * in_token_data_p (output)
 *     Pointer to the memory location where the Input Token data will be updated.
 *     The size of this buffer must be IOTOKEN_IN_WORD_COUNT 32-bit words
 *     at least.
 *
 * Return value
 *     <0  : error, token not updated
 *      0  : reserved
 *     >0  : Success, number of updated 32-bit token words
 *
 */
int
io_token_mark_set(
        u32 * in_token_data_p);


/*----------------------------------------------------------------------------
 * IOToken_Mark_Offset_Get
 *
 * This function returns a word offset in the output token where the mark
 * is stored.
 *
 * Return value
 *     <0  : error
 *      0  : reserved
 *     >0  : Success, offset of the 32-bit word in the output token where
 *           the mark is stored
 *
 */
int
io_token_out_mark_offset_get(void);


/*----------------------------------------------------------------------------
 * io_token_parse
 *
 * This function parses an Output Token.
 *
 * data_p (input)
 *     Pointer to the memory location where the Output Token data will be stored.
 *     The size of this buffer must be IOTOKEN_OUT_WORD_COUNT 32-bit words
 *     at least.
 *
 * dscr_p (output)
 *     Pointer to the Output Token descriptor where the parsed data from
 *     the token will be stored.
 *     Fields in this descriptor will be written using values from data_p.
 *
 * Return value
 *     <0  : error, token not created
 *      0  : reserved
 *     >0  : Success, number of parsed 32-bit Output Token words
 *
 */
int
io_token_parse(
        const u32 * data_p,
        io_token_output_dscr_t * const dscr_p);


/*----------------------------------------------------------------------------
 * io_token_mark_check
 *
 * This function checks if the mark set by the io_token_mark_set() function
 * in the Input Token is also present in the corresponding Output Token.
 *
 * out_token_data_p (input)
 *     Pointer to the memory location where the Output Token data is stored
 *     where the mark must be checked.
 *     The size of this buffer must be IOTOKEN_OUT_WORD_COUNT 32-bit words
 *     at least.
 *
 * Return value
 *     <0  : error, token not checked
 *      0  : mark found in Output Token
 *     >0  : mark not found in Output Token,
 *           number of checked 32-bit token words
 */
int
io_token_mark_check(
        u32 * out_token_data_p);


/*----------------------------------------------------------------------------
 * io_token_packet_legth_get
 *
 * This function reads the packet size (in bytes) from
 * an already existing Output Token.
 *
 * data_p (input)
 *     Pointer to the memory location where the Output Token data is stored.
 *     The size of this buffer must be IOTOKEN_OUT_WORD_COUNT 32-bit words
 *     at least.
 *
 * pkt_byte_count_p (output)
 *     Pointer to the memory location where in packet size will be stored.
 *
 * Return value
 *     <0  : error, packet size not read
 *      0  : reserved
 *     >0  : Success, number of read 32-bit token words
 *
 */
int
io_token_packet_legth_get(
        const u32 * data_p,
        unsigned int * pkt_byte_count_p);


/*----------------------------------------------------------------------------
 * io_token_bypass_legth_get
 *
 * This function reads the bypass data length (in 32-bit words) from
 * an already existing Output Token.
 *
 * data_p (input)
 *     Pointer to the memory location where the Output Token data is stored.
 *     The size of this buffer must be IOTOKEN_OUT_WORD_COUNT 32-bit words
 *     at least.
 *
 * bd_byte_count_p (output)
 *     Pointer to the memory location where in bypass data size will be stored.
 *
 * Return value
 *     <0  : error, packet size not read
 *      0  : reserved
 *     >0  : Success, number of read 32-bit token words
 *
 */
int
io_token_bypass_legth_get(
        const u32 * data_p,
        unsigned int * bd_byte_count_p);


/*----------------------------------------------------------------------------
 * io_token_error_code_get
 *
 * This function reads the packet processing error codes if any from
 * an already existing Output Token.
 *
 * data_p (input)
 *     Pointer to the memory location where the Output Token data is stored.
 *     The size of this buffer must be IOTOKEN_OUT_WORD_COUNT 32-bit words
 *     at least.
 *
 * error_code_p (output)
 *     Pointer to the memory location where packet processing error code
 *     will be stored.
 *
 * Return value
 *     <0  : error, packet processing error code not read
 *      0  : reserved
 *     >0  : Success, number of read 32-bit token words
 *
 */
int
io_token_error_code_get(
        const u32 * data_p,
        unsigned int * error_code_p);


/* end of file iotoken.h */


#endif /* IOTOKEN_H_ */
