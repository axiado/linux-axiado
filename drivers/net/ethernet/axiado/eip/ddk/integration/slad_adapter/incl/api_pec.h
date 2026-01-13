/* api_pec.h
 *
 * Packet Engine Control API (PEC)
 *
 * This API can be used to perform transforms on security protocol packets
 * for a set of security network protocols like IPSec, MACSec, sRTP, SSL,
 * DTLS, etc.
 *
 * This API can supports both Look-Aside (LA) and Hybrid (HB) use cases.
 *
 * Please note that this is a generic API that can be used for many transform
 * engines. A separate document will detail the exact fields and parameters
 * required for the SA and Descriptors.
 */

/*****************************************************************************
* Copyright (c) 2007-2023 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_API_PEC_H
#define INCLUDE_GUARD_API_PEC_H

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"

// This API requires the use of the DMA Buffer Allocation API. All buffers
// are passed through this API as handles.
#include "api_dmabuf.h"         // dma_buf_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// error/status codes
typedef enum
{
    PEC_STATUS_OK = 0,
    PEC_STATUS_BUSY,            // device busy, try again later
    PEC_ERROR_BAD_PARAMETER,
    PEC_ERROR_BAD_HANDLE,
    PEC_ERROR_BAD_USE_ORDER,
    PEC_ERROR_INTERNAL,
    PEC_ERROR_NOT_IMPLEMENTED,
    PEC_NO_FREE_SLOTS

} pec_status_t;


/*----------------------------------------------------------------------------
 * pec_init_block_t
 *
 * This structure contains service initialization parameters that are passed
 * to the pec_init call.
 *
 * For forward-compatibility with future extensions, please zero-init the
 * data structure before setting the fields and providing it to pec_init.
 * Example: pec_init_block_t InitBlock = {0};
 */
typedef struct
{
    bool f_use_dynamic_sa;             // true = use variable-size SA format

    // the following fields are related to Scatter/Gather
    // not all engines support this
    unsigned int fixed_scatter_frag_size_in_bytes;

    bool f_continuous_scatter; // Enable Continous Scatter Mode.
} pec_init_block_t;


/*----------------------------------------------------------------------------
 * pec_command_descriptor_t
 *
 * This data structure describes a transform request that will be queued up
 * for processing by the transform engine. It refers to the input and output
 * data buffers, the SA buffer(s) and contains parameters that describe the
 * transform, like the length of the data.
 *
 * For operations that do not require output buffers such as hash operations
 * the src_pkt_handle and dst_pkt_handle parameters in the
 * pec_command_descriptor_t descriptor must be set equal by applications.
 *
 */
typedef struct
{
    // the pointer that will be returned in the related result descriptor
    void * user_p;

    // Optional EIP-96 Instruction Token buffer,
    // some packet engines do not use tokens,
    // dma_buf_null_handle can be assigned to this variable when not used
    dma_buf_handle_t token_handle;

    // EIP-96 Instruction Token size (in 32-bit words)
    unsigned int token_word_count;

    // data buffers
    dma_buf_handle_t src_pkt_handle;
    dma_buf_handle_t dst_pkt_handle;
    unsigned int src_pkt_byte_count;
    unsigned int bypass_word_count;

    // SA reference
    u32 sa_word_count;
    dma_buf_handle_t sa_handle1;
    dma_buf_handle_t sa_handle2;

    // Engine specific control fields
    // with Transform specific values
    u32 control1;
    u32 control2;

    // Input Token buffer with fixed size (determined by use case)
    // This buffer must be allocated before the descriptor can be passed
    // to pec_packet_put()
    u32 * input_token_p;

} pec_command_descriptor_t;

/*----------------------------------------------------------------------------
 * pec_result_descriptor_t
 *
 * This data structure contains the result of the transform request. The user
 * can match the result to a command using the user_p field.
 *
 * A number of parameters from the command descriptor are also returned, as a
 * courtesy service.
 */
