/* adapter_pec_dma.c
 *
 * Packet Engine Control (PEC) API Implementation
 * using DMA mode.
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
******************************************************************************/

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

#include "api_pec.h"            // PEC_* (the API we implement here)


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default Adapter PEC configuration
#include "c_adapter_pec.h"

// DMABuf API
#include "api_dmabuf.h"         // DMABuf_*

// Adapter DMABuf internal API
#include "adapter_dmabuf.h"

// Adapter PEC device API
#include "adapter_pecdev_dma.h" // Adapter_PECDev_*

// Adapter Locking internal API
#include "adapter_lock.h"       // Adapter_Lock_*

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
#include "api_pec_sg.h"         // PEC_SG_* (the API we implement here)
#endif

// Runtime power Management device Macros API
#include "rpm_device_macros.h"  // RPM_*

// Logging API
#include "log.h"

// Driver Framework DMAResource API
#include "dmares_types.h"       // dma_resource_handle_t
#include "dmares_mgmt.h"        // DMAResource management functions
#include "dmares_rw.h"          // DMAResource buffer access.
#include "dmares_addr.h"        // DMAResource addr translation functions.
#include "dmares_buf.h"         // DMAResource buffer allocations

// Standard IOToken API
#include "iotoken.h"

// Driver Framework C Run-Time Library API
#include "clib.h"               // memcpy, memset

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool, u32


#include <linux/net.h>
#ifndef ADAPTER_PE_MODE_DHM
/*----------------------------------------------------------------------------
 * Definitions and macros
 */

typedef struct
{
    void * user_p;
    dma_buf_handle_t src_pkt_handle;
    dma_buf_handle_t dst_pkt_handle;
    dma_buf_handle_t token_handle;
    unsigned int bypass_word_count;
} adapter_side_channel_record_t;


/* Side channel fifo
   - Normal operation: pec_packet_put adds a record containing several
                                      words for each packet.
                       pec_packet_get pops one record for each packet, fills
                                      fields into result descriptor.
   - Contiuous scatter mode: pec_scatter_preload adds a record
                                      with DestPkt_Handle only for each scatter
                                      buffer.
                             pec_packet_get pops a record for each scatter
                                      buffer used. Can do PostDMA for each
                                      scatter buffer, will not fill in fields
                                      in result descriptor.
*/
typedef struct
{
    int size;
    int read_index;
    int write_index;
    adapter_side_channel_record_t records[1 + ADAPTER_PEC_MAX_PACKETS +
                                        ADAPTER_PEC_MAX_LOGICDESCR];
} adapter_pec_side_channel_fifo_t;


/*----------------------------------------------------------------------------
 * Local variables
 */
static volatile bool pec_is_initialized[ADAPTER_PEC_DEVICE_COUNT];
static volatile bool pec_continuous_scatter[ADAPTER_PEC_DEVICE_COUNT];

// lock and critical section for pec_init/Uninit()
static ADAPTER_LOCK_DEFINE(AdapterPEC_InitLock);
static adapter_lock_cs_t adapter_pec_init_cs;

// Locks and critical sections for pec_packet_put()
static adapter_lock_t adapter_pec_put_lock[ADAPTER_PEC_DEVICE_COUNT];
static adapter_lock_cs_t adapter_pec_put_cs[ADAPTER_PEC_DEVICE_COUNT];

// Locks and critical sections for pec_packet_get()
static adapter_lock_t adapter_pec_get_lock[ADAPTER_PEC_DEVICE_COUNT];
static adapter_lock_cs_t adapter_pec_get_cs[ADAPTER_PEC_DEVICE_COUNT];

// Locks and critical sections for pec_scatter_preload()
static adapter_lock_t adapter_pec_preload_lock[ADAPTER_PEC_DEVICE_COUNT];
static adapter_lock_cs_t adapter_pec_preload_cs[ADAPTER_PEC_DEVICE_COUNT];


static adapter_pec_side_channel_fifo_t
adapter_side_channel_fifo[ADAPTER_PEC_DEVICE_COUNT];

static struct
{
    volatile pec_notify_function_t result_notify_cb_p;
    volatile unsigned int results_count;

    volatile pec_notify_function_t command_notify_cb_p;
    volatile unsigned int commands_count;

} pec_notify[ADAPTER_PEC_DEVICE_COUNT];


#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
/*----------------------------------------------------------------------------
 * adapter_pec_interrupt_handler_result_notify
 *
 * This function is the interrupt handler for the PEC interrupt
 * sources that indicate the arrival of a a result descriptor..There
 * may be several interrupt sources.
 *
 * This function is used to invoke the PEC result notification callback.
 */
static void
adapter_pec_interrupt_handler_result_notify(
        const int n_irq,
        const unsigned int flags)
{
    unsigned int interface_id = adapter_pec_dev_irq_to_inferface_id(n_irq);

    IDENTIFIER_NOT_USED(flags);

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
    {
        LOG_CRIT("adapter_pec_interrupt_handler_result_notify"
                 "interface_id out of range\n");
        return;
    }

    adapter_pec_dev_disable_result_irq(interface_id);

    LOG_INFO("adapter_pec_interrupt_handler_result_notify: Enter\n");

    if (pec_notify[interface_id].result_notify_cb_p != NULL)
    {
        pec_notify_function_t cb_func_p;

        // Keep the callback on stack to allow registration
        // of another result notify request from callback
        cb_func_p = pec_notify[interface_id].result_notify_cb_p;

        pec_notify[interface_id].result_notify_cb_p = NULL;
        pec_notify[interface_id].results_count = 0;

        LOG_INFO(
            "adapter_pec_interrupt_handler_result_notify: "
            "Invoking PEC result notify callback for interface %d\n",
            interface_id);

        cb_func_p();
    }
}


/*----------------------------------------------------------------------------
 * adapter_pec_interrupt_handler_command_notify
 *
 * This function is the interrupt handler for the PEC interrupt sources.that
 * indicate that there is again freee space for new command descriptors.
 *
 * This function is used to invoke the PEC command notification callback.
 */
static void
adapter_pec_interrupt_handler_command_notify(
        const int n_irq,
        const unsigned int flags)
{
    unsigned int interface_id = adapter_pec_dev_irq_to_inferface_id(n_irq);

    IDENTIFIER_NOT_USED(flags);

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
    {
        LOG_CRIT("adapter_pec_interrupt_handler_command_notify"
                 "interface_id out of range\n");
        return;
    }

    adapter_pec_dev_disable_command_irq(interface_id);

    LOG_INFO("adapter_pec_interrupt_handler_command_notify: Enter\n");

    if (pec_notify[interface_id].command_notify_cb_p != NULL)
    {
        pec_notify_function_t cb_func_p;

        // Keep the callback on stack to allow registration
        // of another command notify request from callback
        cb_func_p = pec_notify[interface_id].command_notify_cb_p;

        pec_notify[interface_id].command_notify_cb_p = NULL;
        pec_notify[interface_id].commands_count = 0;

        LOG_INFO(
            "adapter_pec_interrupt_handler_command_notify: "
            "Invoking PEC command notify callback interface=%d\n",
            interface_id);

        cb_func_p();
    }
}
#endif /* ADAPTER_PEC_INTERRUPTS_ENABLE */


/*----------------------------------------------------------------------------
 * adapter_make_command_notify_call_back
 */
static inline void
adapter_make_command_notify_call_back(unsigned int interface_id)
{
    unsigned int packet_slots_empty_count;

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return;

    if (pec_notify[interface_id].command_notify_cb_p != NULL)
    {
        packet_slots_empty_count = adapter_pec_dev_get_free_space(interface_id);

        if (pec_notify[interface_id].commands_count <= packet_slots_empty_count)
        {
            pec_notify_function_t cb_func_p;

            // Keep the callback on stack to allow registeration
            // of another result notify request from callback
            cb_func_p = pec_notify[interface_id].command_notify_cb_p;

            pec_notify[interface_id].command_notify_cb_p = NULL;
            pec_notify[interface_id].commands_count = 0;

            LOG_INFO(
                "pec_packet_get: "
                "Invoking command notify callback\n");

            cb_func_p();
        }
    }
}


