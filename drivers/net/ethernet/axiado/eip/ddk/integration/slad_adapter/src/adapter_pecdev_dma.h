/* adapter_pecdev_dma.h
 *
 * Interface to device-specific layer of the PEC implementation.
 */

/*****************************************************************************
* Copyright (c) 2011-2022 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef ADAPTER_PECDEV_DMA_H_
#define ADAPTER_PECDEV_DMA_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_adapter_pec.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool

// SLAD PEC API
#include "api_pec.h"

// SLAD DMABuf API
#include "api_dmabuf.h"

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
#include "adapter_interrupts.h"
#endif


/*----------------------------------------------------------------------------
 * adapter_pec_dev_capabilities_get
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
adapter_pec_dev_capabilities_get(
        pec_capabilities_t * const capabilities_p);


/*----------------------------------------------------------------------------
 * Adapter_PECDev_Control_Write
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
adapter_pec_dev_cd_control_write(
        pec_command_descriptor_t *command_p,
        const pec_packet_params_t *packet_params_p);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_rd_status_read
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
adapter_pec_dev_rd_status_read(
        const pec_result_descriptor_t * const result_p,
        pec_result_status_t * const result_status_p);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_init
 *
 * This function must be used to initialize the service. No API function may
 * be used before this function has returned.
 */
pec_status_t
adapter_pec_dev_init(
        const unsigned int interface_id,
        const pec_init_block_t * const init_block_p);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_uninit
 *
 * This call un-initializes the service. Use only when there are no pending
 * transforms. The caller must make sure that no API function is used while or
 * after this function executes.
 */
