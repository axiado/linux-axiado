/* adapter_ring_eip202.c
 *
 * Adapter EIP-202 implementation: EIP-202 specific layer.
 */

/*****************************************************************************
* Copyright (c) 2011-2024 by Rambus, Inc. and/or its subsidiaries.
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

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// SLAD Adapter PEC device-specific API
#include "adapter_pecdev_dma.h"

// Ring Control configuration API
#include "adapter_ring_eip202.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_adapter_eip202.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool
#include "api_pec.h"
#include "api_dmabuf.h"

// Adapter DMABuf internal API
#include "adapter_dmabuf.h"

// Convert address to pair of 32-bit words.
#include "adapter_addrpair.h"

// Adapter interrupts API
#include "adapter_interrupts.h" // Adapter_Interrupt_*

// Adapter memory allocation API
#include "adapter_alloc.h"      // adapter_alloc/Free

// Driver Framework DMAResource API
#include "dmares_addr.h"      // AddrTrans_*
#include "dmares_buf.h"       // dma_resource_alloc/Release
#include "dmares_rw.h"        // dma_resource_pre_dma/PostDMA
#include "dmares_mgmt.h"      // dma_resource_alloc/Release

#include "device_types.h"  // device_handle_t
#include "device_mgmt.h" // Device_find
#include "device_rw.h" // device register read/write

#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
#include "api_pec_sg.h"         // PEC_SG_* (the API we implement here)
#endif

// EIP97 Ring Control
#include "eip202_ring_types.h"
#include "eip202_cdr.h"
#include "eip202_rdr.h"

// Standard IOToken API
#include "iotoken.h"


#include "eip97_global_init.h"

#include "clib.h"  // memcpy, zeroinit

// Log API
#include "log.h"


/*----------------------------------------------------------------------------
 * Definitions and macros
 */
#ifdef ADAPTER_EIP202_USE_POINTER_TYPES
#define ADAPTER_EIP202_DUMMY_DMA_ADDRESS_LO       0xfffffffc
#define ADAPTER_EIP202_DUMMY_DMA_ADDRESS_HI       0xffffffff
#else
#define ADAPTER_EIP202_DUMMY_DMA_ADDRESS_LO       3
#define ADAPTER_EIP202_DUMMY_DMA_ADDRESS_HI       0
#endif


// Move bit position src in value v to dst.
#define BIT_MOVE(v, src, dst)  ( ((src) < (dst)) ? \
    ((v) << ((dst) - (src))) & (1 << (dst)) :  \
    ((v) >> ((src) - (dst))) & (1 << (dst)))

#ifndef ADAPTER_EIP202_SEPARATE_RINGS
#define ADAPTER_EIP202_CDR_BYTE_OFFSET              \
                    (EIP202_RD_CTRL_DATA_MAX_WORD_COUNT * sizeof(u32))
#endif


typedef struct
{
    unsigned int CDR_IRQ_ID;

    unsigned int cdr_irq_flags;

    const char * cdr_device_name_p;

    unsigned int RDR_IRQ_ID;

    unsigned int rdr_irq_flags;

    const char * rdr_device_name_p;

} adapter_ring_eip202_device_t;


#ifdef ADAPTER_EIP202_USE_POINTER_TYPES
#define ADAPTER_EIP202_TR_ADDRESS       2
#ifndef ADAPTER_EIP202_USE_LARGE_TRANSFORM_DISABLE
#define ADAPTER_EIP202_TR_LARGE_ADDRESS 3
// Bit in first word of SA record to indicate it is large.
#define ADAPTER_EIP202_TR_ISLARGE              BIT_4
#endif
#endif


/*----------------------------------------------------------------------------
 * Local variables
 */

static const adapter_ring_eip202_device_t eip202_devices [] =
{
    ADAPTER_EIP202_DEVICES
};

// number of devices supported calculated on ADAPTER_EIP202_DEVICES defined
// in c_adapter_eip202.h
#define ADAPTER_EIP202_DEVICE_COUNT_ACTUAL \
        (sizeof(eip202_devices) / sizeof(adapter_ring_eip202_device_t))

static eip202_cdr_settings_t  cdr_settings;
static eip202_rdr_settings_t  rdr_settings;

#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
static unsigned int eip202_interrupts[ADAPTER_EIP202_DEVICE_COUNT];
#endif

static eip202_ring_io_area_t cdr_io_area[ADAPTER_EIP202_DEVICE_COUNT];
static eip202_ring_io_area_t rdr_io_area[ADAPTER_EIP202_DEVICE_COUNT];

static dma_resource_handle_t cdr_handle[ADAPTER_EIP202_DEVICE_COUNT];
static dma_resource_handle_t rdr_handle[ADAPTER_EIP202_DEVICE_COUNT];

#ifdef ADAPTER_AUTO_TOKENBUILDER
static dma_resource_handle_t token_buf_handle[ADAPTER_EIP202_DEVICE_COUNT];
static u32 * token_buf_host_addr[ADAPTER_EIP202_DEVICE_COUNT];
static uintptr_t token_buf_phys_addr[ADAPTER_EIP202_DEVICE_COUNT];
static unsigned int token_buf_index[ADAPTER_EIP202_DEVICE_COUNT];
#endif

static  eip202_arm_command_descriptor_t
    eip202_cdr_entries[ADAPTER_EIP202_DEVICE_COUNT][ADAPTER_EIP202_MAX_LOGICDESCR];

static  eip202_arm_prepared_descriptor_t
    eip202_rdr_prepared[ADAPTER_EIP202_DEVICE_COUNT][ADAPTER_EIP202_MAX_LOGICDESCR];

static  eip202_arm_result_descriptor_t
    eip202_rdr_entries[ADAPTER_EIP202_DEVICE_COUNT][ADAPTER_EIP202_MAX_LOGICDESCR];

static bool eip202_continuous_scatter[ADAPTER_EIP202_DEVICE_COUNT];

static const pec_capabilities_t capabilities_string =
{
    "EIP-202 Packet Engine rings=__ (ARM,"        // sz_text_description
#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
    "sg,"
#endif
#ifndef ADAPTER_EIP202_REMOVE_BOUNCEBUFFERS
    "BB,"
#endif
#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
    "Int)",
#else
    "Poll)",
#endif
    0
};

// Static variables used by the adapter_ring_eip202.h interface implementation
static bool    adapter_ring_eip202_configured;
static u8 adapter_ring_eip202_hdw;
static u8 adapter_ring_eip202_cf_size;
static u8 adapter_ring_eip202_rf_size;
static u8 adapter_ring_eip202_nof_pes = 1;
static u8 adapter_ring_eip202_major_version;
static u8 adapter_ring_eip202_minor_version;
static u8 adapter_ring_eip202_patch_level;

// DMA buffer allocation alignment in bytes
static int adapter_ring_eip202_dma_alignment_byte_count =
                                        ADAPTER_DMABUF_ALIGNMENT_INVALID;


#ifdef ADAPTER_EIP202_ADD_INIT_DIAGNOSTICS
static void
adapter_register_dump(void)
{
    device_handle_t device=eip_device_find("EIP197_GLOBAL");
    unsigned int i,j,v[256];
    LOG_CRIT("REGISTER DUMP\n");
    for (i=0; i<0x100000; i+=1024)
    {
        device_read32_array(device, i, v, 256);
        for (j=0; j<256; j++)
        {
            if (v[j])
            {
                LOG_CRIT("addr 0x%08x = 0x%08x\n",i+j*4,v[j]);
            }
        }
    }
    LOG_CRIT("END REGISTER DUMP\n");
}
#endif

/*----------------------------------------------------------------------------
 * adapter_lib_rd_out_tokens_free
 *
 * Free Output Tokens in the EIP-202 result descriptors.
 */
static void
adapter_lib_rd_out_tokens_free(
        const unsigned int interface_id)
{
    unsigned int i;

    for (i = 0; i < ADAPTER_EIP202_MAX_LOGICDESCR; i++)
        adapter_free(eip202_rdr_entries[interface_id][i].token_p);
}


/*----------------------------------------------------------------------------
 * adapter_lib_rd_out_tokens_alloc
 *
 * Allocate Output Tokens in the EIP-202 result descriptors.
 */
static bool
adapter_lib_rd_out_tokens_alloc(
        const unsigned int interface_id)
{
    unsigned int i;

    for (i = 0; i < ADAPTER_EIP202_MAX_LOGICDESCR; i++)
    {
        u32 * p =
               adapter_alloc(io_token_out_word_count_get() * sizeof (u32));

        if (p)
            eip202_rdr_entries[interface_id][i].token_p = p;
        else
            goto exit_error;
    }

    return true;

exit_error:
    adapter_lib_rd_out_tokens_free(interface_id);

    return false;
}


/*----------------------------------------------------------------------------
 * adapter_get_phys_addr
 *
 * Obtain the physical address from a DMABuf handle.
 * Take the bounce handle into account.
 *
 */
static void
adapter_get_phys_addr(
        const dma_buf_handle_t handle,
        eip202_device_address_t *phys_addr_p)
{
    dma_resource_handle_t dma_handle =
        adapter_dma_buf_handle2_dma_resource_handle(handle);
    dma_resource_record_t * rec_p = NULL;
    dma_resource_addr_pair_t phys_addr_pair;

    if (phys_addr_p == NULL)
    {
        LOG_CRIT("adapter_get_phys_addr: PANIC\n");
        return;
    }

    phys_addr_p->addr        = ADAPTER_EIP202_DUMMY_DMA_ADDRESS_LO;
    phys_addr_p->upper_addr   = ADAPTER_EIP202_DUMMY_DMA_ADDRESS_HI;

    // Note: this function is sometimes invoked with invalid DMA handle
    //       to obtain a dummy address, not an error.
    if (!dma_resource_is_valid_handle(dma_handle))
        return; // success!

    rec_p = dma_resource_handle2_record_ptr(dma_handle);

    if (rec_p == NULL)
    {
        LOG_CRIT("adapter_get_phys_addr: failed\n");
        return;
    }

#ifndef ADAPTER_EIP202_REMOVE_BOUNCEBUFFERS
    if (rec_p->bounce.bounce_handle != NULL)
        dma_handle = rec_p->bounce.bounce_handle;
#endif

    if (dma_resource_translate(dma_handle, DMARES_DOMAIN_BUS,
                              &phys_addr_pair) == 0)
    {
        adapter_addr_to_word_pair(phys_addr_pair.address_p, 0, &phys_addr_p->addr,
                              &phys_addr_p->upper_addr);
        // success!
    }
    else
    {
        LOG_CRIT("adapter_get_phys_addr: failed\n");
    }
}


/*----------------------------------------------------------------------------
 * bool_to_string()
 *
 * Convert boolean value to string.
 */