/*----------------------------------------------------------------------------
 * Adapter_PECResgisterSA_BounceIfRequired
 *
 * Returns false in case of error.
 * Allocate a bounce buffer and copy the data in case this if required.
 */
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
static bool
adapter_pec_register_sa_bounce_if_required(
        dma_resource_handle_t *dma_handle_p)
{
    dma_resource_handle_t dma_handle = *dma_handle_p;
    dma_resource_record_t * rec_p;
    dma_resource_addr_pair_t bounce_host_addr;
    void * host_addr;
    int dmares;

    // skip null handles
    if (!dma_resource_is_valid_handle(dma_handle))
        return true;    // no error

    rec_p = dma_resource_handle2_record_ptr(dma_handle);


    // skip proper buffers
    if (!adapter_dma_resource_is_foreign_allocated(dma_handle))
    {
        rec_p->bounce.bounce_handle = NULL;
        return true;    // no error
    }

    {
        dma_resource_properties_t bounce_properties;

        // used as u32 array
        bounce_properties.alignment  = adapter_dma_resource_alignment_get();
        bounce_properties.bank       = ADAPTER_PEC_BANK_SA;
        bounce_properties.f_cached    = false;
        bounce_properties.size       = rec_p->props.size;

        host_addr = adapter_dma_resource_host_addr(dma_handle);

        dmares = dma_resource_alloc(
                     bounce_properties,
                     &bounce_host_addr,
                     &rec_p->bounce.bounce_handle);

        // bounce buffer handle is stored in the DMA Resource Record
        // of the original buffer, which links the two
        // this will be used when freeing the buffer
        // but also when the SA is referenced in packet put

        if (dmares != 0)
        {
            LOG_CRIT(
                "pec_sa_register: "
                "Failed to alloc bounce buffer (error %d)\n",
                dmares);
            return false;   // error!
        }
        LOG_INFO(
            "pec_sa_register: "
            "Bouncing SA: %p to %p\n",
            dma_handle,
            rec_p->bounce.bounce_handle);
#ifdef ADAPTER_PEC_ARMRING_ENABLE_SWAP
        dma_resource_swap_endianness_set(rec_p->bounce.bounce_handle, true);
#endif

    }

    // copy the data to the bounce buffer
    memcpy(
        bounce_host_addr.address_p,
        host_addr,
        rec_p->props.size);

    *dma_handle_p = rec_p->bounce.bounce_handle;
    return true;        // no error
}
#endif /* ADAPTER_PEC_REMOVE_BOUNCEBUFFERS */


/*----------------------------------------------------------------------------
 * adapter_fifo_put
 *
 * Put packet information into the side channel fifo
 */
static bool
adapter_fifo_put(adapter_pec_side_channel_fifo_t *fifo,
                 void *user_p,
                 dma_buf_handle_t src_pkt_handle,
                 dma_buf_handle_t dst_pkt_handle,
                 dma_buf_handle_t token_handle,
                 unsigned int bypass_word_count)
{
    int write_index = fifo->write_index;
    int read_index = fifo->read_index;
    if (write_index == read_index - 1 ||
        (read_index == 0 && write_index == fifo->size - 1))
    {
        LOG_CRIT("Side channel fifo full\n");
        return false;
    }
    fifo->records[write_index].user_p = user_p;
    fifo->records[write_index].src_pkt_handle = src_pkt_handle;
    fifo->records[write_index].dst_pkt_handle = dst_pkt_handle;

    fifo->records[write_index].token_handle = token_handle;
    if (!dma_buf_handle_is_same(&token_handle, &dma_buf_null_handle))
    {
        fifo->records[write_index].bypass_word_count = bypass_word_count;
    }

    write_index += 1;
    if (write_index == fifo->size)
        write_index = 0;
    fifo->write_index = write_index;
    return true;
}


/*----------------------------------------------------------------------------
 * adapter_fifo_get
 *
 * Get and remove the oldest entry from the side channel fifo.
 */
static bool
adapter_fifo_get(adapter_pec_side_channel_fifo_t *fifo,
                 void **user_p,
                 dma_buf_handle_t *src_pkt_handle_p,
                 dma_buf_handle_t *dst_pkt_handle_p,
                 dma_buf_handle_t *token_handle_p,
                 unsigned int *bypass_word_count_p,
		 int interface_id)//custom added last param for debug purpose only
{
    int write_index = fifo->write_index;
    int read_index = fifo->read_index;
    if (write_index == read_index)
    {
	/* since default-ring doesn't get much.. making it Debug for that case*/
	if (interface_id == 0)
            LOG_DEBG("%s(%d):Trying to read from empty fifo, Ring:%d\n", __func__, __LINE__, interface_id);
	else
            LOG_CRIT("%s(%d):Trying to read from empty fifo, Ring:%d\n", __func__, __LINE__, interface_id);
        return false;
    }
    if (user_p)
        *user_p = fifo->records[read_index].user_p;
    if (src_pkt_handle_p)
        *src_pkt_handle_p = fifo->records[read_index].src_pkt_handle;
    *dst_pkt_handle_p = fifo->records[read_index].dst_pkt_handle;

    if (token_handle_p)
        *token_handle_p = fifo->records[read_index].token_handle;
    if (token_handle_p != NULL &&
        !dma_buf_handle_is_same(token_handle_p, &dma_buf_null_handle) &&
        bypass_word_count_p != NULL)
        *bypass_word_count_p = fifo->records[read_index].bypass_word_count;

    read_index += 1;
    if (read_index == fifo->size)
        read_index = 0;
    fifo->read_index = read_index;
    return true;
}


/*----------------------------------------------------------------------------
 * adapter_fifo_withdraw
 *
 * Withdraw the most recently added record from the side channel fifo.
 */
static void
adapter_fifo_withdraw(
        adapter_pec_side_channel_fifo_t *fifo)
{
    int write_index = fifo->write_index;
    if (write_index == fifo->read_index)
    {
        LOG_CRIT("adapter_fifo_withdraw: fifo is empty\n");
    }
    if (write_index == 0)
        write_index = fifo->size - 1;
    else
        write_index -= 1;
    fifo->write_index = write_index;
}


/* adapter_packet_prepare
 *
 * In case of bounce buffers, allocate bounce buffers for the packet and
 * the packet token.
 * Copy source packet and token into the bounce buffers.
 * Perform PreDMA on all packet buffers (source, destination and token).
 */