typedef struct
{
    // the pointer that from the related command descriptor
    void * user_p;

    // data buffers
    dma_buf_handle_t src_pkt_handle;
    dma_buf_handle_t dst_pkt_handle;
    u32 dst_pkt_byte_count;
    void * dst_pkt_p;
    // number of scatter particles used (continous scatter).
    u32 num_particles;

    unsigned int bypass_word_count;

    // Engine specific status fields
    // with Transform specific values
    u32 status1;
    u32 status2;

    // Output Token buffer with fixed size (determined by use case)
    // This buffer must be allocated before the descriptor can be passed
    // to pec_packet_get()
    u32 * output_token_p;

} pec_result_descriptor_t;


// Per-packet flags
#define PEC_PKT_FLAG_INIT_ARC4     BIT_0
#define PEC_PKT_FLAG_HASH_FINAL    BIT_1


/*----------------------------------------------------------------------------
 * pec_packet_params_t
 *
 * This data structure contains the per-packet parameters in a
 * device-independent way.
 */
typedef struct
{
    // Bitwise or of zero or more PEC_PKT_FLAG_* values, 0 if none apply.
    u8 flags;

    // Next header byte (IPsec padding) or pad byte value.
    u8 pad_byte;

    // Blocksize to which the packet must be padded (must be power of 2).
    u16 pad_boundary;

    // offset of ESP header
    u8 offset;

    // Token Header Word.
    u32 token_header_word;

    // Hardware operation.
    u8 hw_services;

} pec_packet_params_t;

// error status bits.
#define PEC_PKT_ERROR_AUTH         BIT_0 // Authentication failure
#define PEC_PKT_ERROR_PAD          BIT_1 // Pad verify fail
#define PEC_PKT_ERROR_SEQNUM       BIT_2 // Sequence number check failure
#define PEC_PKT_ERROR_BADCMD       BIT_3 // Invalid command.
#define PEC_PKT_ERROR_BADALGO      BIT_4 // Invalid algorithm
#define PEC_PKT_ERROR_PROHIBITED   BIT_5 // Prohibited algorithm
#define PEC_PKT_ERROR_ZEROLENGTH   BIT_6 // zero packet length
#define PEC_PKT_ERROR_BADIP        BIT_7 // Invalid IP header
#define PEC_PKT_ERROR_SPI          BIT_8 // spi mismatch.
#define PEC_PKT_ERROR_CRYPTBLKSIZE BIT_9 // Block size error
#define PEC_PKT_ERROR_BADCOMBO     BIT_10 // Invalid combination of algorithms
#define PEC_PKT_ERROR_LENGTH       BIT_11 // Length error
#define PEC_PKT_ERROR_PROC         BIT_12 // Processing error
#define PEC_PKT_ERROR_INBOUNDLEN   BIT_13 // Bad Inbound PE length
#define PEC_PKT_ERROR_SYSBUS       BIT_14 // System bus error
#define PEC_PKT_ERROR_DESCRIPTOR   BIT_15 // command descriptor error
#define PEC_PKT_ERROR_HASHBLKSIZE  BIT_16 // Hash block size error
#define PEC_PKT_ERROR_TOKEN        BIT_17 // error in token
#define PEC_PKT_ERROR_BYPASS       BIT_18 // Too much bypass data
#define PEC_PKT_ERROR_HASHOVF      BIT_19 // Hash input overflow
#define PEC_PKT_ERROR_TTLHOP       BIT_20 // ttl/Hop limit underflow
#define PEC_PKT_ERROR_CHKSUM       BIT_21 // Checksum error
#define PEC_PKT_ERROR_TIMEOUT      BIT_22 // Packet engine timeout
#define PEC_PKT_ERROR_DSCR_OVF     BIT_25 // Result descriptor overflow
#define PEC_PKT_ERROR_BUF_OVF      BIT_26 // Result buffer overflow


/*----------------------------------------------------------------------------
 * pec_result_status_t
 *
 * This data structure contains the per-packet status/result in a
 * device-independent way.
 */