pec_status_t
adapter_pec_dev_uninit(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_resume
 *
 * This function must be used to resume the device after it was suspended.
 * This is a lightweight implementation of the adapter_pec_dev_init() function.
 */
int
adapter_pec_dev_resume(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_suspend
 *
 * This function must be used to suspend the device after it was resumed.
 * This is a lightweight implementation of the adapter_pec_dev_uninit() function.
 */
int
adapter_pec_dev_suspend(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_sa_prepare
 *
 * This function performs device-specific preparation of the SA record(s).
 *
 * At this point the SA Record is stored in memory in the byte order
 * required by the hardware, which may be different from the host byte order.
 * Hence any accesses must be done with dma_resource_read32() and
 * dma_resource_write32().
 *
 * Note: The resource is not owned by the hardware yet.
 *
 * sa_handle (input)
 *   DMA handle representing the main SA Record.
 *
 * state_handle (input)
 *   DMA handle representing the state Record (null handle if not in use)
 *
 * arc4_handle (input)
 *   DMA handle representing the arc4 state Record (null handle if not in use)
 *
 * Return code
 *   See pec_status_t
 */
pec_status_t
adapter_pec_dev_sa_prepare(
        const dma_buf_handle_t sa_handle,
        const dma_buf_handle_t state_handle,
        const dma_buf_handle_t arc4_handle);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_sa_remove
 *
 * This function performs device-specific removal of the SA record(s).
 *
 * sa_handle (input)
 *   DMA handle representing the main SA Record.
 *
 * state_handle (input)
 *   DMA handle representing the state Record (null handle if not in use)
 *
 * arc4_handle (input)
 *   DMA handle representing the arc4 state Record (null handle if not in use)
 *
 * At this point the SA record is stored in memory in the byte order
 * required by the hardware, which may be different from the host byte order.
 * Hence any accesses must be done with dma_resource_read32() and
 * dma_resource_read32().
 *
 * Note: The record is not owned by the hardware anymore.
 *
 * Return code
 *   See pec_status_t
 */
pec_status_t
adapter_pec_dev_sa_remove(
        const dma_buf_handle_t sa_handle,
        const dma_buf_handle_t state_handle,
        const dma_buf_handle_t arc4_handle);

/*----------------------------------------------------------------------------
 * adapter_pec_dev_inline_rd_put
 *
 * Submit Result Descriptor to Result Descriptor Ring. Result Descriptor
 * should have valid-empty-buffer-pointer. This memory buffer could be
 * allocated using dma_buf_alloc(). The buffer should have sufficient size
 * for storing processed data. Processed data would be stored in the buffer
 * by EIP-HW.
 *
 * interface_id (input)
 *     Represents the ring to which packets are submitted.
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
 *
 * Only the dst_pkt_byte_count, status1 and status2 fields will be filled in for
 * each result descriptor. All DMA resources are still owned by the hardware.
 */
pec_status_t adapter_pec_dev_inline_rd_put(
	const unsigned int interface_id,
	pec_result_descriptor_t *results_p,
	const unsigned int results_limit,
	unsigned int *const get_count_p);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_get_free_space
 *
 * Return the number of free slots in the indicated ring. When
 * Adapter_PECDev_PacketPut is called with no more than that many
 * valid command descriptors and none of these require scatter/gather,
 * the operation is guaranteed to succeed, unless a fatal error
 * happens in the meantime.
 */
unsigned int
adapter_pec_dev_get_free_space(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * Adapter_PECDev_PacketPut
 *
 * Submit one or more non-scatter-gather packets to the device.
 *
 * interface_id (input)
 *     Represents the ring to which packets are submitted.
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
 *
 * If bounce buffers were required for any of the DMA resources, the
 * supplied handles contain the BounceHandle field in their DMA
 * resource records and the data has been copied to bounce
 * buffers. The packet token is stored in the DMA buffer in the
 * required byte order (this is the case for all DMA resources). All
 * DMA resources are owned by the hardware.
 */
pec_status_t
adapter_pec_dev_packet_put(
        const unsigned int interface_id,
        const pec_command_descriptor_t * commands_p,
        const unsigned int commands_count,
        unsigned int * const put_count_p);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_packet_get
 *
 * Retrieve zero or more non-scatter-gather packets from the device.
 *
 * interface_id (input)
 *     Represents the ring to which packets are submitted.
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
 *
 * Only the dst_pkt_byte_count, status1 and status2 fields will be filled in for
 * each result descriptor. All DMA resources are still owned by the hardware.
 */
pec_status_t
adapter_pec_dev_packet_get(
        const unsigned int interface_id,
        pec_result_descriptor_t * results_p,
        const unsigned int results_limit,
        unsigned int * const get_count_p);


#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
/*----------------------------------------------------------------------------
 * adapter_pec_dev_test_sg
 *
 * Test whether a single packet with the indicated number of gather
 * particles and scatter particles can be processed successfully on
 * the indicated ring.
 *
 * If gather_particle_count is zero, the packet does not use Gather.
 * If scatter_particle_count is zero, the packet does not use Scatter.
 */
bool
adapter_pec_dev_test_sg(
        const unsigned int interface_id,
        const unsigned int gather_particle_count,
        const unsigned int scatter_particle_count);
#endif // ADAPTER_PEC_ENABLE_SCATTERGATHER


#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
/*----------------------------------------------------------------------------
 * Adapter_PECDev_IRQToInteraceID
 *
 * Convert the interrupt number to the Interface ID for which this
 * interrupt was generated.
 */
unsigned int
adapter_pec_dev_irq_to_inferface_id(
        const int n_irq);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_enable_result_irq
 *
 * Enable the interrupt or interrupts for available result packets on the
 * specified interface.
 */
void
adapter_pec_dev_enable_result_irq(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_disable_result_irq
 *
 * Disable the interrupt or interrupts for available result packets on the
 * specified interface.
 */
void
adapter_pec_dev_disable_result_irq(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_enable_command_irq
 *
 * Enable the interrupt or interrupts that indicate that the specified
 * interface can accept commands.
 */
void
adapter_pec_dev_enable_command_irq(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_disable_command_irq
 *
 * Disable the interrupt or interrupts that indicate that the specified
 * interface can accept commands.
 */
void
adapter_pec_dev_disable_command_irq(
        const unsigned int interface_id);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_set_result_handler
 *
 * Set a handler function for the result interrupt on the indicated
 * interface.
 */
void adapter_pec_dev_set_result_handler(
    const unsigned int interface_id,
    adapter_interrupt_handler_t handler_function);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_set_command_handler
 *
 * Set a handler function for the command interrupt on the indicated
 * interface.
 */
void adapter_pec_dev_set_command_handler(
        const unsigned int interface_id,
        adapter_interrupt_handler_t handler_function);
#endif // ADAPTER_PEC_INTERRUPTS_ENABLE


/*----------------------------------------------------------------------------
 * adapter_pec_dev_scatter_preload
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
 */
pec_status_t
adapter_pec_dev_scatter_preload(
        const unsigned int interface_id,
        dma_buf_handle_t * handles_p,
        const unsigned int handles_count);

/*----------------------------------------------------------------------------
 * adapter_pec_dev_put_dump
 *
 * Dump the administration data of the packet put operation.
 */
void
adapter_pec_dev_put_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_cdr_admin,
        const bool f_dump_cdr_cache);


/*----------------------------------------------------------------------------
 * adapter_pec_dev_get_dump
 *
 * Dump the administration data of the packet get operation.
 */
void
adapter_pec_dev_get_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_rdr_admin,
        const bool f_dump_rdr_cache);


#endif /* ADAPTER_PECDEV_DMA_H_ */


/* end of file adapter_pecdev_dma.h */