static pec_status_t
adapter_packet_prepare(
        const unsigned int interface_id,
        const pec_command_descriptor_t *cmd_p)
{
    dma_resource_handle_t src_pkt_handle, dst_pkt_handle, token_handle;
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    unsigned int particle_count;
    unsigned int i;
    dma_buf_handle_t particle_handle;
    dma_resource_handle_t dma_res_handle;
    u8 * dummy_ptr;
    unsigned int particle_size;
#endif

    src_pkt_handle =
        adapter_dma_buf_handle2_dma_resource_handle(cmd_p->src_pkt_handle);
    dst_pkt_handle =
        adapter_dma_buf_handle2_dma_resource_handle(cmd_p->dst_pkt_handle);
    token_handle = adapter_dma_buf_handle2_dma_resource_handle(cmd_p->token_handle);

    if (!dma_resource_is_valid_handle(src_pkt_handle) &&
        !dma_resource_is_valid_handle(dst_pkt_handle))
        return PEC_STATUS_OK; // For record invalidation in the Record Cache
    else if (!dma_resource_is_valid_handle(src_pkt_handle) ||
             (!dma_resource_is_valid_handle(dst_pkt_handle) &&
              !pec_continuous_scatter[interface_id]))
    {
        LOG_CRIT("pec_packet_put: invalid source or destination handle\n");
        return PEC_ERROR_BAD_PARAMETER;
    }

    // Token handle
    if (dma_resource_is_valid_handle(token_handle))
    {
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
        dma_resource_record_t * rec_p =
            dma_resource_handle2_record_ptr(token_handle);
        if (adapter_dma_resource_is_foreign_allocated(token_handle))
        {
            // Bounce buffer required.
            dma_resource_addr_pair_t bounce_host_addr;
            void * host_addr;
            int dmares;
            dma_resource_properties_t bounce_properties;

            // used as u32 array
            bounce_properties.alignment  = adapter_dma_resource_alignment_get();
            bounce_properties.bank       = ADAPTER_PEC_BANK_TOKEN;
            bounce_properties.f_cached    = false;
            bounce_properties.size       = rec_p->props.size;

            host_addr = adapter_dma_resource_host_addr(token_handle);

            dmares = dma_resource_alloc(
                bounce_properties,
                &bounce_host_addr,
                &rec_p->bounce.bounce_handle);

            // bounce buffer handle is stored in the DMA Resource Record
            // of the original buffer, which links the two
            // this will be used when freeing the buffer
            // but also when obtaining the bus address.

            if (dmares != 0)
            {
                LOG_CRIT(
                    "pec_packet_put: "
                    "Failed to alloc bounce buffer (error %d)\n",
                dmares);
                return PEC_ERROR_INTERNAL;   // error!
            }

            LOG_INFO(
                "PEC_Packet_Putr: "
                "Bouncing Token: %p to %p\n",
                token_handle,
                rec_p->bounce.bounce_handle);

            // copy the data to the bounce buffer
            memcpy(
                bounce_host_addr.address_p,
                host_addr,
                rec_p->props.size);

            token_handle = rec_p->bounce.bounce_handle;
        }
        else
        {
            rec_p->bounce.bounce_handle = NULL;
        }
#endif // !ADAPTER_PEC_REMOVE_BOUNCEBUFFERS

#ifdef ADAPTER_PEC_ARMRING_ENABLE_SWAP
        // Convert token data to packet engine endianness format
        dma_resource_swap_endianness_set(token_handle, true);

        dma_resource_write32_array(
            token_handle,
            0,
            cmd_p->token_word_count,
            adapter_dma_resource_host_addr(token_handle));
#endif // ADAPTER_PEC_ARMRING_ENABLE_SWAP

        dma_resource_pre_dma(token_handle, 0, 0);
    }

    // source packet handle
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_get_capacity(cmd_p->src_pkt_handle, &particle_count);

    if (particle_count > 0)
    {
        for (i=0; i<particle_count; i++)
        {
            pec_sg_list_read(cmd_p->src_pkt_handle,
                            i,
                            &particle_handle,
                            &particle_size,
                            &dummy_ptr);
            dma_res_handle =
                adapter_dma_buf_handle2_dma_resource_handle(particle_handle);
            dma_resource_pre_dma(dma_res_handle, 0, 0);
        }
    }
    else
#endif
    { // Not a gather packet,
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
        dma_resource_record_t * rec_p =
            dma_resource_handle2_record_ptr(src_pkt_handle);
        dma_resource_record_t * dst_rec_p =
            dma_resource_handle2_record_ptr(dst_pkt_handle);
        if (adapter_dma_resource_is_foreign_allocated(src_pkt_handle) ||
            adapter_dma_resource_is_foreign_allocated(dst_pkt_handle))
        {
            // Bounce buffer required. Use a single bounce buffer for
            // both the source and the destination packet.
            dma_resource_addr_pair_t bounce_host_addr;
            void * host_addr;
            int dmares;
            dma_resource_properties_t bounce_properties;

            // used as u32 array
            bounce_properties.alignment  = adapter_dma_resource_alignment_get();
            bounce_properties.bank       = ADAPTER_PEC_BANK_PACKET;
            bounce_properties.f_cached    = false;
            bounce_properties.size       = MAX(rec_p->props.size,
                                              dst_rec_p->props.size);

            host_addr = adapter_dma_resource_host_addr(src_pkt_handle);

            dmares = dma_resource_alloc(
                bounce_properties,
                &bounce_host_addr,
                &rec_p->bounce.bounce_handle);

            // bounce buffer handle is stored in the DMA Resource Record
            // of the original buffer, which links the two
            // this will be used when freeing the buffer
            // but also when obtaining the bus address.

            if (dmares != 0)
            {
                LOG_CRIT(
                    "pec_packet_put: "
                    "Failed to alloc bounce buffer (error %d)\n",
                dmares);
                return PEC_ERROR_INTERNAL;   // error!
            }
            LOG_INFO(
                "PEC_Packet_Putr: "
                "Bouncing Packet: %p to %p\n",
                src_pkt_handle,
                rec_p->bounce.bounce_handle);


            // copy the data to the bounce buffer
            memcpy(
                bounce_host_addr.address_p,
                host_addr,
                rec_p->props.size);

            dst_pkt_handle = src_pkt_handle = rec_p->bounce.bounce_handle;

            dst_rec_p->bounce.bounce_handle = rec_p->bounce.bounce_handle;
        }
        else
        {
            rec_p->bounce.bounce_handle = NULL;
            dst_rec_p->bounce.bounce_handle = NULL;
        }
#endif
        dma_resource_pre_dma(src_pkt_handle, 0, 0);
    }
    // Destination packet handle, not for continuous scatter.
    if (pec_continuous_scatter[interface_id])
        return PEC_STATUS_OK;
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_get_capacity(cmd_p->dst_pkt_handle, &particle_count);

    if (particle_count > 0)
    {
        for (i=0; i<particle_count; i++)
        {
            pec_sg_list_read(cmd_p->dst_pkt_handle,
                            i,
                            &particle_handle,
                            &particle_size,
                            &dummy_ptr);
            dma_res_handle =
                adapter_dma_buf_handle2_dma_resource_handle(particle_handle);
            dma_resource_pre_dma(dma_res_handle, 0, 0);
        }
    }
    else
#endif
    if (src_pkt_handle != dst_pkt_handle)
    {
        // Only if source and destination are distinct.
        // When bounce buffers were used, these are not distinct.
        dma_resource_pre_dma(dst_pkt_handle, 0, 0);
    }
    return PEC_STATUS_OK;
}

/* adapter_packet_finalize
 *
 * Perform PostDMA on all DMA buffers (source, destination and token).
 * Copy the destination packet from the bounce buffer into the final location.
 * Deallocate any bounce buffers (packet and token).
 */
static pec_status_t
adapter_packet_finalize(
        dma_buf_handle_t dma_buf_src_pkt_handle,
        dma_buf_handle_t dma_buf_dst_pkt_handle,
        dma_buf_handle_t dma_buf_token_handle)
{
    dma_resource_handle_t src_pkt_handle, dst_pkt_handle, token_handle;

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    unsigned int particle_count;
    unsigned int i;
    dma_buf_handle_t particle_handle;
    dma_resource_handle_t dma_res_handle;
    u8 * dummy_ptr;
    unsigned int particle_size;
#endif

    src_pkt_handle =
        adapter_dma_buf_handle2_dma_resource_handle(dma_buf_src_pkt_handle);
    dst_pkt_handle =
        adapter_dma_buf_handle2_dma_resource_handle(dma_buf_dst_pkt_handle);

    if (!dma_resource_is_valid_handle(src_pkt_handle) &&
        !dma_resource_is_valid_handle(dst_pkt_handle))
        return PEC_STATUS_OK; // For record invalidation in the Record Cache

    token_handle = adapter_dma_buf_handle2_dma_resource_handle(dma_buf_token_handle);

    // Token handle.
    if (dma_resource_is_valid_handle(token_handle))
    {
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
        dma_resource_record_t * rec_p =
            dma_resource_handle2_record_ptr(token_handle);
        if (rec_p->bounce.bounce_handle != NULL)
        {
            // Post DMA and release the bounce buffer.
            dma_resource_post_dma(rec_p->bounce.bounce_handle, 0, 0);
            dma_resource_release(rec_p->bounce.bounce_handle);
        }
        else
#endif
        {
            dma_resource_post_dma(token_handle, 0, 0);
        }
    }
    // Destination packet handle

#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_get_capacity(dma_buf_dst_pkt_handle, &particle_count);

    if (particle_count > 0)
    {
        for (i=0; i<particle_count; i++)
        {
            pec_sg_list_read(dma_buf_dst_pkt_handle,
                            i,
                            &particle_handle,
                            &particle_size,
                            &dummy_ptr);
            dma_res_handle =
                adapter_dma_buf_handle2_dma_resource_handle(particle_handle);
            dma_resource_post_dma(dma_res_handle, 0, 0);
        }
    }
    else
#endif
    {
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
        dma_resource_record_t * rec_p =
            dma_resource_handle2_record_ptr(dst_pkt_handle);
        void * host_addr = adapter_dma_resource_host_addr(dst_pkt_handle);
        if (rec_p->bounce.bounce_handle != NULL)
        {
            void * bounce_host_addr =
                adapter_dma_resource_host_addr(rec_p->bounce.bounce_handle);
            // Post DMA, copy and release the bounce buffer.
            dma_resource_post_dma(rec_p->bounce.bounce_handle, 0, 0);

            memcpy( host_addr, bounce_host_addr, rec_p->props.size);

            dma_resource_release(rec_p->bounce.bounce_handle);
            src_pkt_handle = dst_pkt_handle;
        }
        else
#endif
        {
            dma_resource_post_dma(dst_pkt_handle, 0, 0);
        }

    }
    // source packet handle
#ifdef ADAPTER_PEC_ENABLE_SCATTERGATHER
    pec_sg_list_get_capacity(dma_buf_src_pkt_handle, &particle_count);

    if (particle_count > 0)
    {
        for (i=0; i<particle_count; i++)
        {
            pec_sg_list_read(dma_buf_src_pkt_handle,
                            i,
                            &particle_handle,
                            &particle_size,
                            &dummy_ptr);
            dma_res_handle =
                adapter_dma_buf_handle2_dma_resource_handle(particle_handle);
            dma_resource_post_dma(dma_res_handle, 0, 0);
        }
    }
    else
#endif
    if (src_pkt_handle != dst_pkt_handle)
    {
        // Only if source and destination are distinct.
        // When bounce buffers were used, these are not distinct.
        dma_resource_post_dma(src_pkt_handle, 0, 0);
    }

    return PEC_STATUS_OK;
}