typedef struct
{
    // Bitwise or of PEC_PKT_ERROR_* values. zero for successful operation.
    u32 errors;

    // number of pad bytes detected by packet operation.
    u8 pad_byte_count;

    // Next header byte detected by inbound IPsec operation.
    u8 next_header;

    // Hybrid use case only: type of Service (IPv4) / Traffic class (IPv6)
    u8 TOS_TC;

    // IPv4 Hybrid use case only:
    // True if fragmentation is prohibited for the packet
    bool f_df;

    // IPv6 Hybrid use case only: Next Header field offset within packet header
    u16 next_header_offset;

    // Hybrid use case only: Header Processing Context reference
    u32 hdr_proc_ctx_ref;

} pec_result_status_t;

pec_status_t
pec_inline_packet_get(
        const unsigned int interface_id,
        pec_result_descriptor_t * results_p,
        const unsigned int results_limit,
        unsigned int * const get_count_p);

pec_status_t pec_inline_rd_put(
	const unsigned int interface_id,
	pec_result_descriptor_t *results_p,
	const unsigned int results_limit,
	unsigned int *const get_count_p);

/*----------------------------------------------------------------------------
 * pec_inline_dest_packet_finalize
 *
 *
 * dma_buf_dst_pkt_handle (destination packet handle)
 *     Destination packet handle that is used in preparing RD
 */

pec_status_t pec_inline_dest_packet_finalize(dma_buf_handle_t dma_buf_dst_pkt_handle);


/*----------------------------------------------------------------------------
 * pec_notify_function_t
 *
 * This type specifies the callback function prototype for the function
 * pec_command_notify_request and pec_result_notify_request.
 * The notification will occur only once.
 *
 * NOTE: The exact context in which the callback function is invoked and the
 *       allowed actions in that callback are implementation specific. The
 *       intention is that all API functions can be used, except pec_uninit.
 */
typedef void (* pec_notify_function_t)(void);

// Supported cryptographic algorithms and functions.
#define PEC_SUPPORTED_OCE      BIT_14  // Output classification engine.
#define PEC_SUPPORTED_FRC      BIT_17  // Flow record cache.
#define PEC_SUPPORTED_SM4_XTS  BIT_3
#define PEC_SUPPORTED_MASK1024 BIT_4  // Anti-replay mask size 1024
#define PEC_SUPPORTED_BC0      BIT_5  // External block cipher
#define PEC_SUPPORTED_SM4      BIT_6
#define PEC_SUPPORTED_SM3      BIT_7
#define PEC_SUPPORTED_CHACHA20 BIT_8
#define PEC_SUPPORTED_POLY1305 BIT_9
#define PEC_SUPPORTED_MASK256  BIT_10  // Anti-replay mask size 256
#define PEC_SUPPORTED_MASK384  BIT_11  // Anti-replay mask size 384
#define PEC_SUPPORTED_AES      BIT_12  // AES modes ECB, CBC, CTR
#define PEC_SUPPORTED_AES_FB   BIT_13  // CFB and OFB modes for AES
#define PEC_SUPPORTED_DES      BIT_15  // DES/3DES modes ECB, CBC
#define PEC_SUPPORTED_DES_FB   BIT_16  // CFB and OFB modes for DES/3DES
#define PEC_SUPPORTED_ARC4     (BIT_19|BIT_18)
#define PEC_SUPPORTED_AES_XTS  BIT_20
#define PEC_SUPPORTED_WIRELESS BIT_21  // Kasumi, SNOW and ZUC algorithms
#define PEC_SUPPORTED_MD5      BIT_22
#define PEC_SUPPORTED_SHA1     BIT_23
#define PEC_SUPPORTED_SHA2_256 BIT_25  // SHA2-224 and SHA2-256
#define PEC_SUPPORTED_SHA2_512 BIT_26  // SHA2-384 and SHA2-512
#define PEC_SUPPORTED_CBCMAC   BIT_27  // (X)CBC-MAC, 128-bit keys
#define PEC_SUPPORTED_CBCMAC256 BIT_29 // CBC-MAC, 192 and 256 bit keys
#define PEC_SUPPORTED_GHASH    BIT_30  // AES-GCM/GMAC
#define PEC_SUPPORTED_SHA3     BIT_31

/*----------------------------------------------------------------------------
 * pec_capabilities_t
 *
 * sz_text_description[]
 *     zero-terminated descriptive text of the available services.
 */
#define PEC_MAXLEN_TEXT  128