static const char *
bool_to_string(
        bool b)
{
    if (b)
        return "TRUE";
    else
        return "false";
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_cdr_status_report
 *
 * Report the status of the CDR interface
 *
 */
static void
adapter_lib_ring_eip202_cdr_status_report(
        unsigned int interface_id)
{
    eip202_ring_error_t res;
    eip202_cdr_status_t cdr_status;

    LOG_CRIT("status of CDR interface %d\n",interface_id);

    LOG_INFO("\n\t\t\t eip202_cdr_status_get \n");

    zeroinit(cdr_status);
    res = eip202_cdr_status_get(cdr_io_area + interface_id, &cdr_status);
    if (res != EIP202_RING_NO_ERROR)
    {
        LOG_CRIT("eip202_cdr_status_get returned error\n");
        return;
    }

    // Report CDR status
    LOG_CRIT("CDR status: CD prep/proc count %d/%d, proc pkt count %d\n",
             cdr_status.cd_prep_word_count,
             cdr_status.cd_proc_word_count,
             cdr_status.cd_proc_pkt_word_count);

    if (cdr_status.f_dma_error    ||
        cdr_status.f_error       ||
        cdr_status.f_ou_flow_error ||
        cdr_status.f_treshold_int ||
        cdr_status.f_timeout_int)
    {
        // Report CDR errors
        LOG_CRIT("CDR status: error(s) detected\n");
        LOG_CRIT("\tDMA err:       %s\n"
                 "\tErr:           %s\n"
                 "\tOvf/under err: %s\n"
                 "\tThreshold int: %s\n"
                 "\tTimeout int:   %s\n"
                 "\tFIFO count:    %d\n",
                 bool_to_string(cdr_status.f_dma_error),
                 bool_to_string(cdr_status.f_error),
                 bool_to_string(cdr_status.f_ou_flow_error),
                 bool_to_string(cdr_status.f_treshold_int),
                 bool_to_string(cdr_status.f_timeout_int),
                 cdr_status.cdfifo_word_count);
    }
    else
        LOG_CRIT("CDR status: all OK, fifo count: %d\n",
                 cdr_status.cdfifo_word_count);
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_rdr_status_report
 *
 * Report the status of the RDR interface
 *
 */
static void
adapter_lib_ring_eip202_rdr_status_report(
        unsigned int interface_id)
{
    eip202_ring_error_t res;
    eip202_rdr_status_t rdr_status;

    LOG_CRIT("status of RDR interface %d\n",interface_id);
    if (eip202_continuous_scatter[interface_id])
        LOG_CRIT("Ring is in continuous scatter mode\n");

    LOG_INFO("\n\t\t\t eip202_rdr_status_get \n");

    zeroinit(rdr_status);
    res = eip202_rdr_status_get(rdr_io_area + interface_id, &rdr_status);
    if (res != EIP202_RING_NO_ERROR)
    {
        LOG_CRIT("eip202_rdr_status_get returned error\n");
        return;
    }

    // Report RDR status
    LOG_CRIT("RDR status: RD prep/proc count %d/%d, proc pkt count %d\n",
             rdr_status.rd_prep_word_count,
             rdr_status.rd_proc_word_count,
             rdr_status.rd_proc_pkt_word_count);

    if (rdr_status.f_dma_error         ||
        rdr_status.f_error            ||
        rdr_status.f_ou_flow_error      ||
        rdr_status.f_rd_buf_overflow_int ||
        rdr_status.f_rd_overflow_int    ||
        rdr_status.f_treshold_int      ||
        rdr_status.f_timeout_int)
    {
        // Report RDR errors
        LOG_CRIT("RDR status: error(s) detected\n");
        LOG_CRIT("\tDMA err:        %s\n"
                 "\tErr:            %s\n"
                 "\tOvf/under err:  %s\n"
                 "\tBuf ovf:        %s\n"
                 "\tDescriptor ovf: %s\n"
                 "\tThreshold int:  %s\n"
                 "\tTimeout int:    %s\n"
                 "\tFIFO count:     %d\n",
                 bool_to_string(rdr_status.f_dma_error),
                 bool_to_string(rdr_status.f_error),
                 bool_to_string(rdr_status.f_ou_flow_error),
                 bool_to_string(rdr_status.f_rd_buf_overflow_int),
                 bool_to_string(rdr_status.f_rd_overflow_int),
                 bool_to_string(rdr_status.f_treshold_int),
                 bool_to_string(rdr_status.f_timeout_int),
                 rdr_status.rdfifo_word_count);
    }
    else
        LOG_CRIT("RDR status: all OK, fifo count: %d\n",
                 rdr_status.rdfifo_word_count);
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_status_report
 *
 * Report the status of the CDR and RDR interface
 *
 */
static void
adapter_lib_ring_eip202_status_report(
        unsigned int interface_id)
{
    adapter_lib_ring_eip202_cdr_status_report(interface_id);

    adapter_lib_ring_eip202_rdr_status_report(interface_id);
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_align_for_size
 */
static unsigned int
adapter_lib_ring_eip202_align_for_size(
        const unsigned int byte_count,
        const unsigned int align_to)
{
    unsigned int aligned_byte_count = byte_count;

    // Check if alignment and padding for length alignment is required
    if (align_to > 1 && byte_count % align_to)
        aligned_byte_count = byte_count / align_to * align_to + align_to;

    return aligned_byte_count;
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_dma_alignment_determine
 *
 * Determine the required EIP-202 DMA alignment value
 *
 */
static bool
adapter_lib_ring_eip202_dma_alignment_determine(void)
{
#ifdef ADAPTER_EIP202_ALLOW_UNALIGNED_DMA
    adapter_dma_resource_alignment_set(1);
#else
    // Default alignment value is invalid
    adapter_ring_eip202_dma_alignment_byte_count =
                                ADAPTER_DMABUF_ALIGNMENT_INVALID;

    if (adapter_ring_eip202_configured)
    {
        int align_to = adapter_ring_eip202_hdw;

        // Determine the EIP-202 master interface hardware data width
        switch (align_to)
        {
            case EIP202_RING_DMA_ALIGNMENT_4_BYTES:
                adapter_dma_resource_alignment_set(4);
                break;
            case EIP202_RING_DMA_ALIGNMENT_8_BYTES:
                adapter_dma_resource_alignment_set(8);
                break;
            case EIP202_RING_DMA_ALIGNMENT_16_BYTES:
                adapter_dma_resource_alignment_set(16);
                break;
            case EIP202_RING_DMA_ALIGNMENT_32_BYTES:
                adapter_dma_resource_alignment_set(32);
                break;
            default:
                // Not supported, the alignment value cannot be determined
                LOG_CRIT("adapter_lib_ring_eip202_dma_alignment_determine: "
                         "EIP-202 master interface HW data width "
                         "(%d) is unsupported\n",
                         align_to);
                return false; // error
        } // switch
    }
    else
    {
#if ADAPTER_EIP202_DMA_ALIGNMENT_BYTE_COUNT == 0
        // The alignment value cannot be determined
        return false; // error
#else
        // Set the configured non-zero alignment value
        adapter_dma_resource_alignment_set(
                ADAPTER_EIP202_DMA_ALIGNMENT_BYTE_COUNT);
#endif
    }
#endif

    adapter_ring_eip202_dma_alignment_byte_count =
                            adapter_dma_resource_alignment_get();

    return true; // Success
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_dma_alignment_get
 *
 * Get the required EIP-202 DMA alignment value
 *
 */
inline static int
adapter_lib_ring_eip202_dma_alignment_get(void)
{
    return adapter_ring_eip202_dma_alignment_byte_count;
}


/*----------------------------------------------------------------------------
 * adapter_lib_ring_eip202_dma_addr_is_sane
 *
 * Validate the DMA address for the EIP-202 device
 *
 */
inline static bool
adapter_lib_ring_eip202_dma_addr_is_sane(
        const eip202_device_address_t * const dev_addr_p,
        const char * buffer_name_p)
{
#ifndef ADAPTER_EIP202_64BIT_DEVICE
    // Check if 64-bit DMA address is used for EIP-202 configuration
    // that supports 32-bit addresses only
    if (dev_addr_p->upper_addr)
    {
        LOG_CRIT("%s: failed, "
                 "%s bus address (low/high 32 bits 0x%08x/0x%08x) too big\n",
                 __func__,
                 buffer_name_p,
                 dev_addr_p->addr,
                 dev_addr_p->upper_addr);
        return false;
    }
    else
        return true;
#else
    IDENTIFIER_NOT_USED(dev_addr_p);
    IDENTIFIER_NOT_USED(buffer_name_p);
    return true;
#endif
}


/*----------------------------------------------------------------------------
 * Implementation of the adapter_ring_eip202.h interface
 *
 */

/*----------------------------------------------------------------------------
 * adapter_ring_eip202_configure
 */
void
adapter_ring_eip202_configure(
        const u8 host_data_width,
        const u8 cf_size,
        const u8 rf_size,
        const u8 nof_p_es,
        const u8 major_version,
        const u8 minor_version,
        const u8 patch_level)
{
    adapter_ring_eip202_hdw    = host_data_width;
    adapter_ring_eip202_cf_size = cf_size;
    adapter_ring_eip202_rf_size = rf_size;
    adapter_ring_eip202_nof_pes = nof_p_es;
    adapter_ring_eip202_major_version = major_version;
    adapter_ring_eip202_minor_version = minor_version;
    adapter_ring_eip202_patch_level = patch_level;

    adapter_ring_eip202_configured = true;
}

/*----------------------------------------------------------------------------
 * Implementation of the adapter_rc_eip207.h interface
 *
 */

/*----------------------------------------------------------------------------
 * Implementation of the adapter_pecdev_dma.h interface
 *
 */

/*----------------------------------------------------------------------------
 * adapter_pec_dev_capabilities_get
 */
pec_status_t
adapter_pec_dev_capabilities_get(
        pec_capabilities_t * const capabilities_p)
{
    u8 versions[2];

    if (capabilities_p == NULL)
        return PEC_ERROR_BAD_PARAMETER;

    memcpy(capabilities_p, &capabilities_string, sizeof(capabilities_string));

    // now fill in the number of rings.
    {
        versions[0] = ADAPTER_EIP202_DEVICE_COUNT / 10;
        versions[1] = ADAPTER_EIP202_DEVICE_COUNT % 10;
    }

    {
        char * p = capabilities_p->sz_text_description;
        int ver_index = 0;
        int i = 0;

        if (p[PEC_MAXLEN_TEXT-1] != 0)
            return PEC_ERROR_INTERNAL;

        while(p[i])
        {
            if (p[i] == '_')
            {
                if (ver_index == sizeof(versions)/sizeof(versions[0]))
                    return PEC_ERROR_INTERNAL;
                if (versions[ver_index] > 9)
                    p[i] = '?';
                else
                    p[i] = '0' + versions[ver_index++];
            }

            i++;
        }
    }
    capabilities_p->supported_funcs = eip97_supported_funcs_get();
    return PEC_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * Adapter_PECDev_Control_Write
 */
pec_status_t
adapter_pec_dev_cd_control_write(
    pec_command_descriptor_t *command_p,
    const pec_packet_params_t *packet_params_p)
{
    IDENTIFIER_NOT_USED(command_p);
    IDENTIFIER_NOT_USED(packet_params_p);

    return PEC_ERROR_NOT_IMPLEMENTED;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_rd_status_read
 */
pec_status_t
adapter_pec_dev_rd_status_read(
        const pec_result_descriptor_t * const result_p,
        pec_result_status_t * const result_status_p)
{
    unsigned int s;

#ifdef ADAPTER_EIP202_STRICT_ARGS
    if (result_status_p == NULL)
        return PEC_ERROR_BAD_PARAMETER;
#endif

    if (io_token_error_code_get(result_p->output_token_p, &s) < 0)
        return PEC_ERROR_BAD_PARAMETER;

    // Translate the EIP-96 error codes to the PEC error codes
    result_status_p->errors =
        BIT_MOVE(s,  0, 11) | // EIP-96 Packet length error
        BIT_MOVE(s,  1, 17) | // EIP-96 Token error
        BIT_MOVE(s,  2, 18) | // EIP-96 Bypass error
        BIT_MOVE(s,  3,  9) | // EIP-96 Block size error
        BIT_MOVE(s,  4, 16) | // EIP-96 Hash block size error.
        BIT_MOVE(s,  5, 10) | // EIP-96 Invalid combo
        BIT_MOVE(s,  6,  5) | // EIP-96 Prohibited algo
        BIT_MOVE(s,  7, 19) | // EIP-96 Hash overflow
#ifndef ADAPTER_EIP202_RING_TTL_ERROR_WA
        BIT_MOVE(s,  8, 20) | // EIP-96 ttl/Hop limit underflow.
#endif
        BIT_MOVE(s,  9,  0) | // EIP-96 Authentication failed
        BIT_MOVE(s, 10,  2) | // EIP-96 Sequence number check failed.
        BIT_MOVE(s, 11,  8) | // EIP-96 spi check failed.
        BIT_MOVE(s, 12, 21) | // EIP-96 Incorrect checksum
        BIT_MOVE(s, 12,  1) | // EIP-96 Pad verification.
        BIT_MOVE(s, 14, 22);  // EIP-96 timeout

    // Translate the EIP-202 error codes to the PEC error codes
    s = result_p->status1;
    result_status_p->errors  |=
        BIT_MOVE(s, 21, 26) | // EIP-202 Buffer overflow
        BIT_MOVE(s, 20, 25);  // EIP-202 Descriptor overflow

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_init
 */
pec_status_t
adapter_pec_dev_init(
        const unsigned int interface_id,
        const pec_init_block_t * const init_block_p)
{
    device_handle_t cdr_device, rdr_device;
    eip202_ring_error_t res;
    dma_resource_addr_pair_t phys_addr_pair;
    unsigned int cd_word_count, rd_word_count; // command and Result Descriptor size
    unsigned int cd_offset, rd_offset; // command and Result Descriptor Offsets
    unsigned int rd_token_offs_word_count;

    IDENTIFIER_NOT_USED(init_block_p);

    LOG_INFO("\n\t\t adapter_pec_dev_init \n");

    if (ADAPTER_EIP202_DEVICE_COUNT > ADAPTER_EIP202_DEVICE_COUNT_ACTUAL)
    {
        LOG_CRIT("adapter_pec_dev_init: "
                 "Adapter EIP-202 devices configuration is invalid\n");
        return PEC_ERROR_BAD_PARAMETER;
    }

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    cdr_device = eip_device_find(eip202_devices[interface_id].cdr_device_name_p);
    rdr_device = eip_device_find(eip202_devices[interface_id].rdr_device_name_p);

    if (cdr_device == NULL || rdr_device == NULL)
        return PEC_ERROR_INTERNAL;
    eip202_continuous_scatter[interface_id] = init_block_p->f_continuous_scatter;

    LOG_INFO("\n\t\t\t eip202_cdr_reset \n");

    // Reset both the CDR and RDR devives.
    res = eip202_cdr_reset(cdr_io_area + interface_id, cdr_device);
    if (res != EIP202_RING_NO_ERROR)
        return PEC_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t eip202_rdr_reset \n");

    res = eip202_rdr_reset(rdr_io_area + interface_id, rdr_device);
    if (res != EIP202_RING_NO_ERROR)
        return PEC_ERROR_INTERNAL;

    // Determine the DMA buffer allocation alignment
    if (!adapter_lib_ring_eip202_dma_alignment_determine())
        return PEC_ERROR_INTERNAL;

#ifdef ADAPTER_EIP202_RING_LOCAL_CONFIGURE
    // Configure the EIP-202 Ring Managers with configuration
    // parameters obtained from local CDR device.
    {
      eip202_ring_options_t options;

      res = eip202_cdr_options_get(cdr_device, &options);
      if (res != EIP202_RING_NO_ERROR)
      {
          LOG_CRIT("%s: eip202_cdr_options_get failed, error %d\n",
                   __func__,
                   res);
          return PEC_ERROR_INTERNAL;
      }
      adapter_ring_eip202_configure(options.hdw,
                                    options.cf_size,
                                    options.rf_size,
                                    options.nof_pes,
                                    options.major_version,
                                    options.minor_version,
                                    options.patch_level);
    }
#endif
    // Determine required command and result descriptor size and offset
    {
        unsigned int byte_count;

        cd_word_count = EIP202_CD_CTRL_DATA_MAX_WORD_COUNT +
                                            io_token_in_word_count_get();
        byte_count = MAX(adapter_lib_ring_eip202_dma_alignment_get(),
                        ADAPTER_EIP202_CD_OFFSET_BYTE_COUNT);
        byte_count = adapter_lib_ring_eip202_align_for_size(
                                cd_word_count * sizeof(u32),
                                byte_count);
        cd_offset = byte_count / sizeof(u32);

        LOG_CRIT("%s: EIP-202 CD size/offset %d/%d (32-bit words)\n",
                 __func__,
                 cd_word_count,
                 cd_offset);

        if (adapter_ring_eip202_hdw == 3)
        {
            rd_token_offs_word_count = 8;
        }
        else
        {
            rd_token_offs_word_count = EIP202_RD_CTRL_DATA_MAX_WORD_COUNT;
        }
        rd_word_count = rd_token_offs_word_count +
                                            io_token_out_word_count_get();
        byte_count = MAX(adapter_lib_ring_eip202_dma_alignment_get(),
                        ADAPTER_EIP202_RD_OFFSET_BYTE_COUNT);
        byte_count = adapter_lib_ring_eip202_align_for_size(
                                rd_word_count * sizeof(u32),
                                byte_count);
        rd_offset = byte_count / sizeof(u32);

        LOG_CRIT("%s: EIP-202 RD size/offset %d/%d (32-bit words)\n",
                 __func__,
                 rd_word_count,
                 rd_offset);

#ifndef ADAPTER_EIP202_SEPARATE_RINGS
        {
            unsigned int rd_max_token_word_count = rd_offset -
                                           EIP202_RD_CTRL_DATA_MAX_WORD_COUNT;

            if (cd_word_count > rd_max_token_word_count)
            {
                LOG_CRIT("%s: failed, EIP-202 CD (%d 32-bit words) "
                         "exceeds max RD token space (%d 32-bit words)\n",
                         __func__,
                         cd_word_count,
                         rd_max_token_word_count);
                return PEC_ERROR_INTERNAL;
            }
        }
#endif
    }

#ifdef ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE
#ifdef ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
    { // Set the SA pool base address.
        int dmares;
        device_handle_t device;
        dma_resource_handle_t dma_handle;
        dma_resource_properties_t dma_properties;
        dma_resource_addr_pair_t dma_addr;

        // Perform a dummy allocation in bank 1 to obtain the pool base
        // address.
        dma_properties.alignment = adapter_lib_ring_eip202_dma_alignment_get();
        dma_properties.bank      = ADAPTER_EIP202_BANK_SA;
        dma_properties.f_cached   = false;
        dma_properties.size      = ADAPTER_EIP202_TRANSFORM_RECORD_COUNT *
                                    ADAPTER_EIP202_TRANSFORM_RECORD_BYTE_COUNT;

        dmares = dma_resource_alloc(dma_properties, &dma_addr, &dma_handle);
        if (dmares != 0)
            return PEC_ERROR_INTERNAL;

        // Derive the physical address from the DMA resource.
        if (dma_resource_translate(dma_handle,
                                  DMARES_DOMAIN_BUS,
                                  &dma_addr) < 0)
        {
            dma_resource_release(dma_handle);
            return PEC_ERROR_INTERNAL;
        }

        adapter_addr_to_word_pair(dma_addr.address_p,
                               0,
                               &eip202_sa_base_addr,
                               &eip202_sa_base_upper_addr);

        // Set the cache base address.
        device = eip_device_find(ADAPTER_EIP202_GLOBAL_DEVICE_NAME);
        if (device == NULL)
        {
            LOG_CRIT("adapter_pec_dev_uninit: Could not find device\n");
            return PEC_ERROR_INTERNAL;
        }

        LOG_INFO("\n\t\t\t EIP207_RC_BaseAddr_Set \n");

        EIP207_RC_BaseAddr_Set(
            device,
            EIP207_TRC_REG_BASE,
            EIP207_RC_SET_NR_DEFAULT,
            eip202_sa_base_addr,
            eip202_sa_base_upper_addr);

        // Release the DMA resource.
        dma_resource_release(dma_handle);
    }
#endif // ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
#endif // ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE

    // Allocate the ring buffers(s).
    {
        int dmares;
#ifdef ADAPTER_EIP202_SEPARATE_RINGS
        unsigned int cdr_byte_count = 4 * ADAPTER_EIP202_MAX_PACKETS * cd_offset;
        unsigned int rdr_byte_count = 4 * ADAPTER_EIP202_MAX_PACKETS * rd_offset;
#else
        unsigned int rdr_byte_count = 4 * ADAPTER_EIP202_MAX_PACKETS * cd_offset +
                                                ADAPTER_EIP202_CDR_BYTE_OFFSET;
#endif

        dma_resource_properties_t ring_properties;
        dma_resource_addr_pair_t ring_host_addr;

        // used as u32 array
        ring_properties.alignment = adapter_lib_ring_eip202_dma_alignment_get();
        ring_properties.bank      = ADAPTER_EIP202_BANK_RING;
        ring_properties.f_cached   = false;
        ring_properties.size      = rdr_byte_count;

        dmares = dma_resource_alloc(ring_properties,
                                   &ring_host_addr,
                                   rdr_handle + interface_id);
        if (dmares != 0)
            return PEC_ERROR_INTERNAL;

#ifdef ADAPTER_EIP202_ARMRING_ENABLE_SWAP
        dma_resource_swap_endianness_set(rdr_handle[interface_id],true);
#endif

        memset (ring_host_addr.address_p, 0, rdr_byte_count);

#ifdef ADAPTER_EIP202_SEPARATE_RINGS

        ring_properties.size = cdr_byte_count;

        dmares = dma_resource_alloc(ring_properties,
                                   &ring_host_addr,
                                   cdr_handle + interface_id);
        if (dmares != 0)
        {
            dma_resource_release(rdr_handle[interface_id]);
            return PEC_ERROR_INTERNAL;
        }

#ifdef ADAPTER_EIP202_ARMRING_ENABLE_SWAP
        dma_resource_swap_endianness_set(cdr_handle[interface_id], true);
#endif

        memset (ring_host_addr.address_p, 0, cdr_byte_count);

#else
        ring_properties.size -= ADAPTER_EIP202_CDR_BYTE_OFFSET;
        ring_properties.f_cached = false;

        {
            u8 * byte_p = (u8*)ring_host_addr.address_p;

            byte_p += ADAPTER_EIP202_CDR_BYTE_OFFSET;

            ring_host_addr.address_p = byte_p;
        }

        dmares = dma_resource_check_and_register(ring_properties,
                                              ring_host_addr,
                                              'R',
                                              cdr_handle + interface_id);
        if (dmares != 0)
        {
            dma_resource_release(rdr_handle[interface_id]);
            return PEC_ERROR_INTERNAL;
        }

#ifdef ADAPTER_EIP202_ARMRING_ENABLE_SWAP
        dma_resource_swap_endianness_set(cdr_handle[interface_id], true);
#endif

#endif

        LOG_CRIT("%s: CDR/RDR byte count %d/%d, %s, CDR offset %d bytes\n",
                 __func__,
#ifdef ADAPTER_EIP202_SEPARATE_RINGS
                 cdr_byte_count,
                 rdr_byte_count,
                 "non-overlapping",
                 0);
#else
                 0,
                 rdr_byte_count,
                 "overlapping",
                 (int)ADAPTER_EIP202_CDR_BYTE_OFFSET);
#endif
    }

    // Initialize the CDR and RDR devices
    {
        zeroinit(cdr_settings);

#ifdef ADAPTER_EIP202_RING_MANUAL_CONFIGURE
        // Configure the EIP-202 Ring Managers with manually set
        // configuration parameters
        adapter_ring_eip202_configure(ADAPTER_EIP202_HOST_DATA_WIDTH,
                                      ADAPTER_EIP202_CF_SIZE,
                                      ADAPTER_EIP202_RF_SIZE,
                                      0,
                                      0,
                                      0);
#endif

        cdr_settings.f_atp        = (ADAPTER_EIP202_CDR_ATP_PRESENT > 0) ?
                                                                true : false;
        cdr_settings.f_at_pto_token = false;

        cdr_settings.params.data_bus_width_word_count = 1;

        // Not used for CDR
        cdr_settings.params.byte_swap_data_type_mask      = 0;
        cdr_settings.params.byte_swap_packet_mask        = 0;

        // Enable endianess conversion for the RDR master interface
        // if configured
#ifdef ADAPTER_EIP202_CDR_BYTE_SWAP_ENABLE
        cdr_settings.params.byte_swap_token_mask         =
        cdr_settings.params.byte_swap_descriptor_mask    =
                                            EIP202_RING_BYTE_SWAP_METHOD_32;
#else
        cdr_settings.params.byte_swap_token_mask         = 0;
        cdr_settings.params.byte_swap_descriptor_mask    = 0;
#endif

        cdr_settings.params.bufferability = 0;
#ifdef ADAPTER_EIP202_64BIT_DEVICE
        cdr_settings.params.dma_address_mode = EIP202_RING_64BIT_DMA_DSCR_PTR;
#else
        cdr_settings.params.dma_address_mode = EIP202_RING_64BIT_DMA_DISABLED;
#endif
        cdr_settings.params.ring_size_word_count =
                                        cd_offset * ADAPTER_EIP202_MAX_PACKETS;
        cdr_settings.params.ring_dma_handle = cdr_handle[interface_id];

#ifdef ADAPTER_EIP202_SEPARATE_RINGS
        if (dma_resource_translate(cdr_handle[interface_id], DMARES_DOMAIN_BUS,
                                  &phys_addr_pair) < 0)
        {
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
        adapter_addr_to_word_pair(phys_addr_pair.address_p, 0,
                               &cdr_settings.params.ring_dma_address.addr,
                               &cdr_settings.params.ring_dma_address.upper_addr);
#else
        if (dma_resource_translate(rdr_handle[interface_id], DMARES_DOMAIN_BUS,
                                  &phys_addr_pair) < 0)
        {
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
        adapter_addr_to_word_pair(phys_addr_pair.address_p,
                               ADAPTER_EIP202_CDR_BYTE_OFFSET,
                               &cdr_settings.params.ring_dma_address.addr,
                               &cdr_settings.params.ring_dma_address.upper_addr);
#endif

        cdr_settings.params.dscr_size_word_count = cd_word_count;
        cdr_settings.params.dscr_offs_word_count = cd_offset;

#ifndef ADAPTER_EIP202_AUTO_THRESH_DISABLE
        if(adapter_ring_eip202_configured)
        {
            u32 cd_size_rndup;
            int cfcount;

            // Use configuration parameters received via
            // the Ring 97 configuration (adapter_ring_eip202.h) interface
            if(cdr_settings.params.dscr_offs_word_count &
                    ((1 << adapter_ring_eip202_hdw) - 1))
            {
                LOG_CRIT("adapter_pec_dev_init: error, "
                         "command Descriptor offset %d"
                         " is not an integer multiple of Host data Width %d\n",
                         cdr_settings.params.dscr_offs_word_count,
                         adapter_ring_eip202_hdw);

                adapter_pec_dev_uninit(interface_id);
                return PEC_ERROR_INTERNAL;
            }

            // Round up to the next multiple of hdw words
            cd_size_rndup = (cdr_settings.params.dscr_offs_word_count +
                                ((1 << adapter_ring_eip202_hdw) - 1)) >>
                                         adapter_ring_eip202_hdw;

            // Half of number of full descriptors that fit fifo
            // Note: adapter_ring_eip202_cf_size is in hdw words
            cfcount = (1<<(adapter_ring_eip202_cf_size-1)) / cd_size_rndup;
            cfcount -= adapter_ring_eip202_nof_pes;

            // Check if command descriptor fits in fetch fifo
            if(cfcount <= 1)
                cfcount = 2; // does not fit, adjust the count

            // Note: cfcount must be also checked for not exceeding
            //       max DMA length

            // Convert to 32-bits word counts
            cdr_settings.params.dscr_fetch_size_word_count =
                (cfcount-1)* (cd_size_rndup << adapter_ring_eip202_hdw);
            cdr_settings.params.dscr_threshold_word_count =
                     cfcount * (cd_size_rndup << adapter_ring_eip202_hdw);
        }
        else
#endif // #ifndef ADAPTER_EIP202_AUTO_THRESH_DISABLE
        {
            // Use default static (user-defined) configuration parameters
#ifdef ADAPTER_EIP202_CDR_DSCR_FETCH_WORD_COUNT
            cdr_settings.params.dscr_fetch_size_word_count =
                                    ADAPTER_EIP202_CDR_DSCR_FETCH_WORD_COUNT;
#else
            cdr_settings.params.dscr_fetch_size_word_count = cd_offset;
#endif
            cdr_settings.params.dscr_threshold_word_count =
                                    ADAPTER_EIP202_CDR_DSCR_THRESH_WORD_COUNT;
        }

        LOG_CRIT("adapter_pec_dev_init: CDR fetch size %d, threshold %d, "
                 "hdw=%d, CFsize=%d\n",
                 cdr_settings.params.dscr_fetch_size_word_count,
                 cdr_settings.params.dscr_threshold_word_count,
                 adapter_ring_eip202_hdw,
                 adapter_ring_eip202_cf_size);

        // CDR Interrupts will be enabled via the Event Mgmt API functions
        cdr_settings.params.int_threshold_dscr_count = 0;
        cdr_settings.params.int_timeout_dscr_count = 0;
        if ((cdr_settings.params.dscr_fetch_size_word_count /
             cdr_settings.params.dscr_offs_word_count) >
            (cdr_settings.params.dscr_threshold_word_count /
             cdr_settings.params.dscr_size_word_count))
        {
            LOG_CRIT("adapter_pec_dev_init: CDR threshold lower than fetch size"
                     " incorrect setting\n");
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_BAD_PARAMETER;
        }


        LOG_INFO("\n\t\t\t eip202_cdr_init \n");

        res = eip202_cdr_init(cdr_io_area + interface_id,
                              cdr_device,
                              &cdr_settings);
        if (res != EIP202_RING_NO_ERROR)
        {
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
    }

    {
        zeroinit(rdr_settings);

        rdr_settings.params.data_bus_width_word_count = 1;

        // Not used for RDR
        rdr_settings.params.byte_swap_data_type_mask = 0;
        rdr_settings.params.byte_swap_token_mask = 0;

        rdr_settings.f_continuous_scatter = init_block_p->f_continuous_scatter;

        // Enable endianess conversion for the RDR master interface
        // if configured
        rdr_settings.params.byte_swap_packet_mask        = 0;
#ifdef ADAPTER_EIP202_RDR_BYTE_SWAP_ENABLE
        rdr_settings.params.byte_swap_descriptor_mask    =
                                            EIP202_RING_BYTE_SWAP_METHOD_32;
#else
        rdr_settings.params.byte_swap_descriptor_mask    = 0;
#endif

        rdr_settings.params.bufferability = 0;

#ifdef ADAPTER_EIP202_64BIT_DEVICE
        rdr_settings.params.dma_address_mode = EIP202_RING_64BIT_DMA_DSCR_PTR;
#else
        rdr_settings.params.dma_address_mode = EIP202_RING_64BIT_DMA_DISABLED;
#endif

        rdr_settings.params.ring_size_word_count =
                                    rd_offset * ADAPTER_EIP202_MAX_PACKETS;

        rdr_settings.params.ring_dma_handle = rdr_handle[interface_id];

        if (dma_resource_translate(rdr_handle[interface_id], DMARES_DOMAIN_BUS,
                                  &phys_addr_pair) < 0)
        {
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
        adapter_addr_to_word_pair(phys_addr_pair.address_p, 0,
                               &rdr_settings.params.ring_dma_address.addr,
                               &rdr_settings.params.ring_dma_address.upper_addr);

        rdr_settings.params.dscr_size_word_count = rd_word_count;
        rdr_settings.params.dscr_offs_word_count = rd_offset;
        rdr_settings.params.token_offset_word_count = rd_token_offs_word_count;

#ifndef ADAPTER_EIP202_AUTO_THRESH_DISABLE
        if(adapter_ring_eip202_configured)
        {
            u32 rd_size_rndup;
            int rfcount;

            // Use configuration parameters received via
            // the Ring 97 configuration (adapter_ring_eip202.h) interface
            if(rdr_settings.params.dscr_offs_word_count &
                    ((1 << adapter_ring_eip202_hdw) - 1))
            {
                LOG_CRIT("adapter_pec_dev_init: error, "
                         "Result Descriptor offset %d"
                         " is not an integer multiple of Host data Width %d\n",
                         rdr_settings.params.dscr_offs_word_count,
                         adapter_ring_eip202_hdw);

                adapter_pec_dev_uninit(interface_id);
                return PEC_ERROR_INTERNAL;
            }

            // Round up to the next multiple of hdw words
            rd_size_rndup = (rdr_settings.params.dscr_offs_word_count +
                                ((1 << adapter_ring_eip202_hdw) - 1)) >>
                                         adapter_ring_eip202_hdw;

            // Half of number of full descriptors that fit fifo
            // Note: adapter_ring_eip202_rf_size is in hdw words
            rfcount = (1 << (adapter_ring_eip202_rf_size - 1)) / rd_size_rndup;

            rfcount -= adapter_ring_eip202_nof_pes;

            // Check if prepared result descriptor fits in fetch fifo
            if(rfcount <= 1)
                rfcount = 2; // does not fit, adjust the count

            // Note: rfcount must be also checked for not exceeding
            //       max DMA length

            // Convert to 32-bit words counts
            rdr_settings.params.dscr_fetch_size_word_count =
                (rfcount - 1) * (rd_size_rndup << adapter_ring_eip202_hdw);
            rdr_settings.params.dscr_threshold_word_count =
                     rfcount * (rd_size_rndup << adapter_ring_eip202_hdw);
        }
        else
#endif // #ifndef ADAPTER_EIP202_AUTO_THRESH_DISABLE
        {
            // Use default static (user-defined) configuration parameters
            rdr_settings.params.dscr_fetch_size_word_count =
                                    ADAPTER_EIP202_RDR_DSCR_FETCH_WORD_COUNT;
            rdr_settings.params.dscr_threshold_word_count =
                                    ADAPTER_EIP202_RDR_DSCR_THRESH_WORD_COUNT;
        }

        LOG_CRIT("adapter_pec_dev_init: RDR fetch size %d, threshold %d, "
                 "RFsize=%d\n",
                 rdr_settings.params.dscr_fetch_size_word_count,
                 rdr_settings.params.dscr_threshold_word_count,
                 adapter_ring_eip202_rf_size);

        // RDR Interrupts will be enabled via the Event Mgmt API functions
        rdr_settings.params.int_threshold_dscr_count = 0;
        rdr_settings.params.int_timeout_dscr_count = 0;

        if ((rdr_settings.params.dscr_fetch_size_word_count /
             rdr_settings.params.dscr_offs_word_count) >
            (rdr_settings.params.dscr_threshold_word_count /
#ifdef ADAPTER_EIP202_64BIT_DEVICE
             4 /* RDR prepared descriptor size for 64-bit */
#else
             2 /* RDR prepared descriptor size for 32-bit */
#endif
                ))
        {
            LOG_CRIT("adapter_pec_dev_init: RDR threshold lower than fetch size"
                     " incorrect setting\n");
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_BAD_PARAMETER;
        }
        LOG_INFO("\n\t\t\t eip202_rdr_init \n");

        if (adapter_ring_eip202_hdw == 3)
        {
            unsigned int version = (adapter_ring_eip202_major_version << 16) |
                (adapter_ring_eip202_minor_version << 8) | adapter_ring_eip202_patch_level;
            LOG_INFO("EIP202 Version = 0x%06x thr words %d\n",version,rdr_settings.params.dscr_threshold_word_count);
            if (version < 0x020803) {
                /* Work-around for some hardware configurations with 256-bit bus */
                LOG_INFO("Work-around required for hdw=3 and EIP202 ver <2.8.3\n");
                rdr_settings.params.dscr_threshold_word_count <<= 1;
            }
        }
        res = eip202_rdr_init(rdr_io_area + interface_id,
                              rdr_device,
                              &rdr_settings);
        if (res != EIP202_RING_NO_ERROR)
        {
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
    }

    if (!adapter_lib_rd_out_tokens_alloc(interface_id))
    {
        LOG_CRIT("adapter_pec_dev_init: failed to allocate output tokens\n");
        adapter_pec_dev_uninit(interface_id);
        return PEC_ERROR_INTERNAL; // Out of memory
    }

#ifdef ADAPTER_AUTO_TOKENBUILDER
    {
        // Allocate buffer for locally generated tokens.
        int dmares;
        unsigned int tb_byte_count = 4 * ADAPTER_EIP202_MAX_PACKETS *
            ADAPTER_AUTO_TOKEN_MAX_WORD_COUNT;

        dma_resource_properties_t tb_properties;
        dma_resource_addr_pair_t tb_host_addr;

        // used as u32 array
        tb_properties.alignment = adapter_lib_ring_eip202_dma_alignment_get();
        tb_properties.bank      = ADAPTER_EIP202_BANK_RING;
        tb_properties.f_cached   = false;
        tb_properties.size      = tb_byte_count;

        dmares = dma_resource_alloc(tb_properties,
                                   &tb_host_addr,
                                   token_buf_handle + interface_id);
        if (dmares != 0)
        {
            LOG_CRIT("adapter_pec_dev_init: failed to allocate token buffer\n");
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
#ifdef ADAPTER_EIP202_ARMRING_ENABLE_SWAP
        dma_resource_swap_endianness_set(token_buf_handle[interface_id],true);
#endif
        token_buf_host_addr[interface_id] = tb_host_addr.address_p;
        token_buf_index[interface_id] = 0;
        if (dma_resource_translate(token_buf_handle[interface_id], DMARES_DOMAIN_BUS,
                                  &phys_addr_pair) < 0)
        {
            adapter_pec_dev_uninit(interface_id);
            return PEC_ERROR_INTERNAL;
        }
        token_buf_phys_addr[interface_id] = (uintptr_t)phys_addr_pair.address_p;
        LOG_INFO("Allocated Token Buffer of size %u, hostaddr=%p physaddr=%p\n",
                 tb_byte_count, tb_host_addr.address_p, phys_addr_pair.address_p);
    }
#endif

    adapter_lib_ring_eip202_status_report(interface_id);

#ifdef ADAPTER_EIP202_ADD_INIT_DIAGNOSTICS
    adapter_register_dump();
#endif

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_uninit
 */
pec_status_t
adapter_pec_dev_uninit(
        const unsigned int interface_id)
{
    device_handle_t cdr_device, rdr_device;

    LOG_INFO("\n\t\t adapter_pec_dev_uninit \n");

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    adapter_lib_ring_eip202_status_report(interface_id);

#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
    {
        // Make a last attempt to get rid of any remaining result descriptors
        // belonging to unused scatter particles.
        u32 dscr_done_count,dscr_count;

        LOG_INFO("\n\t\t\t eip202_rdr_descriptor_get \n");

        eip202_rdr_descriptor_get(rdr_io_area + interface_id,
                                 eip202_rdr_entries[interface_id],
                                 ADAPTER_EIP202_MAX_LOGICDESCR,
                                 ADAPTER_EIP202_MAX_LOGICDESCR,
                                 &dscr_done_count,
                                 &dscr_count);
    }
#endif

    adapter_lib_rd_out_tokens_free(interface_id);

#ifdef ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
    {
        // Reset the trc base address to 0.
        device_handle_t device;

        device = eip_device_find(ADAPTER_EIP202_GLOBAL_DEVICE_NAME);
        if (device == NULL)
        {
            LOG_CRIT("adapter_pec_dev_uninit: Could not find device\n");
            return PEC_ERROR_INTERNAL;
        }

        LOG_INFO("\n\t\t\t EIP207_RC_BaseAddr_Set \n");

        EIP207_RC_BaseAddr_Set(
            device,
            EIP207_TRC_REG_BASE,
            EIP207_RC_SET_NR_DEFAULT,
            0,
            0);
    }
#endif // ADAPTER_EIP202_RC_DMA_BANK_SUPPORT

    cdr_device = eip_device_find(eip202_devices[interface_id].cdr_device_name_p);
    rdr_device = eip_device_find(eip202_devices[interface_id].rdr_device_name_p);

    if (cdr_device == NULL || rdr_device == NULL)
        return PEC_ERROR_INTERNAL;

    LOG_INFO("\n\t\t\t eip202_cdr_reset \n");

    eip202_cdr_reset(cdr_io_area + interface_id,
                    cdr_device);

    LOG_INFO("\n\t\t\t eip202_rdr_reset \n");

    eip202_rdr_reset(rdr_io_area + interface_id,
                    rdr_device);

    if (rdr_handle[interface_id] != NULL)
    {
        dma_resource_release(rdr_handle[interface_id]);
        rdr_handle[interface_id] = NULL;
    }

    if (cdr_handle[interface_id] != NULL)
    {
        dma_resource_release(cdr_handle[interface_id]);
        cdr_handle[interface_id] = NULL;
    }

#ifdef ADAPTER_AUTO_TOKENBUILDER
    if (token_buf_handle[interface_id] != NULL)
    {
        dma_resource_release(token_buf_handle[interface_id]);
        token_buf_handle[interface_id] = NULL;
        token_buf_host_addr[interface_id] = NULL;
        token_buf_phys_addr[interface_id] = 0;
        token_buf_index[interface_id] = 0;
    }
#endif

#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
    adapter_interrupt_set_handler(eip202_devices[interface_id].RDR_IRQ_ID, NULL);
    adapter_interrupt_set_handler(eip202_devices[interface_id].CDR_IRQ_ID, NULL);
#endif

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_resume
 */
int
adapter_pec_dev_resume(
        const unsigned int interface_id)
{
    eip202_ring_error_t res;

    LOG_INFO("\n\t\t %s \n", __func__);

    LOG_INFO("\n\t\t\t eip202_cdr_init \n");
    // Restore EIP-202 CDR
    res = eip202_cdr_init(cdr_io_area + interface_id,
                          eip_device_find(eip202_devices[interface_id].cdr_device_name_p),
                          &cdr_settings);
    if (res != EIP202_RING_NO_ERROR)
    {
        LOG_CRIT("%s: eip202_cdr_init() error %d", __func__, res);
        return -1;
    }

    LOG_INFO("\n\t\t\t eip202_rdr_init \n");
    // Restore EIP-202 RDR
    res = eip202_rdr_init(rdr_io_area + interface_id,
                          eip_device_find(eip202_devices[interface_id].rdr_device_name_p),
                          &rdr_settings);
    if (res != EIP202_RING_NO_ERROR)
    {
        LOG_CRIT("%s: eip202_cdr_init() error %d", __func__, res);
        return -2;
    }

#ifdef ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE
#ifdef ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
    LOG_INFO("\n\t\t\t EIP207_RC_BaseAddr_Set \n");
    // Restore EIP-207 Record Cache base address
    EIP207_RC_BaseAddr_Set(
        eip_device_find(ADAPTER_EIP202_GLOBAL_DEVICE_NAME),
        EIP207_TRC_REG_BASE,
        EIP207_RC_SET_NR_DEFAULT,
        eip202_sa_base_addr,
        eip202_sa_base_upper_addr);
#endif // ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
#endif // ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE

#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
    // Restore RDR interrupt
    if (eip202_interrupts[interface_id] & BIT_0)
        adapter_pec_dev_enable_result_irq(interface_id);

    // Restore CDR interrupt
    if (eip202_interrupts[interface_id] & BIT_1)
        adapter_pec_dev_enable_command_irq(interface_id);
#endif // ADAPTER_EIP202_INTERRUPTS_ENABLE

    return 0; // success
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_suspend
 */
int
adapter_pec_dev_suspend(
        const unsigned int interface_id)
{
    LOG_INFO("\n\t\t %s \n", __func__);

#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
    // Disable RDR interrupt
    if (eip202_interrupts[interface_id] & BIT_0)
    {
        adapter_pec_dev_disable_result_irq(interface_id);

        // Remember that interrupt was enabled
        eip202_interrupts[interface_id] |= BIT_0;
    }

    // Disable CDR interrupt
    if (eip202_interrupts[interface_id] & BIT_1)
    {
        adapter_pec_dev_disable_command_irq(interface_id);

        // Remember that interrupt was enabled
        eip202_interrupts[interface_id] |= BIT_1;
    }
#endif // ADAPTER_EIP202_INTERRUPTS_ENABLE

#ifdef ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
    {
        LOG_INFO("\n\t\t\t EIP207_RC_BaseAddr_Set \n");

        // Reset the trc base address to 0
        EIP207_RC_BaseAddr_Set(
            eip_device_find(ADAPTER_EIP202_GLOBAL_DEVICE_NAME),
            EIP207_TRC_REG_BASE,
            EIP207_RC_SET_NR_DEFAULT,
            0,
            0);
    }
#endif // ADAPTER_EIP202_RC_DMA_BANK_SUPPORT

    LOG_INFO("\n\t\t\t eip202_cdr_reset \n");

    eip202_cdr_reset(cdr_io_area + interface_id,
                     eip_device_find(eip202_devices[interface_id].cdr_device_name_p));

    LOG_INFO("\n\t\t\t eip202_rdr_reset \n");

    eip202_rdr_reset(rdr_io_area + interface_id,
                     eip_device_find(eip202_devices[interface_id].rdr_device_name_p));

    return 0; // success
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_sa_prepare
 */
pec_status_t
adapter_pec_dev_sa_prepare(
        const dma_buf_handle_t sa_handle,
        const dma_buf_handle_t state_handle,
        const dma_buf_handle_t arc4_handle)
{
    dma_resource_handle_t sa_dma_handle;

    IDENTIFIER_NOT_USED(state_handle.p);
    IDENTIFIER_NOT_USED(arc4_handle.p);

    if (dma_buf_handle_is_same(&sa_handle, &dma_buf_null_handle))
        return PEC_ERROR_BAD_PARAMETER;
    else
    {
        dma_resource_record_t * rec_p;

        sa_dma_handle = adapter_dma_buf_handle2_dma_resource_handle(sa_handle);
        rec_p = dma_resource_handle2_record_ptr(sa_dma_handle);

        if (rec_p == NULL)
            return PEC_ERROR_INTERNAL;

#ifdef ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE
        if (rec_p->props.bank != ADAPTER_EIP202_BANK_SA)
        {
            LOG_CRIT("pec_sa_register: Invalid bank for SA\n");
            return PEC_ERROR_BAD_PARAMETER;
        }
#endif

#ifndef ADAPTER_EIP202_USE_LARGE_TRANSFORM_DISABLE
        {
            u32 first_word = dma_resource_read32(sa_dma_handle, 0);
            // Register in the DMA resource record whether the transform
            // is large.
            if ( (first_word & ADAPTER_EIP202_TR_ISLARGE) != 0)
            {
                rec_p->f_is_large_transform = true;
                dma_resource_write32(sa_dma_handle,
                                    0,
                                    first_word & ~ADAPTER_EIP202_TR_ISLARGE);
                // Clear that bit in the SA record itself.
            }
            else
            {
                rec_p->f_is_large_transform = false;
            }
        }
#endif

        rec_p->f_is_new_sa = true;
#ifdef ADAPTER_AUTO_TOKENBUILDER
        {
            u32 * host_addr = adapter_dma_resource_host_addr(sa_dma_handle);
            unsigned int ctx_offs = rec_p->f_is_large_transform ? ADAPTER_LARGE_RECORD_WORD_COUNT : ADAPTER_RECORD_WORD_COUNT;
            rec_p->context_p = (void*)&host_addr[ctx_offs];
#ifdef ADAPTER_EIP202_ARMRING_ENABLE_SWAP
            /* SA record was byte-swapped as required, but Context Record part
               must not be swapped, so swap it back here. */
            dma_resource_write32_array(
                sa_dma_handle,
                ctx_offs,
                rec_p->props.size/4 - ctx_offs,
                (u32*)rec_p->context_p);
#endif
        }
#endif
    }

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_sa_remove
 */
pec_status_t
adapter_pec_dev_sa_remove(
        const dma_buf_handle_t sa_handle,
        const dma_buf_handle_t state_handle,
        const dma_buf_handle_t arc4_handle)
{
    IDENTIFIER_NOT_USED(state_handle.p);
    IDENTIFIER_NOT_USED(arc4_handle.p);

    if (dma_buf_handle_is_same(&sa_handle, &dma_buf_null_handle))
        return PEC_ERROR_BAD_PARAMETER;

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_get_free_space
 */
unsigned int
adapter_pec_dev_get_free_space(
        const unsigned int interface_id)
{
    unsigned int free_cdr, free_rdr, filled_cdr, filled_rdr;
    eip202_ring_error_t res;

    LOG_INFO("\n\t\t adapter_pec_dev_get_free_space \n");

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    LOG_INFO("\n\t\t\t eip202_cdr_fill_level_get \n");

    res = eip202_cdr_fill_level_get(cdr_io_area + interface_id,
                                  &filled_cdr);
    if (res != EIP202_RING_NO_ERROR)
        return 0;

    if (filled_cdr > ADAPTER_EIP202_MAX_PACKETS)
        return 0;

    free_cdr = ADAPTER_EIP202_MAX_PACKETS - filled_cdr;

    if (eip202_continuous_scatter[interface_id])
    {
        return free_cdr;
    }
    else
    {
        LOG_INFO("\n\t\t\t eip202_rdr_fill_level_get \n");

        res = eip202_rdr_fill_level_get(rdr_io_area + interface_id,
                                       &filled_rdr);
        if (res != EIP202_RING_NO_ERROR)
            return 0;

        if (filled_rdr > ADAPTER_EIP202_MAX_PACKETS)
            return 0;

        free_rdr = ADAPTER_EIP202_MAX_PACKETS - filled_rdr;

        return MIN(free_cdr, free_rdr);
    }
}


/*----------------------------------------------------------------------------
 * Adapter_PECDev_PacketPut
 */
pec_status_t
adapter_pec_dev_packet_put(
        const unsigned int interface_id,
        const pec_command_descriptor_t * commands_p,
        const unsigned int commands_count,
        unsigned int * const put_count_p)
{
    unsigned int cmd_lp;
#ifdef ADAPTER_EIP202_STRICT_ARGS
    unsigned int free_cdr,free_rdr;
#endif
    unsigned int filled_cdr, filled_rdr, cdr_index=0, rdr_index=0;
    unsigned int submitted = 0;
    eip202_ring_error_t res;
    eip202_cdr_control_t cdr_control;
    eip202_rdr_prepared_control_t rdr_control;

    LOG_INFO("\n\t\t adapter_pec_dev_packet_put \n");

    *put_count_p = 0;
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

#ifdef ADAPTER_EIP202_STRICT_ARGS
    LOG_INFO("\n\t\t\t eip202_cdr_fill_level_get \n");

    res = eip202_cdr_fill_level_get(cdr_io_area + interface_id,
                                  &filled_cdr);
    if (res != EIP202_RING_NO_ERROR)
        return PEC_ERROR_INTERNAL;

    if (filled_cdr > ADAPTER_EIP202_MAX_PACKETS)
        return PEC_ERROR_INTERNAL;

    free_cdr = ADAPTER_EIP202_MAX_PACKETS - filled_cdr;

    LOG_INFO("\n\t\t\t eip202_rdr_fill_level_get \n");

    if (!eip202_continuous_scatter[interface_id])
    {
        res = eip202_rdr_fill_level_get(rdr_io_area + interface_id,
                                       &filled_rdr);
        if (res != EIP202_RING_NO_ERROR)
            return PEC_ERROR_INTERNAL;

        if (filled_rdr > ADAPTER_EIP202_MAX_PACKETS)
        return PEC_ERROR_INTERNAL;

        free_rdr = ADAPTER_EIP202_MAX_PACKETS - filled_rdr;
        free_rdr = MIN(ADAPTER_EIP202_MAX_LOGICDESCR, free_rdr);
    }
    else
    {
        free_rdr = 1;
    }
    free_cdr = MIN(ADAPTER_EIP202_MAX_LOGICDESCR, free_cdr);
#endif

    for (cmd_lp = 0; cmd_lp < commands_count; cmd_lp++)
    {
        u8 token_word_count;
        eip202_device_address_t sa_phys_addr;

#ifdef ADAPTER_EIP202_STRICT_ARGS
        if (cdr_index == free_cdr || (!eip202_continuous_scatter[interface_id] && rdr_index == free_rdr))
            break; // Run out of free descriptors in any of the rings.
#endif

        if (!commands_p[cmd_lp].input_token_p)
        {
            LOG_CRIT("adapter_pec_dev_packet_put: failed, missing input token "
                     "for command descriptor %d\n",
                     cmd_lp);
            return PEC_ERROR_BAD_PARAMETER;
        }

        // Prepare (first) descriptor, except for source pointer/size.
        eip202_cdr_entries[interface_id][cdr_index].src_packet_byte_count =
                                            commands_p[cmd_lp].src_pkt_byte_count;

        if (dma_buf_handle_is_same(&commands_p[cmd_lp].token_handle,
                                 &dma_buf_null_handle))
        {
#ifdef ADAPTER_AUTO_TOKENBUILDER
            {
                /* If no instruction token was passed, try to
                   build one inside the driver. */
                dma_resource_handle_t dma_handle =
                adapter_dma_buf_handle2_dma_resource_handle(
                                        commands_p[cmd_lp].sa_handle1);
                dma_resource_record_t * rec_p;
                void  *context_p;
                int rc;
                if (dma_handle == NULL)
                    rec_p = NULL;
                else
                    rec_p = dma_resource_handle2_record_ptr(dma_handle);
                if (rec_p == NULL)
                {
                    context_p = NULL;
                }
                else
                {
                    context_p = rec_p->context_p;
                }

                rc = io_token_token_convert(
                    commands_p[cmd_lp].input_token_p,
                    commands_p[cmd_lp].src_pkt_handle,
                    context_p,
                    token_buf_host_addr[interface_id]+token_buf_index[interface_id]*ADAPTER_AUTO_TOKEN_MAX_WORD_COUNT);
                if (rc < 0)
                {
                    LOG_CRIT("Unsupported packet processing operation\n");
                    return PEC_ERROR_INTERNAL;
                }
                token_word_count = (u8)rc;
            }
#else
            token_word_count = 0;
#endif
        }
        else
        {
            // Look-aside use case. token is created by the caller
            if (commands_p[cmd_lp].token_word_count > 255)
                return PEC_ERROR_INTERNAL;

            token_word_count = (u8)commands_p[cmd_lp].token_word_count;
        }

        cdr_control.token_word_count = token_word_count;

        adapter_get_phys_addr(commands_p[cmd_lp].token_handle,
              &(eip202_cdr_entries[interface_id][cdr_index].token_data_addr));
#ifdef ADAPTER_AUTO_TOKENBUILDER
        if (token_word_count > 0 &&
            !dma_resource_is_valid_handle(
                adapter_dma_buf_handle2_dma_resource_handle((commands_p[cmd_lp].token_handle))))
        {
            /* Token handler was not passed by application, use driver's
               own token buffer */
            adapter_addr_to_word_pair(
                (void*)token_buf_phys_addr[interface_id],
                token_buf_index[interface_id] * 4 *ADAPTER_AUTO_TOKEN_MAX_WORD_COUNT,
                &eip202_cdr_entries[interface_id][cdr_index].token_data_addr.addr,
                &eip202_cdr_entries[interface_id][cdr_index].token_data_addr.upper_addr);
#ifdef ADAPTER_EIP202_ARMRING_ENABLE_SWAP
            /* byte-swap the instruction token if needed */
            dma_resource_write32_array(
                token_buf_handle[interface_id],
                token_buf_index[interface_id]*ADAPTER_AUTO_TOKEN_MAX_WORD_COUNT,
                token_word_count,
                token_buf_host_addr[interface_id]+token_buf_index[interface_id]*ADAPTER_AUTO_TOKEN_MAX_WORD_COUNT);
#endif
            /* Any required cache flushing for this token. */
            dma_resource_pre_dma(
                token_buf_handle[interface_id],
                4 * token_buf_index[interface_id]*ADAPTER_AUTO_TOKEN_MAX_WORD_COUNT,
                4 * token_word_count);

            /* Increment index in ring buffer */
            token_buf_index[interface_id]++;
            if (token_buf_index[interface_id] == ADAPTER_EIP202_MAX_PACKETS)
                token_buf_index[interface_id] = 0;
        }
#endif
        if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                &eip202_cdr_entries[interface_id][cdr_index].token_data_addr,
                "token buffer"))
            return PEC_ERROR_INTERNAL;

        cdr_control.f_first_segment = true;
        cdr_control.f_last_segment  = false;
        cdr_control.f_force_engine = (commands_p[cmd_lp].control2 & BIT_5) != 0;
        cdr_control.engine_id = commands_p[cmd_lp].control2 & MASK_5_BITS;

        eip202_cdr_entries[interface_id][cdr_index].token_p =
                                            commands_p[cmd_lp].input_token_p;

        adapter_get_phys_addr(commands_p[cmd_lp].sa_handle1, &sa_phys_addr);

        if (sa_phys_addr.addr != ADAPTER_EIP202_DUMMY_DMA_ADDRESS_LO ||
            sa_phys_addr.upper_addr != ADAPTER_EIP202_DUMMY_DMA_ADDRESS_HI)
        {
            dma_resource_handle_t dma_handle =
                adapter_dma_buf_handle2_dma_resource_handle(
                                        commands_p[cmd_lp].sa_handle1);
            dma_resource_record_t * rec_p =
                                    dma_resource_handle2_record_ptr(dma_handle);
            if (rec_p == NULL)
                return PEC_ERROR_INTERNAL;

#ifdef ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE
            if (rec_p->props.bank != ADAPTER_EIP202_BANK_SA)
            {
                LOG_CRIT("pec_packet_put: Invalid bank for SA\n");
                return PEC_ERROR_BAD_PARAMETER;
            }
#endif

            if (io_token_sa_reuse_update(!rec_p->f_is_new_sa,
                                       commands_p[cmd_lp].input_token_p) < 0)
                return PEC_ERROR_INTERNAL;

            if (rec_p->f_is_new_sa)
                rec_p->f_is_new_sa = false;
        }

#ifdef ADAPTER_EIP202_USE_POINTER_TYPES
        if (sa_phys_addr.addr != ADAPTER_EIP202_DUMMY_DMA_ADDRESS_LO ||
            sa_phys_addr.upper_addr != ADAPTER_EIP202_DUMMY_DMA_ADDRESS_HI)
        {
#ifndef ADAPTER_EIP202_USE_LARGE_TRANSFORM_DISABLE
            dma_resource_handle_t dma_handle =
                adapter_dma_buf_handle2_dma_resource_handle(
                                        commands_p[cmd_lp].sa_handle1);
            dma_resource_record_t * rec_p =
                            dma_resource_handle2_record_ptr(dma_handle);

            if (rec_p->f_is_large_transform)
                sa_phys_addr.addr |= ADAPTER_EIP202_TR_LARGE_ADDRESS;
            else
#endif
                sa_phys_addr.addr |= ADAPTER_EIP202_TR_ADDRESS;
        }
#endif

#ifdef ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE
#ifdef ADAPTER_EIP202_RC_DMA_BANK_SUPPORT
        if (sa_phys_addr.addr != ADAPTER_EIP202_DUMMY_DMA_ADDRESS ||
            sa_phys_addr.upper_addr != ADAPTER_EIP202_DUMMY_DMA_ADDRESS_HI)
            sa_phys_addr.addr -= eip202_sa_base_addr;

        sa_phys_addr.upper_addr = 0;
#endif // ADAPTER_EIP202_RC_DMA_BANKS_SUPPORT
#endif // ADAPTER_EIP202_DMARESOURCE_BANKS_ENABLE

        eip202_cdr_entries[interface_id][cdr_index].context_data_addr = sa_phys_addr;

        if (!adapter_lib_ring_eip202_dma_addr_is_sane(&sa_phys_addr, "SA buffer"))
            return PEC_ERROR_INTERNAL;

        {
            io_token_phys_addr_t tkn_pa;

            tkn_pa.lo = sa_phys_addr.addr;
            tkn_pa.hi = sa_phys_addr.upper_addr;

            if (io_token_sa_addr_update(&tkn_pa,
                                      commands_p[cmd_lp].input_token_p) < 0)
                return PEC_ERROR_INTERNAL;
        }

        rdr_control.f_first_segment           = true;
        rdr_control.f_last_segment            = false;
        rdr_control.expected_result_word_count = 0;

#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
        {
            unsigned int gather_particles;
            unsigned int scatter_particles;
            unsigned int required_cdr, required_rdr;
            unsigned int i;
            unsigned int gather_byte_count;

            pec_sg_list_get_capacity(commands_p[cmd_lp].src_pkt_handle,
                                   &gather_particles);
            if (eip202_continuous_scatter[interface_id])
                scatter_particles = 1;
            else
                pec_sg_list_get_capacity(commands_p[cmd_lp].dst_pkt_handle,
                                       &scatter_particles);

            if (gather_particles == 0)
                required_cdr = 1;
            else
                required_cdr = gather_particles;

            if (scatter_particles == 0)
                required_rdr = 1;
            else
                required_rdr = scatter_particles;

#ifndef ADAPTER_EIP202_SEPARATE_RINGS
            // If using overlapping rings, require an equal number of CDR
            // and RDR entries for the packet, the maximum of both.
            required_cdr = MAX(required_cdr,required_rdr);
            required_rdr = required_cdr;
#endif
            /* Check whether it will fit into the rings and the
             * prepared descriptor arrays.*/
#ifdef ADAPTER_EIP202_STRICT_ARGS
            if (cdr_index + required_cdr > free_cdr ||
                rdr_index + required_rdr > free_rdr)
                break;
#endif

            if (gather_particles > 0)
            {
                gather_byte_count = commands_p[cmd_lp].src_pkt_byte_count;
                for (i=0; i<gather_particles; i++)
                {
                    dma_buf_handle_t particle_handle;
                    u8 * dummy_ptr;
                    unsigned int particle_size;

                    pec_sg_list_read(commands_p[cmd_lp].src_pkt_handle,
                                    i,
                                    &particle_handle,
                                    &particle_size,
                                    &dummy_ptr);

                    adapter_get_phys_addr(particle_handle,
                      &(eip202_cdr_entries[interface_id][cdr_index+i].src_packet_addr));

                    if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                            &eip202_cdr_entries[interface_id][cdr_index+i].src_packet_addr,
                            "source packet buffer"))
                        return PEC_ERROR_INTERNAL;

                    if (particle_size > gather_byte_count)
                        particle_size = gather_byte_count;
                    gather_byte_count -= particle_size;
                    // limit the total size of the gather particles to the
                    // actual packet length.

                    cdr_control.f_last_segment = (required_cdr == i + 1);
                    cdr_control.segment_byte_count = particle_size;
                    eip202_cdr_entries[interface_id][cdr_index+i].control_word =
                        eip202_cdr_write_control_word(&cdr_control);
                    cdr_control.f_first_segment = false;
                    cdr_control.token_word_count = 0;
                }
            }
            else
            { /* No gather, use single source buffer */

                adapter_get_phys_addr(commands_p[cmd_lp].src_pkt_handle,
                 &(eip202_cdr_entries[interface_id][cdr_index].src_packet_addr));

                if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                        &eip202_cdr_entries[interface_id][cdr_index].src_packet_addr,
                        "source packet buffer"))
                    return PEC_ERROR_INTERNAL;

                cdr_control.f_last_segment = (required_cdr == 1);
                cdr_control.segment_byte_count =
                    commands_p[cmd_lp].src_pkt_byte_count;
                eip202_cdr_entries[interface_id][cdr_index].control_word =
                    eip202_cdr_write_control_word(&cdr_control);

                cdr_control.f_first_segment = false;
                cdr_control.token_word_count = 0;
                i = 1;
            }

            /* Add any dummy segments for overlapping rings */
            for ( ; i<required_cdr; i++)
            {

                cdr_control.f_last_segment = (required_cdr == i + 1);
                cdr_control.segment_byte_count = 0;
                eip202_cdr_entries[interface_id][cdr_index+i].control_word =
                    eip202_cdr_write_control_word(&cdr_control);
            }

            if (!eip202_continuous_scatter[interface_id])
            {
                if (scatter_particles > 0)
                {
                    for (i=0; i<scatter_particles; i++)
                    {
                        dma_buf_handle_t particle_handle;
                        u8 * dummy_ptr;
                        unsigned int particle_size;

                        pec_sg_list_read(commands_p[cmd_lp].dst_pkt_handle,
                                        i,
                                        &particle_handle,
                                        &particle_size,
                                        &dummy_ptr);

                        adapter_get_phys_addr(particle_handle,
                                            &(eip202_rdr_prepared[interface_id][rdr_index+i].dst_packet_addr));

                        if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                                &eip202_rdr_prepared[interface_id][rdr_index+i].dst_packet_addr,
                                "destination packet buffer"))
                            return PEC_ERROR_INTERNAL;

                        rdr_control.f_last_segment = (required_rdr == i + 1);
                        rdr_control.prep_segment_byte_count = particle_size;
                        eip202_rdr_prepared[interface_id][rdr_index+i].prep_control_word =
                            eip202_rdr_write_prepared_control_word(&rdr_control);

                        rdr_control.f_first_segment = false;
                    }
                }
                else
                { /* No scatter, use single destination buffer */
                    dma_resource_handle_t *dma_handle =
                    adapter_dma_buf_handle2_dma_resource_handle(
                        commands_p[cmd_lp].dst_pkt_handle);
                    dma_resource_record_t * rec_p;

                    if (dma_resource_is_valid_handle(dma_handle))
                        rec_p  = dma_resource_handle2_record_ptr(dma_handle);
                    else
                        rec_p = NULL;

                    // Check if NULL packet pointers are allowed
                    // for record invalidation commands
#ifndef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
                    if (rec_p == NULL)
                        return PEC_ERROR_INTERNAL;
#endif

                    adapter_get_phys_addr(commands_p[cmd_lp].dst_pkt_handle,
                                        &(eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr));

                    if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                      &eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr,
                      "destination packet buffer"))
                    return PEC_ERROR_INTERNAL;

                    rdr_control.f_last_segment = (required_rdr==1);

#ifdef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
                    // For NULL packet pointers only, record cache invalidation
                    if (rec_p == NULL)
                        rdr_control.prep_segment_byte_count = 0;
                    else
#endif
                        rdr_control.prep_segment_byte_count = rec_p->props.size;

                    eip202_rdr_prepared[interface_id][rdr_index].prep_control_word =
                        eip202_rdr_write_prepared_control_word(&rdr_control);

                    rdr_control.f_first_segment = false;
                    i = 1;
                }

                /* Add any dummy segments for overlapping rings */
                for ( ; i<required_rdr; i++)
                {
                    rdr_control.f_last_segment = (required_rdr == i + 1);
                    rdr_control.prep_segment_byte_count = 0;
                    eip202_rdr_prepared[interface_id][rdr_index+i].prep_control_word =
                        eip202_rdr_write_prepared_control_word(&rdr_control);
                }
                rdr_index += required_rdr;
            }
            cdr_index += required_cdr;
        }
#else
        {
            // Prepare source and destination buffer in non-sg case.
            dma_resource_handle_t *dma_handle =
                adapter_dma_buf_handle2_dma_resource_handle(
                    commands_p[cmd_lp].dst_pkt_handle);
            dma_resource_record_t * rec_p;

            if (dma_resource_is_valid_handle(dma_handle))
                rec_p  = dma_resource_handle2_record_ptr(dma_handle);
            else
                rec_p = NULL;

            // Check if NULL packet pointers are allowed
            // for record invalidation commands
#ifndef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
            if (rec_p == NULL)
                return PEC_ERROR_INTERNAL;
#endif

            adapter_get_phys_addr(commands_p[cmd_lp].src_pkt_handle,
                 &(eip202_cdr_entries[interface_id][cdr_index].src_packet_addr));

            if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                  &eip202_cdr_entries[interface_id][cdr_index].src_packet_addr,
                  "source packet buffer"))
                return PEC_ERROR_INTERNAL;

            cdr_control.f_last_segment     = true;
            cdr_control.segment_byte_count = commands_p[cmd_lp].src_pkt_byte_count;

            eip202_cdr_entries[interface_id][cdr_index].control_word =
                                eip202_cdr_write_control_word(&cdr_control);

            if (!eip202_continuous_scatter[interface_id])
            {
                adapter_get_phys_addr(commands_p[cmd_lp].dst_pkt_handle,
                                    &(eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr));

                if (!adapter_lib_ring_eip202_dma_addr_is_sane(
                        &eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr,
                        "destination packet buffer"))
                    return PEC_ERROR_INTERNAL;

                rdr_control.f_last_segment = true;

#ifdef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
                // For NULL packet pointers only, record cache invalidation
                if (rec_p == NULL)
                    rdr_control.prep_segment_byte_count = 0;
                else
#endif
                    rdr_control.prep_segment_byte_count = rec_p->props.size;

                eip202_rdr_prepared[interface_id][rdr_index].prep_control_word =
                    eip202_rdr_write_prepared_control_word(&rdr_control);

                rdr_index +=1;
            }
            cdr_index +=1;
        }
#endif
        *put_count_p += 1;
    } // for, commands_count, cmd_lp


    if (!eip202_continuous_scatter[interface_id])
    {
        LOG_INFO("\n\t\t\t eip202_rdr_descriptor_prepare \n");
        res = eip202_rdr_descriptor_prepare(rdr_io_area + interface_id,
                                            eip202_rdr_prepared[interface_id],
                                            rdr_index,
                                            &submitted,
                                            &filled_rdr);
        if (res != EIP202_RING_NO_ERROR || submitted != rdr_index)
        {
            LOG_CRIT("adapter_pec_dev_packet_put: writing prepared descriptors"
                     "error code %d count=%d expected=%d\n",
                     res, submitted, rdr_index);
            return PEC_ERROR_INTERNAL;
        }
    }
    LOG_INFO("\n\t\t\t eip202_cdr_descriptor_put \n");

    res = eip202_cdr_descriptor_put(cdr_io_area + interface_id,
                                   eip202_cdr_entries[interface_id],
                                   cdr_index,
                                   &submitted,
                                   &filled_cdr);
    if (res != EIP202_RING_NO_ERROR || submitted != cdr_index)
    {
        LOG_CRIT("adapter_pec_dev_packet_put: writing command descriptors"
                 "error code %d count=%d expected=%d\n",
                 res, submitted, cdr_index);
        return PEC_ERROR_INTERNAL;
    }

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_packet_get
 */
pec_status_t
adapter_pec_dev_packet_get(
        const unsigned int interface_id,
        pec_result_descriptor_t * results_p,
        const unsigned int results_limit,
        unsigned int * const get_count_p)
{
    unsigned int res_lp;
    unsigned int dscr_count;
    unsigned int dscr_done_count;
    unsigned int res_index;

    eip202_ring_error_t res;

    LOG_INFO("\n\t\t adapter_pec_dev_packet_get \n");

    *get_count_p = 0;

    if (results_limit == 0)
        return PEC_STATUS_OK;

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return PEC_ERROR_BAD_PARAMETER;

    LOG_INFO("\n\t\t\t eip202_rdr_descriptor_get \n");

    // Assume that we do get the requested number of descriptors
    // as they were reported available.
    res = eip202_rdr_descriptor_get(rdr_io_area + interface_id,
                                    eip202_rdr_entries[interface_id],
                                    results_limit, // Max number of packets.
                                    ADAPTER_EIP202_MAX_LOGICDESCR,
                                    &dscr_done_count,
                                    &dscr_count);
    if (res != EIP202_RING_NO_ERROR)
        return PEC_ERROR_INTERNAL;

    res_index = 0;
    for (res_lp = 0; res_lp < results_limit; res_lp++)
    {
        eip202_rdr_result_control_t control_word;
        bool f_encountered_first = false;

        if (res_index >= dscr_done_count)
            break;

        for (;;)
        {
            LOG_INFO("\n\t\t\t eip202_rdr_read_processed_control_word \n");

            eip202_rdr_read_processed_control_word(
                                eip202_rdr_entries[interface_id] + res_index,
                                &control_word,
                                NULL);

            if ( control_word.f_first_segment)
            {
                f_encountered_first = true;
                results_p[res_lp].num_particles = 0;
            }
            results_p[res_lp].num_particles++;

            if ( control_word.f_last_segment && f_encountered_first)
                break; // Last segment of packet, only valid when
                       // first segment was encountered.
            res_index++;

            // There may be unused scatter particles after the last segment
            // that must be skipped.
            if (res_index >= dscr_done_count)
                return PEC_STATUS_OK;
        }

        // Presence of Output Token placeholder is optional
        if (results_p[res_lp].output_token_p)
        {
            // Copy Output Token from EIP-202 to PEC result descriptor
            memcpy(results_p[res_lp].output_token_p,
                   eip202_rdr_entries[interface_id][res_index].token_p,
                   io_token_out_word_count_get() * sizeof (u32));

            io_token_packet_legth_get(results_p[res_lp].output_token_p,
                                    &results_p[res_lp].dst_pkt_byte_count);

            io_token_bypass_legth_get(results_p[res_lp].output_token_p,
                                    &results_p[res_lp].bypass_word_count);
        }

        // Copy the first EIP-202 result descriptor word, contains
        // - Particle byte count,
        // - Buffer overflow (BIT_21) and Descriptor overflow (BIT_20) errors
        // - First segment (BIT_23) and Last segment (BIT_22) indicators
        // - Descriptor overflow word count if BIT_20 is set
        results_p[res_lp].status1 =
              eip202_rdr_entries[interface_id][res_index].proc_control_word;

        *get_count_p += 1;
        res_index++;
    }

    return PEC_STATUS_OK;
}

/*----------------------------------------------------------------------------
 * adapter_pec_dev_inline_rd_put
 */
pec_status_t adapter_pec_dev_inline_rd_put(
	const unsigned int interface_id,
	pec_result_descriptor_t *results_p,
	const unsigned int results_limit,
	unsigned int *const get_count_p)
{
	unsigned int res_lp;
#ifdef ADAPTER_EIP202_STRICT_ARGS
	unsigned int free_rdr;
#endif
	unsigned int filled_rdr, rdr_index = 0;
	unsigned int submitted = 0;
	eip202_ring_error_t res;
	eip202_rdr_prepared_control_t rdr_control;

	*get_count_p = 0;
	if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
		return PEC_ERROR_BAD_PARAMETER;

#ifdef ADAPTER_EIP202_STRICT_ARGS

	res = eip202_rdr_fill_level_get(rdr_io_area + interface_id, &filled_rdr);
	if (res != EIP202_RING_NO_ERROR)
		return PEC_ERROR_INTERNAL;

	if (filled_rdr > ADAPTER_EIP202_MAX_PACKETS)
		return PEC_ERROR_INTERNAL;

	free_rdr = ADAPTER_EIP202_MAX_PACKETS - filled_rdr;

	//    free_cdr = MIN(ADAPTER_EIP202_MAX_LOGICDESCR, free_cdr);
	free_rdr = MIN(ADAPTER_EIP202_MAX_LOGICDESCR, free_rdr);
#endif

	for (res_lp = 0; res_lp < results_limit; res_lp++)
	{
#ifdef ADAPTER_EIP202_STRICT_ARGS
		if (rdr_index == free_rdr)
		{
			LOG_CRIT("Run out of free descriptors in any of the rings.");
			break;
		}
#endif

		rdr_control.f_first_segment = true;
		rdr_control.f_last_segment = false;
		rdr_control.expected_result_word_count = 0;

#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
		{
			unsigned int gather_particles;
			unsigned int scatter_particles;
			unsigned int required_rdr;
			unsigned int i;
			unsigned int gather_byte_count;

			pec_sg_list_get_capacity(results_p[res_lp].dst_pkt_handle,
								   &scatter_particles);

			if (scatter_particles == 0)
				required_rdr = 1;
			else
				required_rdr = scatter_particles;

			/* Check whether it will fit into the rings and the
             * prepared descriptor arrays.*/
#ifdef ADAPTER_EIP202_STRICT_ARGS
			if (rdr_index + required_rdr > free_rdr)
			{
				LOG_CRIT("Not enough free descriptor available");
				break;
			}
#endif

			if (scatter_particles > 0)
			{
				for (i = 0; i < scatter_particles; i++) {
					dma_buf_handle_t particle_handle;
					u8 *dummy_ptr;
					unsigned int particle_size;

					pec_sg_list_read(results_p[res_lp].dst_pkt_handle, i,
									&particle_handle, &particle_size, &dummy_ptr);

					adapter_get_phys_addr(particle_handle,
						&(eip202_rdr_prepared[interface_id][rdr_index + i]
							  .dst_packet_addr));

					if (!adapter_lib_ring_eip202_dma_addr_is_sane(
							&eip202_rdr_prepared[interface_id][rdr_index + i]
								 .dst_packet_addr, "destination packet buffer"))
						return PEC_ERROR_INTERNAL;

					rdr_control.f_last_segment = (required_rdr == i + 1);
					rdr_control.prep_segment_byte_count = particle_size;
					eip202_rdr_prepared[interface_id][rdr_index + i]
						.prep_control_word = eip202_rdr_write_prepared_control_word(&rdr_control);

					rdr_control.f_first_segment = false;
				}
			} else { /* No scatter, use single destination buffer */
				dma_resource_handle_t *dma_handle =
					adapter_dma_buf_handle2_dma_resource_handle(
						results_p[res_lp]
							.dst_pkt_handle);
				dma_resource_record_t *rec_p;

				if (dma_resource_is_valid_handle(dma_handle))
					rec_p = dma_resource_handle2_record_ptr(dma_handle);
				else
					rec_p = NULL;

					// Check if NULL packet pointers are allowed
					// for record invalidation commands
#ifndef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
				if (rec_p == NULL)
					return PEC_ERROR_INTERNAL;
#endif

				adapter_get_phys_addr(
					results_p[cmd_lp].dst_pkt_handle,
					&(eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr));

				if (!adapter_lib_ring_eip202_dma_addr_is_sane(
						&eip202_rdr_prepared[interface_id][rdr_index]
							 .dst_packet_addr, "destination packet buffer"))
					return PEC_ERROR_INTERNAL;

				rdr_control.f_last_segment = (required_rdr == 1);

#ifdef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
				// For NULL packet pointers only, record cache invalidation
				if (rec_p == NULL)
					rdr_control.prep_segment_byte_count = 0;
				else
#endif
					rdr_control.prep_segment_byte_count = rec_p->props.size;

				eip202_rdr_prepared[interface_id][rdr_index].prep_control_word =
					eip202_rdr_write_prepared_control_word(&rdr_control);

				rdr_control.f_first_segment = false;
				i = 1;
			}

			/* Add any dummy segments for overlapping rings */
			for (; i < required_rdr; i++)
			{
				rdr_control.f_last_segment = (required_rdr == i + 1);
				rdr_control.prep_segment_byte_count = 0;
				eip202_rdr_prepared[interface_id][rdr_index + i].prep_control_word =
					eip202_rdr_write_prepared_control_word(&rdr_control);
			}
			rdr_index += required_rdr;
		}
#else //NO SCATTER GATHER MODE
		{
			// Prepare source and destination buffer in non-sg case.
			dma_resource_handle_t *dma_handle =
				adapter_dma_buf_handle2_dma_resource_handle(
					results_p[res_lp].dst_pkt_handle);

			dma_resource_record_t *rec_p;

			if (dma_resource_is_valid_handle(dma_handle))
				rec_p = dma_resource_handle2_record_ptr(dma_handle);
			else
				rec_p = NULL;

			// Check if NULL packet pointers are allowed
			// for record invalidation commands
#ifndef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
			if (rec_p == NULL) {
				LOG_CRIT("NULL_PKT_POINTER");
				return PEC_ERROR_INTERNAL;
			}
#endif
			adapter_get_phys_addr(
				results_p[res_lp].dst_pkt_handle,
				&(eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr));

			if (!adapter_lib_ring_eip202_dma_addr_is_sane(
					&eip202_rdr_prepared[interface_id][rdr_index].dst_packet_addr,
					"destination packet buffer")) {
				LOG_CRIT("Failed in adapter_lib_ring_eip202_dma_addr_is_sane ");
				return PEC_ERROR_INTERNAL;
			}

			rdr_control.f_last_segment = true;

#ifdef ADAPTER_EIP202_INVALIDATE_NULL_PKT_POINTER
			// For NULL packet pointers only, record cache invalidation
			if (rec_p == NULL)
				rdr_control.prep_segment_byte_count = 0;
			else
#endif
				rdr_control.prep_segment_byte_count = rec_p->props.size;

			eip202_rdr_prepared[interface_id][rdr_index].prep_control_word =
				eip202_rdr_write_prepared_control_word(&rdr_control);

			rdr_index += 1;
		}
#endif
		*get_count_p += 1;
	} // for, commands_count, cmd_lp

	LOG_INFO("\n\tEIP202_RDR_Descriptor_Prepare, RDRIdx: 0x%x \n", rdr_index);

	res = eip202_rdr_descriptor_prepare(rdr_io_area + interface_id,
										eip202_rdr_prepared[interface_id],
										rdr_index, &submitted, &filled_rdr);
	if (res != EIP202_RING_NO_ERROR || submitted != rdr_index) {
		LOG_CRIT("adapter_pec_dev_inline_rd_put: writing prepared descriptors"
				 "error code %d count=%d expected=%d\n",
				 res, submitted, rdr_index);
		return PEC_ERROR_INTERNAL;
	}

	return PEC_STATUS_OK;
}


#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
/*----------------------------------------------------------------------------
 * adapter_pec_dev_test_sg
 */
bool
adapter_pec_dev_test_sg(
    const unsigned int interface_id,
    const unsigned int gather_particle_count,
    const unsigned int scatter_particle_count)
{
    unsigned int g_count = gather_particle_count;
    unsigned int s_count = scatter_particle_count;
    unsigned int free_cdr, free_rdr, filled_cdr, filled_rdr;
    eip202_ring_error_t res;

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return false;

    if (g_count == 0)
        g_count = 1;

    if (s_count == 0)
        s_count = 1;

#ifndef ADAPTER_EIP202_SEPARATE_RINGS
    g_count = MAX(g_count, s_count);
    s_count = g_count;
#endif

    if (g_count > ADAPTER_EIP202_MAX_LOGICDESCR ||
        s_count > ADAPTER_EIP202_MAX_LOGICDESCR)
        return false;

    LOG_INFO("\n\t\t\t eip202_cdr_fill_level_get \n");

    res = eip202_cdr_fill_level_get(cdr_io_area + interface_id,
                                  &filled_cdr);
    if (res != EIP202_RING_NO_ERROR)
        return false;

    if (filled_cdr > ADAPTER_EIP202_MAX_PACKETS)
        return false;

    free_cdr = ADAPTER_EIP202_MAX_PACKETS - filled_cdr;

    LOG_INFO("\n\t\t\t eip202_rdr_fill_level_get \n");

    if (eip202_continuous_scatter[interface_id])
    {
        return (free_cdr >= g_count);
    }
    else
    {
        res = eip202_rdr_fill_level_get(rdr_io_area + interface_id,
                                           &filled_rdr);
        if (res != EIP202_RING_NO_ERROR)
            return false;

        if (filled_rdr > ADAPTER_EIP202_MAX_PACKETS)
            return false;

        free_rdr = ADAPTER_EIP202_MAX_PACKETS - filled_rdr;

        return (free_cdr >= g_count && free_rdr >= s_count);
    }
}
#endif // ADAPTER_EIP202_ENABLE_SCATTERGATHER


#ifdef ADAPTER_EIP202_INTERRUPTS_ENABLE
/* Adapter_PECDev_IRQToInteraceID
 */
unsigned int
adapter_pec_dev_irq_to_inferface_id(
        const int n_irq)
{
    unsigned int i, irq_nr;

    if (n_irq < 0)
        return 0;

    irq_nr = (unsigned int)n_irq;

    for (i = 0; i < ADAPTER_EIP202_DEVICE_COUNT; i++)
    {
        if (irq_nr == eip202_devices[i].RDR_IRQ_ID ||
            irq_nr == eip202_devices[i].CDR_IRQ_ID)
        {
            return i;
        }
    }

    LOG_CRIT("Adapter_PECDev_IRQToInterfaceId: unknown interrupt %d\n",n_irq);

    return 0;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_enable_result_irq
 */
void
adapter_pec_dev_enable_result_irq(
        const unsigned int interface_id)
{
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    LOG_INFO(
      "\n\t\t\t eip202_rdr_processed_fill_level_high_int_clear_and_disable \n");

    eip202_rdr_processed_fill_level_high_int_clear_and_disable (
        rdr_io_area + interface_id,
        false);

    LOG_INFO("\n\t\t\t eip202_rdr_processed_fill_level_high_int_enable \n");

    eip202_rdr_processed_fill_level_high_int_enable(
        rdr_io_area + interface_id,
        ADAPTER_EIP202_DESCRIPTORDONECOUNT,
        ADAPTER_EIP202_DESCRIPTORDONETIMEOUT,
        true);

    adapter_interrupt_enable(eip202_devices[interface_id].RDR_IRQ_ID,
                             eip202_devices[interface_id].rdr_irq_flags);

    eip202_interrupts[interface_id] |= BIT_0;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_disable_result_irq
 */
void
adapter_pec_dev_disable_result_irq(
        const unsigned int interface_id)
{
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    LOG_INFO(
       "\n\t\t\t eip202_rdr_processed_fill_level_high_int_clear_and_disable \n");

    eip202_rdr_processed_fill_level_high_int_clear_and_disable (
        rdr_io_area + interface_id,
        false);

    adapter_interrupt_disable(eip202_devices[interface_id].RDR_IRQ_ID,
                              eip202_devices[interface_id].rdr_irq_flags);

    eip202_interrupts[interface_id] &= ~BIT_0;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_enable_command_irq
 */
void
adapter_pec_dev_enable_command_irq(
        const unsigned int interface_id)
{
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    LOG_INFO("\n\t\t\t eip202_cdr_fill_level_low_int_enable \n");

    eip202_cdr_fill_level_low_int_enable(
        cdr_io_area + interface_id,
        ADAPTER_EIP202_DESCRIPTORDONECOUNT,
        ADAPTER_EIP202_DESCRIPTORDONETIMEOUT);

    adapter_interrupt_enable(eip202_devices[interface_id].CDR_IRQ_ID,
                             eip202_devices[interface_id].cdr_irq_flags);

    eip202_interrupts[interface_id] |= BIT_1;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_disable_command_irq
 */
void
adapter_pec_dev_disable_command_irq(
        const unsigned int interface_id)
{
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    LOG_INFO("\n\t\t\t eip202_cdr_fill_level_low_int_clear_and_disable \n");

    eip202_cdr_fill_level_low_int_clear_and_disable(
        cdr_io_area + interface_id);

    adapter_interrupt_disable(eip202_devices[interface_id].CDR_IRQ_ID,
                              eip202_devices[interface_id].cdr_irq_flags);

    eip202_interrupts[interface_id] &= ~BIT_1;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_set_result_handler
 */
void adapter_pec_dev_set_result_handler(
        const unsigned int interface_id,
        adapter_interrupt_handler_t handler_function)
{
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    adapter_interrupt_set_handler(eip202_devices[interface_id].RDR_IRQ_ID,
                                 handler_function);
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_set_command_handler
 */
void adapter_pec_dev_set_command_handler(
        const unsigned int interface_id,
        adapter_interrupt_handler_t handler_function)
{
    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    adapter_interrupt_set_handler(eip202_devices[interface_id].CDR_IRQ_ID,
                                 handler_function);
}
#endif // ADAPTER_EIP202_INTERRUPTS_ENABLE


/*----------------------------------------------------------------------------
 * adapter_pec_dev_scatter_preload
 */
pec_status_t
adapter_pec_dev_scatter_preload(
        const unsigned int interface_id,
        dma_buf_handle_t * handles_p,
        const unsigned int handles_count)
{
    unsigned int i;
    eip202_rdr_prepared_control_t rdr_control;
    eip202_ring_error_t res;
    unsigned int submitted,filled_rdr;
    for (i = 0; i <  handles_count; i++)
    {
        dma_buf_handle_t particle_handle = handles_p[i];
        dma_resource_handle_t dma_handle = adapter_dma_buf_handle2_dma_resource_handle(particle_handle);
        dma_resource_record_t * rec_p = dma_resource_handle2_record_ptr(dma_handle);

        rdr_control.f_first_segment           = true;
#ifdef ADAPTER_EIP202_ENABLE_SCATTERGATHER
        rdr_control.f_last_segment            = false;
#else
        rdr_control.f_last_segment            = true;
        // When No scatter gather supported, packets must fit into single buffer.
#endif
        rdr_control.prep_segment_byte_count = rec_p->props.size;
        rdr_control.expected_result_word_count = 0;
        eip202_rdr_prepared[interface_id][i].prep_control_word =
            eip202_rdr_write_prepared_control_word(&rdr_control);
        adapter_get_phys_addr(particle_handle,
                            &(eip202_rdr_prepared[interface_id][i].dst_packet_addr));
    }
    res = eip202_rdr_descriptor_prepare(rdr_io_area + interface_id,
                                       eip202_rdr_prepared[interface_id],
                                       handles_count,
                                       &submitted,
                                       &filled_rdr);
    if (res != EIP202_RING_NO_ERROR || submitted != handles_count)
    {
        LOG_CRIT("adapter_pec_dev_packet_put: writing prepared descriptors"
                 "error code %d count=%d expected=%d\n",
                 res, submitted, handles_count);
        return PEC_ERROR_INTERNAL;
    }

    return PEC_STATUS_OK;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_put_dump
 */
void
adapter_pec_dev_put_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_cdr_admin,
        const bool f_dump_cdr_cache)
{
    void * va;
    unsigned int i, cd_offset;
    u32 word32;
    eip202_ring_admin_t ring_admin;
    dma_resource_addr_pair_t addr_pair;

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    if (first_slot_id >= ADAPTER_EIP202_MAX_PACKETS ||
        last_slot_id >= ADAPTER_EIP202_MAX_PACKETS)
        return;

    if (first_slot_id > last_slot_id)
        return;

    adapter_lib_ring_eip202_cdr_status_report(interface_id);

    zeroinit(ring_admin);
    eip202_cdr_dump(cdr_io_area + interface_id, &ring_admin);
    cd_offset = ring_admin.desc_offs_word_count;

    zeroinit(addr_pair);
    if (dma_resource_translate(cdr_handle[interface_id],
                          DMARES_DOMAIN_HOST,
                              &addr_pair)==0)
    {
        va = addr_pair.address_p;
    }
    else
    {
        va = NULL;
    }

    if (f_dump_cdr_admin)
    {
        void * pa;

        zeroinit(addr_pair);
        if (dma_resource_translate(cdr_handle[interface_id],
                                  DMARES_DOMAIN_BUS,
                                  &addr_pair) == 0) {
            pa = addr_pair.address_p;
        }
        else
        {
            pa = NULL;
        }

        LOG_CRIT("\n\tCDR admin data, all sizes in 32-bit words:\n"
                 "\tSeparate:         %s\n"
                 "\tRing size:        %d\n"
                 "\tDescriptor size:  %d\n"
                 "\tInput ring size:  %d\n"
                 "\tInput ring tail:  %d\n"
                 "\tOutput ring size: %d\n"
                 "\tOutput ring head: %d\n"
                 "\tRing phys addr:   %p\n"
                 "\tRing host addr:   %p\n",
                 ring_admin.f_separate ? "yes" : "no",
                 ring_admin.ring_size_word_count,
                 ring_admin.desc_offs_word_count,
                 ring_admin.in_size,
                 ring_admin.in_tail,
                 ring_admin.out_size,
                 ring_admin.out_head,
                 pa,
                 va);
    }

    LOG_CRIT("\n\tCDR dump, first slot %d, last slot %d\n",
             first_slot_id,
             last_slot_id);

    for (i = first_slot_id; i <= last_slot_id; i++)
    {
        unsigned int j;

        LOG_CRIT("\tDescriptor %d:\n", i);
        for (j = 0; j < cd_offset; j++)
        {
            word32 = dma_resource_read32(cdr_handle[interface_id],
                                        i * cd_offset + j);

            LOG_CRIT("\tCD[%02d] word[%02d] 0x%08x\n",
                     i,
                     j,
                     word32);
        }
    }

    if (!f_dump_cdr_cache)
        return;

    LOG_CRIT("\n\tCDR cache dump, size %d entries\n",
             ADAPTER_EIP202_MAX_LOGICDESCR);

    for (i = 0; i < ADAPTER_EIP202_MAX_LOGICDESCR; i++)
    {
        unsigned int j;

        LOG_CRIT("\tDescriptor %d cache:\n", i);

        word32 = eip202_cdr_entries[interface_id][i].control_word;
        LOG_CRIT("\tCDC[%02d] word[00] 0x%08x\n", i, word32);

        word32 = eip202_cdr_entries[interface_id][i].src_packet_addr.addr;
        LOG_CRIT("\tCDC[%02d] word[02] 0x%08x\n", i, word32);

        word32 = eip202_cdr_entries[interface_id][i].src_packet_addr.upper_addr;
        LOG_CRIT("\tCDC[%02d] word[03] 0x%08x\n", i, word32);

        word32 = eip202_cdr_entries[interface_id][i].token_data_addr.addr;
        LOG_CRIT("\tCDC[%02d] word[04] 0x%08x\n", i, word32);

        word32 = eip202_cdr_entries[interface_id][i].token_data_addr.upper_addr;
        LOG_CRIT("\tCDC[%02d] word[05] 0x%08x\n", i, word32);

        if (eip202_cdr_entries[interface_id][i].token_p)
            for (j = 0; j < io_token_in_word_count_get(); j++)
            {
                word32 = eip202_cdr_entries[interface_id][i].token_p[j];
                LOG_CRIT("\tCDC[%02d] word[%02d] 0x%08x\n", i, 6 + j, word32);
            }
    }

    return;
}


/*----------------------------------------------------------------------------
 * adapter_pec_dev_get_dump
 */
void
adapter_pec_dev_get_dump(
        const unsigned int interface_id,
        const unsigned int first_slot_id,
        const unsigned int last_slot_id,
        const bool f_dump_rdr_admin,
        const bool f_dump_rdr_cache)
{
    void * va;
    unsigned int i, rd_offset;
    u32 word32;
    eip202_ring_admin_t ring_admin;
    dma_resource_addr_pair_t addr_pair;

    if (interface_id >= ADAPTER_EIP202_DEVICE_COUNT)
        return;

    if (first_slot_id >= ADAPTER_EIP202_MAX_PACKETS ||
        last_slot_id >= ADAPTER_EIP202_MAX_PACKETS)
        return;

    if (first_slot_id > last_slot_id)
        return;

    adapter_lib_ring_eip202_rdr_status_report(interface_id);

    zeroinit(ring_admin);
    eip202_rdr_dump(rdr_io_area + interface_id, &ring_admin);
    rd_offset = ring_admin.desc_offs_word_count;

    zeroinit(addr_pair);
    if (dma_resource_translate(rdr_handle[interface_id],
                              DMARES_DOMAIN_HOST,
                              &addr_pair) == 0) {
        va = addr_pair.address_p;
    }
    else
    {
        va = NULL;
    }

    if (f_dump_rdr_admin)
    {
        void * pa;

        zeroinit(addr_pair);
        if (dma_resource_translate(rdr_handle[interface_id],
                              DMARES_DOMAIN_BUS,
                                  &addr_pair) == 0) {
            pa = addr_pair.address_p;
        }
        else
        {
            pa = NULL;
        }
        LOG_CRIT("\n\tRDR admin data, all sizes in 32-bit words:\n"
                 "\tSeparate:         %s\n"
                 "\tRing size:        %d\n"
                 "\tDescriptor size:  %d\n"
                 "\tInput ring size:  %d\n"
                 "\tInput ring tail:  %d\n"
                 "\tOutput ring size: %d\n"
                 "\tOutput ring head: %d\n"
                 "\tRing phys addr:   %p\n"
                 "\tRing host addr:   %p\n",
                 ring_admin.f_separate ? "yes" : "no",
                 ring_admin.ring_size_word_count,
                 ring_admin.desc_offs_word_count,
                 ring_admin.in_size,
                 ring_admin.in_tail,
                 ring_admin.out_size,
                 ring_admin.out_head,
                 pa,
                 va);
    }

    LOG_CRIT("\n\tRDR dump, first slot %d, last slot %d\n",
             first_slot_id,
             last_slot_id);

    for (i = first_slot_id; i <= last_slot_id; i++)
    {
        unsigned int j;

        LOG_CRIT("\tDescriptor %d:\n", i);
        for (j = 0; j < rd_offset; j++)
        {
            word32 = dma_resource_read32(rdr_handle[interface_id],
                                        i * rd_offset + j);

            LOG_CRIT("\tRD[%02d] word[%02d] 0x%08x\n",
                     i,
                     j,
                     word32);
        }
    }

    if (!f_dump_rdr_cache)
        return;

    LOG_CRIT("\n\tRDR cache dump, size %d entries\n",
             ADAPTER_EIP202_MAX_LOGICDESCR);

    for (i = 0; i < ADAPTER_EIP202_MAX_LOGICDESCR; i++)
    {
        unsigned int j;

        LOG_CRIT("\tDescriptor %d cache:\n", i);

        word32 = eip202_rdr_entries[interface_id][i].proc_control_word;
        LOG_CRIT("\tRDC[%02d] word[00] 0x%08x\n", i, word32);

        word32 = eip202_rdr_entries[interface_id][i].dst_packet_addr.addr;
        LOG_CRIT("\tRDC[%02d] word[02] 0x%08x\n", i, word32);

        word32 = eip202_rdr_entries[interface_id][i].dst_packet_addr.upper_addr;
        LOG_CRIT("\tRDC[%02d] word[03] 0x%08x\n", i, word32);

        for (j = 0; j < io_token_out_word_count_get(); j++)
        {
            word32 = eip202_rdr_entries[interface_id][i].token_p[j];
            LOG_CRIT("\tRDC[%02d] word[%02d] 0x%08x\n", i, 4 + j, word32);
        }
    }

    return;
}


/* end of file adapter_ring_eip202.c */