#ifdef ADAPTER_PEC_RPM_EIP202_DEVICE0_ID
/*----------------------------------------------------------------------------
 * adapter_pec_resume
 */
static int
adapter_pec_resume(void * p)
{
    int interface_id = *(int *)p;

    if (interface_id < 0 || interface_id < ADAPTER_PEC_RPM_EIP202_DEVICE0_ID)
        return -3; // error

    interface_id -= ADAPTER_PEC_RPM_EIP202_DEVICE0_ID;

    return adapter_pec_dev_resume(interface_id);
}


/*----------------------------------------------------------------------------
 * adapter_pec_suspend
 */
static int
adapter_pec_suspend(void * p)
{
    int interface_id = *(int *)p;

    if (interface_id < 0 || interface_id < ADAPTER_PEC_RPM_EIP202_DEVICE0_ID)
        return -3; // error

    interface_id -= ADAPTER_PEC_RPM_EIP202_DEVICE0_ID;

    return adapter_pec_dev_suspend(interface_id);
}
#endif


/*----------------------------------------------------------------------------
 * pec_capabilities_get
 */
pec_status_t
pec_capabilities_get(
        pec_capabilities_t * const capabilities_p)
{
    return adapter_pec_dev_capabilities_get(capabilities_p);
}


/*----------------------------------------------------------------------------
 * pec_init
 */
pec_status_t
pec_init(
        const unsigned int interface_id,
        const pec_init_block_t * const init_block_p)
{
    LOG_INFO("\n\t pec_init \n");

    if (!init_block_p)
        return PEC_ERROR_BAD_PARAMETER;

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    adapter_lock_cs_set(&adapter_pec_init_cs, &AdapterPEC_InitLock);

    if (!adapter_lock_cs_enter(&adapter_pec_init_cs))
        return PEC_STATUS_BUSY;

    // ensure we init only once
    if (pec_is_initialized[interface_id])
    {
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_STATUS_OK;
    }
    pec_continuous_scatter[interface_id] = init_block_p->f_continuous_scatter;

    // Allocate the Put lock
    adapter_pec_put_lock[interface_id] = adapter_lock_alloc();
    if (adapter_pec_put_lock[interface_id] == NULL)
    {
        LOG_CRIT("pec_init: PutLock allocation failed\n");
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_ERROR_INTERNAL;
    }
    adapter_lock_cs_set(&adapter_pec_put_cs[interface_id],
                         adapter_pec_put_lock[interface_id]);

    // Allocate the Get lock
    adapter_pec_get_lock[interface_id] = adapter_lock_alloc();
    if (adapter_pec_get_lock[interface_id] == NULL)
    {
        LOG_CRIT("pec_init: GetLock allocation failed\n");
        adapter_lock_free(adapter_pec_put_lock[interface_id]);
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_ERROR_INTERNAL;
    }
    adapter_lock_cs_set(&adapter_pec_get_cs[interface_id],
                         adapter_pec_get_lock[interface_id]);

    if (init_block_p->f_continuous_scatter)
    {
        // Allocate the Preoload lock
        adapter_pec_preload_lock[interface_id] = adapter_lock_alloc();
        if (adapter_pec_get_lock[interface_id] == NULL)
        {
            LOG_CRIT("pec_init: GetLock allocation failed\n");
            adapter_lock_free(adapter_pec_put_lock[interface_id]);
            adapter_lock_free(adapter_pec_get_lock[interface_id]);
            adapter_lock_cs_leave(&adapter_pec_init_cs);
            return PEC_ERROR_INTERNAL;
        }
        adapter_lock_cs_set(&adapter_pec_preload_cs[interface_id],
                            adapter_pec_preload_lock[interface_id]);
    }

    zeroinit(pec_notify[interface_id]);

    if (RPM_DEVICE_INIT_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                    adapter_pec_suspend,
                                    adapter_pec_resume) != RPM_SUCCESS)
        return PEC_ERROR_INTERNAL;

    // Init the device
    if (adapter_pec_dev_init(interface_id, init_block_p) != PEC_STATUS_OK)
    {
        (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id);
        LOG_CRIT("pec_init: adapter_pec_dev_init failed\n");
        adapter_lock_free(adapter_pec_put_lock[interface_id]);
        adapter_lock_free(adapter_pec_get_lock[interface_id]);
        if (init_block_p->f_continuous_scatter)
        {
            adapter_lock_free(adapter_pec_preload_lock[interface_id]);
        }
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_ERROR_INTERNAL;
    }

    (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id);

    adapter_side_channel_fifo[interface_id].size =
        sizeof(adapter_side_channel_fifo[interface_id].records) /
        sizeof(adapter_side_channel_fifo[interface_id].records[0]);
    adapter_side_channel_fifo[interface_id].write_index = 0;
    adapter_side_channel_fifo[interface_id].read_index = 0;

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
    // enable the descriptor done interrupt
    LOG_INFO("pec_init: Registering interrupt handler\n");

    adapter_pec_dev_set_result_handler(
            interface_id,
            adapter_pec_interrupt_handler_result_notify);

    adapter_pec_dev_set_command_handler(
            interface_id,
            adapter_pec_interrupt_handler_command_notify);
#endif /* ADAPTER_PEC_INTERRUPTS_ENABLE */

    pec_is_initialized[interface_id] = true;

    LOG_INFO("\n\t pec_init done \n");

    adapter_lock_cs_leave(&adapter_pec_init_cs);

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * pec_uninit
 */
pec_status_t
pec_uninit(
        const unsigned int interface_id)
{
    LOG_INFO("\n\t pec_uninit \n");

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    adapter_lock_cs_set(&adapter_pec_init_cs, &AdapterPEC_InitLock);

    if (!adapter_lock_cs_enter(&adapter_pec_init_cs))
        return PEC_STATUS_BUSY;

    // ensure we uninit only once
    if (!pec_is_initialized[interface_id])
    {
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_STATUS_OK;
    }

    if (!adapter_lock_cs_enter(&adapter_pec_put_cs[interface_id]))
    {
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_STATUS_BUSY;
    }

    if (!adapter_lock_cs_enter(&adapter_pec_get_cs[interface_id]))
    {
        adapter_lock_cs_leave(&adapter_pec_put_cs[interface_id]);
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_STATUS_BUSY;
    }

    if (pec_continuous_scatter[interface_id])
    {
        if (!adapter_lock_cs_enter(&adapter_pec_preload_cs[interface_id]))
        {
            adapter_lock_cs_leave(&adapter_pec_put_cs[interface_id]);
            adapter_lock_cs_leave(&adapter_pec_get_cs[interface_id]);
            adapter_lock_cs_leave(&adapter_pec_init_cs);
            return PEC_STATUS_BUSY;
        }
    }

    if (RPM_DEVICE_UNINIT_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                true) != RPM_SUCCESS)
    {
        if (pec_continuous_scatter[interface_id])
        {
            adapter_lock_cs_leave(&adapter_pec_preload_cs[interface_id]);
        }
        adapter_lock_cs_leave(&adapter_pec_get_cs[interface_id]);
        adapter_lock_cs_leave(&adapter_pec_put_cs[interface_id]);
        adapter_lock_cs_leave(&adapter_pec_init_cs);
        return PEC_ERROR_INTERNAL;
    }

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
    adapter_pec_dev_disable_result_irq(interface_id);
    adapter_pec_dev_disable_command_irq(interface_id);