typedef struct
{
    char sz_text_description[PEC_MAXLEN_TEXT];
    unsigned int supported_funcs;
} pec_capabilities_t;


/*----------------------------------------------------------------------------
 * pec_capabilities_get
 *
 * This routine returns a structure that describes the capabilities of the
 * implementation. See description of pec_capabilities_t for details.
 *
 * capabilities_p
 *     Pointer to the capabilities structure to fill in.
 *
 * This function is re-entrant.
 */
pec_status_t
pec_capabilities_get(
        pec_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * pec_init
 *
 * This function must be used to initialize the service. No API function may
 * be used before this function has returned.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) to be initialized.
 *
 * init_block_p (input)
 *     Pointer to the initialization block data
 *
 */
pec_status_t
pec_init(
        const unsigned int interface_id,
        const pec_init_block_t * const init_block_p);


/*----------------------------------------------------------------------------
 * pec_uninit
 *
 * This call un-initializes the service. Use only when there are no pending
 * transforms. The caller must make sure that no API function is used while or
 * after this function executes.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) to be un-initialized.
 *
 */
pec_status_t
pec_uninit(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * pec_sa_register
 *
 * This function must be used to register an SA so it can be used for
 * transforms. The caller is responsible for filling in the SA fields
 * according to the specification of the engine, with the exception of any
 * fields that are designated to hold addresses of the other SA memory blocks.
 * The buffers are considered arrays of 32bit words in host-native byte order.
 *
 * When this call returns it is no longer allowed to access these SA memory
 * blocks directly.
 *
 * interface_id (input)
 *     Not used, reserved.
 *
 * sa_handle1 (input)
 *     handle for the main SA memory block. This memory block contains all the
 *     static material and is typically read completely for every transform
 *     but only selective parts are written back after the transform.
 *
 * sa_handle2 (input)
 * sa_handle3 (input)
 *     Handles for the optional second and third memory blocks. These are
 *     typically used to remember state information and are therefore
 *     required only for certain types of SA's. These blocks are typically
 *     read and written for every transform. Putting them in a separate memory
 *     blocks allows these to be put in more high performance memory.
 *     Provide zero for unused handles.
 *
 * The exact number of handles required depends on the transform engine and on
 * the SA parameters and is specified in a separate document. The exact size
 * of each memory block is known to the service through the handle.
 */
pec_status_t
pec_sa_register(
        const unsigned int interface_id,
        dma_buf_handle_t sa_handle1,
        dma_buf_handle_t sa_handle2,
        dma_buf_handle_t sa_handle3);


/*----------------------------------------------------------------------------
 * pec_sa_un_register
 *
 * This function must be used to remove an SA from the system, which means it
 * can no longer be used to perform transforms with the engine. When this
 * function returns the caller is allowed to access the SA memory blocks again.
 *
 * interface_id (input)
 *     Not used, reserved.
 *
 * sa_handle1 (input)
 * sa_handle2 (input)
 * sa_handle3 (input)
 *     The same handles as we provided in the pec_sa_register call.
 *     Provide zero for unused handles.
 */
pec_status_t
pec_sa_un_register(
        const unsigned int interface_id,
        dma_buf_handle_t sa_handle1,
        dma_buf_handle_t sa_handle2,
        dma_buf_handle_t sa_handle3);


/*----------------------------------------------------------------------------
 * pec_packet_put
 *
 * This function must be put transform requests into a queue. It is possible
 * to queue up just one request, or an array of requests (these do not have to
 * be related). In case the queue is full, none or only a few of the requests
 * might be queued up.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) where the packet(s)
 *     must be submitted to. See the SLAD PEC API Implementation Notes
 *     for the details on how this parameter is used.
 *
 * commands_p (input)
 *     Pointer to one (or an array of command descriptors, each describe one
 *     transform request.
 *
 * commands_count (input)
 *     The number of command descriptors pointed to by commands_p.
 *
 * put_count_p (output)
 *     This parameter is used to return the actual number of descriptors that
 *     was queued up for processing (0..commands_count).
 */
pec_status_t
pec_packet_put(
        const unsigned int interface_id,
        const pec_command_descriptor_t * commands_p,
        const unsigned int commands_count,
        unsigned int * const put_count_p);

pec_status_t
pec_inline_packet_put(
        const unsigned int interface_id,
        const pec_command_descriptor_t * commands_p,
        const unsigned int commands_count,
        unsigned int * const put_count_p);

/*----------------------------------------------------------------------------
 * PEC_GetCDSpace(..)
 * Unlocked version seems much better.. butt DDK doesn't
 * seem to have right primitives, neither this is implemented.
 * A direct call to DeviceRead32() seems better (TODO)
 */
unsigned int pec_get_free_space(
	const unsigned int interface_id);
/*----------------------------------------------------------------------------
 * pec_packet_get
 *
 * This function must be used to retrieve the results for the requested
 * transform operations. Every request generates one result, whether it is
 * successful or failed. The caller is able to retrieve one result or an
 * array of results.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) where the packet(s)
 *     must be retrieved from. See the SLAD PEC API Implementation Notes
 *     for the details on how this parameter is used.
 *
 * results_p (input)
 *     Pointer to the result descriptor, or array of result descriptors, that
 *     will be populated by the service based on completed transform requests.
 *
 * results_limit (input)
 *     The number of result descriptors available from results_p and onwards.
 *
 * get_count_p (output)
 *     The actual number of result descriptors that were populated is returned
 *     in this parameter.
 */
pec_status_t
pec_packet_get(
        const unsigned int interface_id,
        pec_result_descriptor_t * results_p,
        const unsigned int results_limit,
        unsigned int * const get_count_p);


/*----------------------------------------------------------------------------
 * pec_command_notify_request
 *
 * This routine can be used to request a one-time notification of space
 * available for new commands. It is typically used after PEC_PacketPut
 * returned fewer packets added that requested (could be zero) and the caller
 * does not want to poll for new results.
 *
 * Once the requested number of spaces are available, the implementation will
 * invoke the callback one time to notify the user of the available results.
 * The notification is then immediately disabled.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) for which notification
 *     must be generated.
 *
 * cb_func_p
 *     address of the callback function.
 *
 * commands_count
 *     The number of commands that must be possible to add with
 *     PEC_PacketPut before the notification function will be invoked.
 */
pec_status_t
pec_command_notify_request(
        const unsigned int interface_id,
        pec_notify_function_t cb_func_p,
        const unsigned int commands_count);


/*----------------------------------------------------------------------------
 * pec_result_notify_request
 *
 * This routine can be used to request a one-time notification of available
 * results. It is typically used after PEC_PacketGet returned zero results
 * and the caller does not want to poll for new results.
 *
 * Once the requested transforms have completed, the implementation will
 * invoke the callback one time to notify this fact.
 * The notification is then immediately disabled. It is possible that the
 * notification callback is invoked when fewer than the expected number of
 * transforms have completed (or even zero). In this case the application
 * must invoke pec_result_notify_request again to be notified of the completion
 * of the remaining expected transforms.
 *
 * Once the notification callback is invoked, the application must use
 * pec_packet_get to read all available results and then call
 * PEC_ResultNotifyRequest again, else the notification
 * callback may not be called again.
 *
 * It is permissible (and even recommended) that the callback function
 * calls pec_packet_get to retrieve all available results and then
 * PEC_ResultNotifyRequest to schedule the next invocation of the same
 * callback. In this case the main application must call
 * pec_result_notify_request exactly once after pec_init.
 *
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) for which notification
 *     must be generated.
 *
 * cb_func_p
 *     address of the callback function.
 *
 * results_count
 *     The requested number of results that should be available the next
 *     time pec_packet_get is called after the callback function has occured.
 *     This parameter should have a minimum value of 1 and its maximum value is
 *     implementation dependend.
 */
pec_status_t
pec_result_notify_request(
        const unsigned int interface_id,
        pec_notify_function_t cb_func_p,
        const unsigned int results_count);


/*----------------------------------------------------------------------------
 * pec_cd_control_write
 *
 * Write the control1 and control2 engine-specific fields in the
 * command Descriptor The other fields (such as src_pkt_byte_count and
 * bypass_word_count must have been filled in already.
 *
 * command_p (input, output)
 *     command descriptor whose control1 and control2 fields must be filled in.
 *
 * packet_params_p (input)
 *     Per-packet parameters.
 *
 * This function is not implemented for all engine types.
 */
pec_status_t
pec_cd_control_write(
        pec_command_descriptor_t * command_p,
        const pec_packet_params_t * const packet_params_p);


/*----------------------------------------------------------------------------
 * pec_rd_status_read
 *
 * Read the engine-specific status1 and status2 fields from a Result Descriptor
 * and convert them to an engine-independent format.
 *
 * result_p (input)
 *     Result descriptor.
 *
 * result_status_p (output)
 *     Engine-independent status information.
 *
 * Not all error conditions can occur on all engine types.
 */
pec_status_t
pec_rd_status_read(
        const pec_result_descriptor_t * const result_p,
        pec_result_status_t * const result_status_p);


/*----------------------------------------------------------------------------
 * pec_scatter_preload
 *
 * This function must be used to pre-load the service with scatter buffers
 * that will be consumed by transforms configured to use scatter buffers
 * instead of a provided destination buffer(s). This is required for certain
 * engines only - check the driver documentation.
 *
 * interface_id (input)
 *    Packet I/O interface (such as ring, for example) for which scatter buffers
 *    must be pre-loaded.
 *
 * handles_p (input)
 *     Pointer to an array of handles, each handle representing one scatter
 *     buffer.
 *
 * handles_count (input)
 *     The number of handles in the array pointed to by handles_p.
 *
 * accepted_count_p (output)
 *     Pointer to the output parameter that reports how many of the handles,
 *     from the start of the array, could accepted by the service due to
 *     storage capacity limit. value will be between 0 and handles_count.
 */
pec_status_t
pec_scatter_preload(
        const unsigned int interface_id,
        dma_buf_handle_t * handles_p,
        const unsigned int handles_count,
        unsigned int * const accepted_count_p);


/*----------------------------------------------------------------------------
 * pec_scatter_preload_notify_request
 *
 * This routine can be used to request a one-time notification of space
 * available in the scatter preload list. This is typically used when
 * pec_scatter_preload does not accept further buffers and the caller
 * does not want to keep making those calls.
 *
 * Once the requested number of scatter buffers have been consumed, the
 * implementation will invoke the callback one time to notify this fact.
 * The notification is then immediately disabled.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) for which notification
 *     must be generated.
 *
 * cb_func_p
 *     address of the callback function.
 *
 * consumed_count
 *     The number of scatter buffers consumed before invoking the callback.
 */
pec_status_t
pec_scatter_preload_notify_request(
        const unsigned int interface_id,
        pec_notify_function_t cb_func_p,
        const unsigned int consumed_count);


/*----------------------------------------------------------------------------
 * pec_put_dump
 *
 * Dump packet put operation administration and cache data.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) for which notification
 *     must be generated.
 *
 * first_slot_id (input)
 *     First slot in the input ring buffer to start dumping.
 *
 * last_slot_id (input)
 *     Last slot in the input ring buffer to stop dumping.
 *
 * f_dump_put_admin (input)
 *     true if the input ring administration data must be dumped.
 *
 * f_dump_put_cache (input)
 *     true if the input ring cache data must be dumped.
 *
 * Return value
 *     None
 */
void
pec_put_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_put_admin,
        const bool f_dump_put_cache);


/*----------------------------------------------------------------------------
 * pec_get_dump
 *
 * Dump packet get operation administration and cache data.
 *
 * interface_id (input)
 *     Packet I/O interface (such as ring, for example) for which notification
 *     must be generated.
 *
 * first_slot_id (input)
 *     First slot in the output ring buffer to start dumping.
 *
 * last_slot_id (input)
 *     Last slot in the output ring buffer to stop dumping.
 *
 * f_dump_get_admin (input)
 *     true if the output ring administration data must be dumped.
 *
 * f_dump_get_cache (input)
 *     true if the output ring cache data must be dumped.
 *
 * Return value
 *     None
 */
void
pec_get_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_get_admin,
        const bool f_dump_get_cache);


#endif /* Include Guard */


/* end of file api_pec.h */