#endif

    adapter_pec_dev_uninit(interface_id);

    pec_is_initialized[interface_id] = false;

    (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id);

    adapter_lock_cs_leave(&adapter_pec_get_cs[interface_id]);
    adapter_lock_cs_leave(&adapter_pec_put_cs[interface_id]);

    // Free Preload lock
    if (pec_continuous_scatter[interface_id])
    {
        adapter_lock_cs_leave(&adapter_pec_preload_cs[interface_id]);
        adapter_lock_free(adapter_lock_cs_get(&adapter_pec_preload_cs[interface_id]));
        adapter_lock_cs_set(&adapter_pec_preload_cs[interface_id], adapter_lock_null);
    }

    // Free Get lock
    adapter_lock_free(adapter_lock_cs_get(&adapter_pec_get_cs[interface_id]));
    adapter_lock_cs_set(&adapter_pec_get_cs[interface_id], adapter_lock_null);

    // Free Put lock
    adapter_lock_free(adapter_lock_cs_get(&adapter_pec_put_cs[interface_id]));
    adapter_lock_cs_set(&adapter_pec_put_cs[interface_id], adapter_lock_null);

    LOG_INFO("\n\t pec_uninit done \n");

    adapter_lock_cs_leave(&adapter_pec_init_cs);

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * pec_sa_register
 */
pec_status_t
pec_sa_register(
        const unsigned int interface_id,
        dma_buf_handle_t sa_handle1,
        dma_buf_handle_t sa_handle2,
        dma_buf_handle_t sa_handle3)
{
    dma_resource_handle_t dma_handle1, dma_handle2, dma_handle3;
    pec_status_t res;

    LOG_INFO("\n\t pec_sa_register \n");

    IDENTIFIER_NOT_USED(interface_id);

    dma_handle1 = adapter_dma_buf_handle2_dma_resource_handle(sa_handle1);
    dma_handle2 = adapter_dma_buf_handle2_dma_resource_handle(sa_handle2);
    dma_handle3 = adapter_dma_buf_handle2_dma_resource_handle(sa_handle3);

    // The SA, state Record and arc4 state Record are arrays of u32.
    // The caller provides them in host-native format.
    // This function converts them to device-native format
    // using DMAResource and in-place operations.

    // Endianness conversion for the 1st SA memory block (Main SA Record)
#ifdef ADAPTER_PEC_ARMRING_ENABLE_SWAP
    {
        dma_resource_record_t * const rec_p =
            dma_resource_handle2_record_ptr(dma_handle1);

        if (rec_p == NULL)
            return PEC_ERROR_INTERNAL;

        dma_resource_swap_endianness_set(dma_handle1, true);

        dma_resource_write32_array(
            dma_handle1,
            0,
            rec_p->props.size / 4,
            adapter_dma_resource_host_addr(dma_handle1));
    }

    // Endianness conversion for the 2nd SA memory block (state Record)
    if (dma_handle2 != NULL)
    {
        dma_resource_record_t * const rec_p =
            dma_resource_handle2_record_ptr(dma_handle2);

        if (rec_p == NULL)
            return PEC_ERROR_INTERNAL;

        // The 2nd SA memory block can never be a subset of
        // the 1st SA memory block so it is safe to perform
        // the endianness conversion
        dma_resource_swap_endianness_set(dma_handle2, true);

        dma_resource_write32_array(
            dma_handle2,
            0,
            rec_p->props.size / 4,
            adapter_dma_resource_host_addr(dma_handle2));
    }

    // Endianness conversion for the 3d SA memory block (arc4 state Record)
    if (dma_handle3 != NULL)
    {
        dma_resource_record_t * const rec_p =
            dma_resource_handle2_record_ptr(dma_handle3);

        if (rec_p == NULL)
            return PEC_ERROR_INTERNAL;

        // The 3d SA memory block can never be a subset of
        // the 2nd SA memory block.

        // Check if the 3d SA memory block is not a subset of the 1st one
        if (!adapter_dma_resource_is_sub_range_of(dma_handle3, dma_handle1))
        {
            // The 3d SA memory block is a separate buffer and does not
            // overlap with the 1st SA memory block,
            // so the endianness conversion must be done
            dma_resource_swap_endianness_set(dma_handle3, true);

            dma_resource_write32_array(
                    dma_handle3,
                    0,
                    rec_p->props.size / 4,
                    adapter_dma_resource_host_addr(dma_handle3));
        }
    }
#endif // ADAPTER_PEC_ARMRING_ENABLE_SWAP

#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
    // Bounce the SA buffers if required
    // Check if the 3d SA memory block is not a subset of the 1st one
    if (dma_handle3 != NULL &&
        !adapter_dma_resource_is_sub_range_of(dma_handle3, dma_handle1))
    {
        if (!adapter_pec_register_sa_bounce_if_required(&dma_handle3))
            return PEC_ERROR_INTERNAL;
    }

    if (!adapter_pec_register_sa_bounce_if_required(&dma_handle1))
        return PEC_ERROR_INTERNAL;

    if (!adapter_pec_register_sa_bounce_if_required(&dma_handle2))
        return PEC_ERROR_INTERNAL;
#endif

    res = adapter_pec_dev_sa_prepare(sa_handle1, sa_handle2, sa_handle3);
    if (res != PEC_STATUS_OK)
    {
        LOG_WARN(
            "pec_sa_register: "
            "Adapter_PECDev_PrepareSA returned %d\n",
            res);
        return PEC_ERROR_INTERNAL;
    }

    // now use DMAResource to ensure the engine
    // can read the memory blocks using DMA
    dma_resource_pre_dma(dma_handle1, 0, 0);     // 0,0 = "entire buffer"

    if (dma_handle2 != NULL)
        dma_resource_pre_dma(dma_handle2, 0, 0);

    // Check if the 3d SA memory block is not a subset of the 1st one
    if (dma_handle3 != NULL &&
        !adapter_dma_resource_is_sub_range_of(dma_handle3, dma_handle1))
        dma_resource_pre_dma(dma_handle3, 0, 0);

    LOG_INFO("\n\t pec_sa_register done \n");

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * pec_sa_un_register
 */
pec_status_t
pec_sa_un_register(
        const unsigned int interface_id,
        dma_buf_handle_t sa_handle1,
        dma_buf_handle_t sa_handle2,
        dma_buf_handle_t sa_handle3)
{
    dma_resource_handle_t sa_handle[3];
    pec_status_t res;
    int i, max_handles;

    LOG_INFO("\n\t pec_sa_un_register \n");

    IDENTIFIER_NOT_USED(interface_id);

    res = adapter_pec_dev_sa_remove(sa_handle1, sa_handle2, sa_handle3);
    if (res != PEC_STATUS_OK)
    {
        LOG_CRIT(
            "pec_sa_un_register: "
            "adapter_pec_dev_sa_remove returned %d\n",
            res);
        return PEC_ERROR_INTERNAL;
    }

    sa_handle[0] = adapter_dma_buf_handle2_dma_resource_handle(sa_handle1);
    sa_handle[1] = adapter_dma_buf_handle2_dma_resource_handle(sa_handle2);
    sa_handle[2] = adapter_dma_buf_handle2_dma_resource_handle(sa_handle3);

    // Check if the 3d SA memory block is not a subset of the 1st one
    if (sa_handle[0] != NULL &&
        sa_handle[2] != NULL &&
        adapter_dma_resource_is_sub_range_of(sa_handle[2], sa_handle[0]))
        max_handles = 2;
    else
        max_handles = 3;

    for (i = 0; i < max_handles; i++)
    {
        if (dma_resource_is_valid_handle(sa_handle[i]))
        {
            dma_resource_handle_t dma_handle = sa_handle[i];
            void *host_addr;
            dma_resource_record_t * rec_p =
                dma_resource_handle2_record_ptr(dma_handle);

            // Check if a bounce buffer is in use
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
            void * orig_host_addr;
            dma_resource_record_t * host_rec_p = rec_p;

            orig_host_addr = adapter_dma_resource_host_addr(dma_handle);

            if (adapter_dma_resource_is_foreign_allocated(sa_handle[i]))
            {
                // Get bounce buffer handle and its record
                dma_handle = host_rec_p->bounce.bounce_handle;
                rec_p = dma_resource_handle2_record_ptr(dma_handle);
            }
#endif /* ADAPTER_PEC_REMOVE_BOUNCEBUFFERS */

            host_addr = adapter_dma_resource_host_addr(dma_handle);
            // ensure we look at valid engine-written data
            // 0,0 = "entire buffer"
            dma_resource_post_dma(dma_handle, 0, 0);

            // convert to host format
            if (rec_p != NULL)
                dma_resource_read32_array(
                    dma_handle,
                    0,
                    rec_p->props.size / 4,
                    host_addr);

            // copy from bounce buffer to original buffer
#ifndef ADAPTER_PEC_REMOVE_BOUNCEBUFFERS
            if (adapter_dma_resource_is_foreign_allocated(sa_handle[i]) &&
                host_rec_p != NULL)
            {
                // copy the data from bounce to original buffer
                memcpy(
                    orig_host_addr,
                    host_addr,
                    host_rec_p->props.size);

                // free the bounce handle
                dma_resource_release(host_rec_p->bounce.bounce_handle);
                host_rec_p->bounce.bounce_handle = NULL;
            }
#endif /* ADAPTER_PEC_REMOVE_BOUNCEBUFFERS */
        } // if handle valid
    } // for

    LOG_INFO("\n\t pec_sa_un_register done\n");

    return PEC_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * pec_packet_put
 */
pec_status_t
pec_packet_put(
        const unsigned int interface_id,
        const pec_command_descriptor_t * commands_p,
        const unsigned int commands_count,
        unsigned int * const put_count_p)
{
    unsigned int cmd_lp;
    unsigned int pkt_cnt;
    unsigned int cmd_descriptor_count;
    pec_status_t res = 0, res2, pec_rc = PEC_STATUS_OK;
    unsigned int free_slots;

    LOG_INFO("\n\t pec_packet_put \n");

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

#ifdef ADAPTER_PEC_STRICT_ARGS
    if (commands_p == NULL ||
        commands_count == 0 ||
        put_count_p == NULL)
    {
        return PEC_ERROR_BAD_PARAMETER;
    }
#endif

    // initialize the output parameters
    *put_count_p = 0;

#ifdef ADAPTER_PEC_STRICT_ARGS
    // validate the descriptors
    // (error out before bounce buffer allocation)
    for (cmd_lp = 0; cmd_lp < commands_count; cmd_lp++)
        if (commands_p[cmd_lp].bypass_word_count > 255)
            return PEC_ERROR_BAD_PARAMETER;
#endif /* ADAPTER_PEC_STRICT_ARGS */

    if (!adapter_lock_cs_enter(&adapter_pec_put_cs[interface_id]))
        return PEC_STATUS_BUSY;

    if (!pec_is_initialized[interface_id])
    {
        adapter_lock_cs_leave(&adapter_pec_put_cs[interface_id]);
        return PEC_ERROR_BAD_USE_ORDER;
    }

    cmd_descriptor_count = MIN(ADAPTER_PEC_MAX_LOGICDESCR, commands_count);
    free_slots = 0;
    cmd_lp = 0;
    while (cmd_lp < cmd_descriptor_count)
    {
        unsigned int j;
        unsigned int count;
        unsigned int non_sg_packets;

#ifndef ADAPTER_PEC_ENABLE_SCATTERGATHER
        non_sg_packets = cmd_descriptor_count - cmd_lp;
        // All remaining packets are non-sg.
#else
        unsigned int gather_particles;
        unsigned int scatter_particles;
        unsigned int i;

        for (i = cmd_lp; i < cmd_descriptor_count; i++)
        {
            pec_sg_list_get_capacity(commands_p[i].src_pkt_handle,
                                   &gather_particles);
            if (pec_continuous_scatter[interface_id])
                scatter_particles = 0;
            else
                pec_sg_list_get_capacity(commands_p[i].dst_pkt_handle,
                                       &scatter_particles);
            if ( gather_particles > 0 || scatter_particles > 0)
                break;
        }
        non_sg_packets = i - cmd_lp;

        if (non_sg_packets == 0)
        {
            bool f_success;

            if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                                   interface_id,
                                          RPM_FLAG_SYNC) != RPM_SUCCESS)
            {
                pec_rc = PEC_ERROR_INTERNAL;
                break;
            }

            // First packet found is scatter gather.
            f_success = adapter_pec_dev_test_sg(interface_id,
                                             gather_particles,
                                             scatter_particles);

            (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                           interface_id,
                                           RPM_FLAG_ASYNC);

            if (!f_success)
            {
                pec_rc = PEC_ERROR_INTERNAL;
                break;
            }

            // Process a single sg packet in this iteration.
            free_slots = 1;
        }
        else
#endif
        if (free_slots == 0)
        {
            if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                                   interface_id,
                                          RPM_FLAG_SYNC) != RPM_SUCCESS)
            {
                pec_rc = PEC_ERROR_INTERNAL;
                break;
            }

            // Allow all non-sg packets to be processed in this iteration,
            // but limited by the number of free slots in the ring(s).
		//Note: for Continous Scatter mode enabled on interface, it returns free CDR
		//doesn't do MIN(rdr,cdr) which is what reqd
            free_slots = adapter_pec_dev_get_free_space(interface_id);

            (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                           interface_id,
                                           RPM_FLAG_ASYNC);

            if (free_slots > non_sg_packets)
                free_slots = non_sg_packets;

            if (free_slots == 0)
                break;
        }

        for (pkt_cnt=0; pkt_cnt<free_slots; pkt_cnt++)
        {
            res = adapter_packet_prepare(interface_id,
                                         commands_p + cmd_lp + pkt_cnt);
            if (res != PEC_STATUS_OK)
            {
                LOG_CRIT("%s: adapter_packet_prepare error %d\n", __func__, res);
                pec_rc = res;
                break;
            }

            if (!pec_continuous_scatter[interface_id])
            {
                adapter_fifo_put(&(adapter_side_channel_fifo[interface_id]),
                                 commands_p[cmd_lp+pkt_cnt].user_p,
                                 commands_p[cmd_lp+pkt_cnt].src_pkt_handle,
                                 commands_p[cmd_lp+pkt_cnt].dst_pkt_handle,
                                 commands_p[cmd_lp+pkt_cnt].token_handle,
                                 commands_p[cmd_lp+pkt_cnt].bypass_word_count);
            }
        }

        // rpm_device_io_start() must be called for each successfully submitted
        // packet
        for (j = 0; j < pkt_cnt; j++)
        {
            // Skipped error checking to reduce code complexity
            (void)RPM_DEVICE_IO_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                                interface_id,
                                            RPM_FLAG_SYNC);
        }

        res2 = adapter_pec_dev_packet_put(interface_id,
                                         commands_p + cmd_lp,
                                         pkt_cnt,
                                         &count);
        if (res2 != PEC_STATUS_OK)
        {
            LOG_CRIT("%s: adapter_pec_dev_packet_put error %d\n", __func__, res2);
            pec_rc = res2;
        }

        free_slots -= count;
        *put_count_p += count;

        if (count <  pkt_cnt)
        {
            LOG_WARN("pec_packet_put: withdrawing %d prepared packets\n",
                     pkt_cnt - count);

            for (j = count; j < pkt_cnt; j++)
            {
                if (!pec_continuous_scatter[interface_id])
                {
                    adapter_fifo_withdraw(&(adapter_side_channel_fifo[interface_id]));
                    adapter_packet_finalize(commands_p[cmd_lp + j].src_pkt_handle,
                                            commands_p[cmd_lp + j].dst_pkt_handle,
                                            commands_p[cmd_lp + j].token_handle);
                }

                // RPM_DEVICE_IO_STOP_MACRO() must be called here for packets
                // which could not be successfully submitted,
                // for example because device queue was full
                (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                               interface_id,
                                               RPM_FLAG_ASYNC);
            }
            break;
        }

        cmd_lp += count;

        if (res != PEC_STATUS_OK || res2 != PEC_STATUS_OK)
        {
            pec_rc = PEC_ERROR_INTERNAL;
            break;
        }
    } // while

    LOG_INFO("\n\t pec_packet_put done \n");

    adapter_lock_cs_leave(&adapter_pec_put_cs[interface_id]);

    return pec_rc;
}


/*----------------------------------------------------------------------------
 * pec_packet_get
 */
pec_status_t
pec_packet_get(
        const unsigned int interface_id,
        pec_result_descriptor_t * results_p,
        const unsigned int results_limit,
        unsigned int * const get_count_p)
{
    LOG_INFO("\n\t pec_packet_get \n");

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

#ifdef ADAPTER_PEC_STRICT_ARGS
    if (results_p == NULL ||
        get_count_p == NULL ||
        results_limit == 0)
    {
        return PEC_ERROR_BAD_PARAMETER;
    }
#endif

    // initialize the output parameter
    *get_count_p = 0;

    if (!adapter_lock_cs_enter(&adapter_pec_get_cs[interface_id]))
        return PEC_STATUS_BUSY;

    if (!pec_is_initialized[interface_id])
    {
        adapter_lock_cs_leave(&adapter_pec_get_cs[interface_id]);
        return PEC_ERROR_BAD_USE_ORDER;
    }

    // read descriptors from PEC device
    {
        pec_status_t res;
        unsigned int res_lp;
        unsigned int limit = MIN(results_limit, ADAPTER_PEC_MAX_LOGICDESCR);
        unsigned int count;
        dma_buf_handle_t token_handle = dma_buf_null_handle;

        res=adapter_pec_dev_packet_get(interface_id,
                                      results_p,
                                      limit,
                                      &count);
        if (res != PEC_STATUS_OK)
        {
            LOG_CRIT("pec_packet_get() returned error: %d\n", res);
            adapter_lock_cs_leave(&adapter_pec_get_cs[interface_id]);
            return res;
        }

        for (res_lp = 0; res_lp < count; res_lp++)
        {
            // To help CommandNotifyCB
            if (res_lp == count-1)
                adapter_make_command_notify_call_back(interface_id);

            (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID +
                                                        interface_id,
                                           RPM_FLAG_ASYNC);
            if (pec_continuous_scatter[interface_id])
            {
                unsigned int i;
                dma_buf_handle_t dest_handle = dma_buf_null_handle;
                dma_resource_handle_t dma_res_handle;
                if (results_p[res_lp].num_particles == 1)
                {
                    /* No real scatter, can fixup using single destination
                       handle */
                    adapter_fifo_get(&adapter_side_channel_fifo[interface_id],
                                     NULL,
                                     NULL,
                                     &dest_handle,
                                     NULL,
                                     NULL,
				     interface_id);
                    dma_res_handle =
                        adapter_dma_buf_handle2_dma_resource_handle(dest_handle);
                    dma_resource_post_dma(dma_res_handle, 0, 0);
#ifdef ADAPTER_AUTO_FIXUP
                    io_token_fixup(results_p[res_lp].output_token_p,
                                  dest_handle);
#endif
                    results_p[res_lp].user_p = NULL;
                    results_p[res_lp].src_pkt_handle = dma_buf_null_handle;
                    results_p[res_lp].dst_pkt_handle = dest_handle;
                    results_p[res_lp].bypass_word_count = 0;
                    if (!dma_buf_handle_is_same(&results_p[res_lp].dst_pkt_handle,
                                              &dma_buf_null_handle))
                    {
                        results_p[res_lp].dst_pkt_p =
                            adapter_dma_resource_host_addr(
                                adapter_dma_buf_handle2_dma_resource_handle(
                                    results_p[res_lp].dst_pkt_handle));
                    }
                    else
                    {
                        results_p[res_lp].dst_pkt_p = NULL;
                    }
                }
                else
                {
#if defined(ADAPTER_PEC_ENABLE_SCATTERGATHER) && defined(ADAPTER_AUTO_FIXUP)
                    dma_resource_record_t * rec_p;
                    pec_status_t pec_rc;
                    dma_buf_handle_t fixup_sg_list; /* scatter list for Fixup in continuous scatter mode */
                    pec_rc = pec_sg_list_create(results_p[res_lp].num_particles,
                                               &fixup_sg_list);
#endif
                    for (i = 0; i < results_p[res_lp].num_particles; i++)
                    {
                        adapter_fifo_get(&adapter_side_channel_fifo[interface_id],
                                         NULL,
                                         NULL,
                                         &dest_handle,
                                         NULL,
                                     NULL,
					interface_id);
                        dma_res_handle =
                            adapter_dma_buf_handle2_dma_resource_handle(dest_handle);
                        dma_resource_post_dma(dma_res_handle, 0, 0);
#if defined(ADAPTER_PEC_ENABLE_SCATTERGATHER) && defined(ADAPTER_AUTO_FIXUP)
                        if (pec_rc == PEC_STATUS_OK)
                        {
                            rec_p = dma_resource_handle2_record_ptr(dma_res_handle);

                            pec_sg_list_write(fixup_sg_list,
                                             i,
                                             dest_handle,
                                             rec_p->props.size);
                        }
#endif
                    }
#if defined(ADAPTER_PEC_ENABLE_SCATTERGATHER) && defined(ADAPTER_AUTO_FIXUP)
                    if (pec_rc == PEC_STATUS_OK)
                    {
                        io_token_fixup(results_p[res_lp].output_token_p,
                              fixup_sg_list);
                        pec_sg_list_destroy(fixup_sg_list);
                    }

#endif
                    results_p[res_lp].user_p = NULL;
                    results_p[res_lp].src_pkt_handle = dma_buf_null_handle;
                    results_p[res_lp].dst_pkt_handle = dma_buf_null_handle;
                    results_p[res_lp].bypass_word_count = 0;
                }
            }
            else
            {
                adapter_fifo_get(&(adapter_side_channel_fifo[interface_id]),
                                 &(results_p[res_lp].user_p),
                                 &(results_p[res_lp].src_pkt_handle),
                                 &(results_p[res_lp].dst_pkt_handle),
                                 &token_handle,
                                 &(results_p[res_lp].bypass_word_count),
				 interface_id);

                adapter_packet_finalize(results_p[res_lp].src_pkt_handle,
                                    results_p[res_lp].dst_pkt_handle,
                                    token_handle);
#ifdef ADAPTER_AUTO_FIXUP
                io_token_fixup(results_p[res_lp].output_token_p,
                              results_p[res_lp].dst_pkt_handle);
#endif

                if (!dma_buf_handle_is_same(&results_p[res_lp].dst_pkt_handle,
                                          &dma_buf_null_handle))
                {
                    results_p[res_lp].dst_pkt_p =
                        adapter_dma_resource_host_addr(
                            adapter_dma_buf_handle2_dma_resource_handle(
                                results_p[res_lp].dst_pkt_handle));
                }
                else
                {
                    results_p[res_lp].dst_pkt_p = NULL;
                }
            }
            *get_count_p += 1;
        } // for
    }

    LOG_INFO("\n\t pec_packet_get done \n");

    adapter_lock_cs_leave(&adapter_pec_get_cs[interface_id]);

    return PEC_STATUS_OK;
}


pec_status_t
pec_inline_rd_put(
		const unsigned int interface_id,
		pec_result_descriptor_t *results_p, /*const pec_command_descriptor_t * commands_p,*/
		const unsigned int results_limit, /*const unsigned int commands_count,*/
		unsigned int *const get_count_p) /*unsigned int * const put_count_p */
{
	//Call Adapter_PECDev_Inline_Packet_Get()

	// initialize the output parameter
	*get_count_p = 0;
	pec_status_t res;
	unsigned int limit = MIN(results_limit, ADAPTER_PEC_MAX_LOGICDESCR);
	unsigned int count;

	{
		res = adapter_pec_dev_inline_rd_put(interface_id, results_p, limit, &count);
		if (res != PEC_STATUS_OK)
		{
			return res;
		}
	}
	*get_count_p = count;
	return PEC_STATUS_OK;
}


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
    pec_command_descriptor_t *command_p,
    const pec_packet_params_t *packet_params_p)
{
    return adapter_pec_dev_cd_control_write(command_p, packet_params_p);
}


/*----------------------------------------------------------------------------
 * pec_rd_status_read
 */
pec_status_t
pec_rd_status_read(
        const pec_result_descriptor_t * const result_p,
        pec_result_status_t * const result_status_p)
{
    return adapter_pec_dev_rd_status_read(result_p, result_status_p);
}


/*----------------------------------------------------------------------------
 * pec_command_notify_request
 */
pec_status_t
pec_command_notify_request(
        const unsigned int interface_id,
        pec_notify_function_t cb_func_p,
        const unsigned int commands_count)
{
    unsigned int packet_slots_empty_count;

    LOG_INFO("\n\t pec_command_notify_request \n");

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    if (cb_func_p == NULL ||
        commands_count == 0 ||
        commands_count > ADAPTER_PEC_MAX_PACKETS)
    {
        return PEC_ERROR_BAD_PARAMETER;
    }

    if (!pec_is_initialized[interface_id])
        return PEC_ERROR_BAD_USE_ORDER;

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
        return PEC_ERROR_INTERNAL;

    packet_slots_empty_count = adapter_pec_dev_get_free_space(interface_id);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                   RPM_FLAG_ASYNC);

    if (commands_count <= packet_slots_empty_count)
    {
        LOG_INFO(
            "pec_command_notify_request: "
            "Invoking command notify callback immediately\n");

        cb_func_p();
    }
    else
    {
        pec_notify[interface_id].commands_count = commands_count;
        pec_notify[interface_id].command_notify_cb_p = cb_func_p;

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
        if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                      RPM_FLAG_SYNC) != RPM_SUCCESS)
            return PEC_ERROR_INTERNAL;

        /* Note that space for new commands may have become available before
         * the call to pec_command_notify_request and the associated interrupt
         * may already be pending. In this case the interrupt will occur
         * immediately.
         */
        adapter_pec_dev_enable_command_irq(interface_id);

        (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                       RPM_FLAG_ASYNC);
#endif /* ADAPTER_PEC_INTERRUPTS_ENABLE */
    }

    LOG_INFO("\n\t pec_command_notify_request done \n");

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * pec_result_notify_request
 */
pec_status_t
pec_result_notify_request(
        const unsigned int interface_id,
        pec_notify_function_t cb_func_p,
        const unsigned int results_count)
{
    LOG_INFO("\n\t pec_result_notify_request \n");

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    if (cb_func_p == NULL ||
        results_count == 0 ||
        results_count > ADAPTER_PEC_MAX_PACKETS)
    {
        return PEC_ERROR_BAD_PARAMETER;
    }

    if (!pec_is_initialized[interface_id])
        return PEC_ERROR_BAD_USE_ORDER;

    // install it
    pec_notify[interface_id].results_count = results_count;
    pec_notify[interface_id].result_notify_cb_p = cb_func_p;

#ifdef ADAPTER_PEC_INTERRUPTS_ENABLE
    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
        return PEC_ERROR_INTERNAL;

    /* Note that results may have become available before the call
       to pec_result_notify_request and the associated interrupts may already
       be pending. In this case the interrupt will occur immediately.
     */
    adapter_pec_dev_enable_result_irq(interface_id);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_PEC_RPM_EIP202_DEVICE0_ID + interface_id,
                                   RPM_FLAG_ASYNC);
#endif /* ADAPTER_PEC_INTERRUPTS_ENABLE */

    LOG_INFO("\n\t pec_result_notify_request done\n");

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * pec_scatter_preload
 */
pec_status_t
pec_scatter_preload(
        const unsigned int interface_id,
        dma_buf_handle_t * handles_p,
        const unsigned int handles_count,
        unsigned int * const accepted_count_p)
{
    pec_status_t rc;
    unsigned int i;
    unsigned int handles_to_use,ri,wi;
        LOG_INFO("\n\t pec_scatter_preload\n");
    //LOG_CRIT("custom-preload-debug: AdapterPEC_ RING:%d, handle-count:%d\n", interface_id, handles_count );

    if (interface_id >= ADAPTER_PEC_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    if (!pec_is_initialized[interface_id] || !pec_continuous_scatter[interface_id])
    {
        return PEC_ERROR_BAD_USE_ORDER;
    }

    if (!adapter_lock_cs_enter(&adapter_pec_preload_cs[interface_id]))
        return PEC_STATUS_BUSY;

    if (!pec_is_initialized[interface_id] || !pec_continuous_scatter[interface_id])
    {
        adapter_lock_cs_leave(&adapter_pec_preload_cs[interface_id]);
        return PEC_ERROR_BAD_USE_ORDER;
    }
    ri = adapter_side_channel_fifo[interface_id].read_index;
    wi = adapter_side_channel_fifo[interface_id].write_index;
    // Compute the number of free slots in the ring from the read/write index
    // of the side channel fifo (which is larger than the ring)
    if (wi >= ri)
        handles_to_use = ADAPTER_PEC_MAX_PACKETS - 1 - wi + ri ;
    else
        handles_to_use = ADAPTER_PEC_MAX_PACKETS - 1
            - adapter_side_channel_fifo[interface_id].size - wi + ri;
    if (handles_to_use > ADAPTER_PEC_MAX_LOGICDESCR)
        handles_to_use = ADAPTER_PEC_MAX_LOGICDESCR;
    if (handles_to_use > handles_count)
        handles_to_use = handles_count;
    for (i=0; i < handles_to_use; i++)
    {
        //LOG_CRIT("custom-preload-debug: AdapterPEC_ handle:%px\n", handles_p[i]);
        dma_resource_handle_t dma_res_handle =
            adapter_dma_buf_handle2_dma_resource_handle(handles_p[i]);
        dma_resource_pre_dma(dma_res_handle, 0, 0);
        adapter_fifo_put(&adapter_side_channel_fifo[interface_id],
                         NULL,
                         dma_buf_null_handle,
                         handles_p[i],
                         dma_buf_null_handle,
                         0);
    }
    rc = adapter_pec_dev_scatter_preload(interface_id,
                                        handles_p,
                                        handles_to_use);

    *accepted_count_p = handles_to_use;
    adapter_lock_cs_leave(&adapter_pec_preload_cs[interface_id]);
    return rc;
}


/*----------------------------------------------------------------------------
 * pec_scatter_preload_notify_request
 */
pec_status_t
pec_scatter_preload_notify_request(
        const unsigned int interface_id,
        pec_notify_function_t cb_func_p,
        const unsigned int consumed_count)
{
    IDENTIFIER_NOT_USED(interface_id);
    IDENTIFIER_NOT_USED(cb_func_p);
    IDENTIFIER_NOT_USED(consumed_count);

    return PEC_ERROR_NOT_IMPLEMENTED;
}


/*----------------------------------------------------------------------------
 * pec_put_dump
 */
void
pec_put_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_rdr_admin,
        const bool f_dump_rdr_cache)
{
    adapter_pec_dev_put_dump(interface_id,
                            first_slot_id,
                            last_slot_id,
                            f_dump_rdr_admin,
                            f_dump_rdr_cache);
}




/*----------------------------------------------------------------------------
 * pec_get_dump
 */
void
pec_get_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_rdr_admin,
        const bool f_dump_rdr_cache)
{
    adapter_pec_dev_get_dump(interface_id,
                            first_slot_id,
                            last_slot_id,
                            f_dump_rdr_admin,
                            f_dump_rdr_cache);
}


#endif /* ADAPTER_PE_MODE_DHM */


/* end of file adapter_pec_dma.c */
