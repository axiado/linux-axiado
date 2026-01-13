/* adapter_global_cs_init.c
 *
 * Initialize Global Classification Control functionality.
 */

/*****************************************************************************
* Copyright (c) 2011-2021 by Rambus, Inc. and/or its subsidiaries.
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

#include "adapter_global_cs_init.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_adapter_cs.h"

// Global Control API
#include "api_global_eip97.h"

// Global Control Classification API
#include "api_global_eip207.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework C Library API
#include "clib.h"               // memcpy, zeroinit

#include "device_types.h"       // device_handle_t
#include "device_mgmt.h"        // Device_find
#include "log.h"                // Log API


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

static const u32 global_iv_data[4] = ADAPTER_CS_IV;


/*----------------------------------------------------------------------------
 * yes_no
 *
 * Convert boolean value to string.
 */
static const char *
yes_no(
        const bool b)
{
    if (b)
        return "YES";
    else
        return "no";
}


/*----------------------------------------------------------------------------
 * adapter_global_cs_status_report()
 *
 * Obtain all available global status information from the Global Classification
 * hardware and report it.
 */
static void
adapter_global_cs_status_report(void)
{
    global_control207_error_t rc;
    unsigned int i;
    unsigned int nof_c_es;

    LOG_INFO("\n\t\t adapter_global_cs_status_report \n");

    LOG_CRIT("Global Classification Control status\n");
    global_control97_interfaces_get(&nof_c_es, NULL, NULL, NULL);

    for (i = 0; i < nof_c_es; i++)
    {
        global_control207_status_t ce_status;
        global_control207_global_stats_t ce_global_stats;
        global_control207_clock_t ce_clock;

        zeroinit(ce_status);
        zeroinit(ce_global_stats);
        zeroinit(ce_clock);

        LOG_CRIT("Classification Engine %d status\n", i);

        rc = global_control207_status_get(i, &ce_status);
        if (rc != EIP207_GLOBAL_CONTROL_NO_ERROR)
            LOG_CRIT("%s: global_control207_status_get() failed\n", __func__);
        else
        {
            if (ce_status.ice.f_pue_ecc_corr          ||
                ce_status.ice.f_pue_ecc_derr          ||
                ce_status.ice.f_fpp_ecc_corr          ||
                ce_status.ice.f_fpp_ecc_derr          ||
                ce_status.ice.f_timer_overflow        ||
                ce_status.oce.f_pue_ecc_corr          ||
                ce_status.oce.f_pue_ecc_derr          ||
                ce_status.oce.f_fpp_ecc_corr          ||
                ce_status.oce.f_fpp_ecc_derr          ||
                ce_status.oce.f_timer_overflow        ||
                ce_status.flue.error1 != 0          ||
                ce_status.flue.error2 != 0          ||
                ce_status.frc[0].f_dma_read_error      ||
                ce_status.frc[0].f_dma_write_error     ||
                ce_status.frc[0].f_data_ecc_oflo       ||
                ce_status.frc[0].f_data_ecc_err        ||
                ce_status.frc[0].f_admin_ecc_err       ||
                ce_status.trc[0].f_dma_read_error      ||
                ce_status.trc[0].f_dma_write_error     ||
                ce_status.trc[0].f_data_ecc_oflo       ||
                ce_status.trc[0].f_data_ecc_err        ||
                ce_status.trc[0].f_admin_ecc_err       ||
                ce_status.arc4_rc[0].f_dma_read_error   ||
                ce_status.arc4_rc[0].f_dma_write_error  ||
                ce_status.arc4_rc[0].f_data_ecc_oflo    ||
                ce_status.arc4_rc[0].f_data_ecc_err     ||
                ce_status.arc4_rc[0].f_admin_ecc_err)
            {
                LOG_CRIT("%s: error(s) detected\n", __func__);
                LOG_CRIT(
                 "\tICE Pull-Up ECC Correctable Err:        %s\n"
                 "\tICE Pull-Up ECC Non-Corr Err:           %s\n"
                 "\tICE Post-Processor ECC Correctable Err: %s\n"
                 "\tICE Post-Processor ECC Non-Corr Err:    %s\n"
                 "\tICE Timer ovf detected:                 %s\n"
                 "\tOCE Pull-Up ECC Correctable Err:        %s\n"
                 "\tOCE Pull-Up ECC Non-Corr Err:           %s\n"
                 "\tOCE Post-Processor ECC Correctable Err: %s\n"
                 "\tOCE Post-Processor ECC Non-Corr Err:    %s\n"
                 "\tOCE Timer ovf detected:                 %s\n"
                 "\tFLUE Err 1:                             %s (mask=0x%08x)\n"
                 "\tFLUE Err 2:                             %s (mask=0x%08x)\n"
                 "\tFRC#0 DMA Read Err:                     %s\n"
                 "\tFRC#0 DMA Write Err:                    %s\n"
                 "\tFRC#0 data RAM ECC Non-Corr Ovf Err:    %s\n"
                 "\tFRC#0 data RAM ECC Non-Corr Err:        %s\n"
                 "\tFRC#0 Admin RAM ECC Non-Corr Err:       %s\n"
                 "\tTRC#0 DMA Read Err:                     %s\n"
                 "\tTRC#0 DMA Write Err:                    %s\n"
                 "\tTRC#0 data RAM ECC Non-Corr Ovf Err:    %s\n"
                 "\tTRC#0 data RAM ECC Non-Corr Err:        %s\n"
                 "\tTRC#0 Admin RAM ECC Non-Corr Err:       %s\n"
                 "\tARC4RC#0 DMA Read Err:                  %s\n"
                 "\tARC4RC#0 DMA Write Err:                 %s\n"
                 "\tARC4RC#0 data RAM ECC Non-Corr Ovf Err: %s\n"
                 "\tARC4RC#0 data RAM ECC Non-Corr Err:     %s\n"
                 "\tARC4RC#0 Admin RAM ECC Non-Corr Err:    %s\n\n",
                 yes_no(ce_status.ice.f_pue_ecc_corr),
                 yes_no(ce_status.ice.f_pue_ecc_derr),
                 yes_no(ce_status.ice.f_fpp_ecc_corr),
                 yes_no(ce_status.ice.f_fpp_ecc_derr),
                 yes_no(ce_status.ice.f_timer_overflow),
                 yes_no(ce_status.oce.f_pue_ecc_corr),
                 yes_no(ce_status.oce.f_pue_ecc_derr),
                 yes_no(ce_status.oce.f_fpp_ecc_corr),
                 yes_no(ce_status.oce.f_fpp_ecc_derr),
                 yes_no(ce_status.oce.f_timer_overflow),
                 yes_no(ce_status.flue.error1 != 0),
                 ce_status.flue.error1,
                 yes_no(ce_status.flue.error2 != 0),
                 ce_status.flue.error2,
                 yes_no(ce_status.frc[0].f_dma_read_error),
                 yes_no(ce_status.frc[0].f_dma_write_error),
                 yes_no(ce_status.frc[0].f_data_ecc_oflo),
                 yes_no(ce_status.frc[0].f_data_ecc_err),
                 yes_no(ce_status.frc[0].f_admin_ecc_err),
                 yes_no(ce_status.trc[0].f_dma_read_error),
                 yes_no(ce_status.trc[0].f_dma_write_error),
                 yes_no(ce_status.trc[0].f_data_ecc_oflo),
                 yes_no(ce_status.trc[0].f_data_ecc_err),
                 yes_no(ce_status.trc[0].f_admin_ecc_err),
                 yes_no(ce_status.arc4_rc[0].f_dma_read_error),
                 yes_no(ce_status.arc4_rc[0].f_dma_write_error),
                 yes_no(ce_status.arc4_rc[0].f_data_ecc_oflo),
                 yes_no(ce_status.arc4_rc[0].f_data_ecc_err),
                 yes_no(ce_status.arc4_rc[0].f_admin_ecc_err));
            }
            else
                LOG_CRIT("%s: all OK\n", __func__);
            if (ce_status.frc_stats[0].prefetch_exec ||
                ce_status.frc_stats[0].prefetch_block ||
                ce_status.frc_stats[0].prefetch_dma ||
                ce_status.frc_stats[0].select_ops ||
                ce_status.frc_stats[0].select_dma ||
                ce_status.frc_stats[0].int_dma_write ||
                ce_status.frc_stats[0].ext_dma_write ||
                ce_status.frc_stats[0].invalidate_ops)
            {
                LOG_CRIT("frc statistics:\n"
                         "\tPrefetches executed: %u\n"
                         "\tPrefetches blocked:  %u\n"
                         "\tPrefetches with DMA: %u\n"
                         "\tSelect ops:          %u\n"
                         "\tSelect ops with DMA: %u\n"
                         "\tInternal DMA writes: %u\n"
                         "\tExternal DMA writes: %u\n"
                         "\tInvalidate ops:      %u\n"
                         "\tDMA err flags        0x%x\n"
                         "\tRead DMA errs:       %u\n"
                         "\tWrite DMA errs:      %u\n"
                         "\tECC invalidates:     %u\n"
                         "\tECC data RAM Corr:   %u\n"
                         "\tECC Admin RAM Corr:  %u\n",
                         (u32)ce_status.frc_stats[0].prefetch_exec,
                         (u32)ce_status.frc_stats[0].prefetch_block,
                         (u32)ce_status.frc_stats[0].prefetch_dma,
                         (u32)ce_status.frc_stats[0].select_ops,
                         (u32)ce_status.frc_stats[0].select_dma,
                         (u32)ce_status.frc_stats[0].int_dma_write,
                         (u32)ce_status.frc_stats[0].ext_dma_write,
                         (u32)ce_status.frc_stats[0].invalidate_ops,
                         (u32)ce_status.frc_stats[0].read_dma_err_flags,
                         (u32)ce_status.frc_stats[0].read_dma_errors,
                         (u32)ce_status.frc_stats[0].write_dma_errors,
                         (u32)ce_status.frc_stats[0].invalidate_ecc,
                         (u32)ce_status.frc_stats[0].data_ecc_corr,
                         (u32)ce_status.frc_stats[0].admin_ecc_corr);
            }
            if (ce_status.trc_stats[0].prefetch_exec ||
                ce_status.trc_stats[0].prefetch_block ||
                ce_status.trc_stats[0].prefetch_dma ||
                ce_status.trc_stats[0].select_ops ||
                ce_status.trc_stats[0].select_dma ||
                ce_status.trc_stats[0].int_dma_write ||
                ce_status.trc_stats[0].ext_dma_write ||
                ce_status.trc_stats[0].invalidate_ops)
            {
                LOG_CRIT("trc statistics:\n"
                         "\tPrefetches executed: %u\n"
                         "\tPrefetches blocked:  %u\n"
                         "\tPrefetches with DMA: %u\n"
                         "\tSelect ops:          %u\n"
                         "\tSelect ops with DMA: %u\n"
                         "\tInternal DMA writes: %u\n"
                         "\tExternal DMA writes: %u\n"
                         "\tInvalidate ops:      %u\n"
                         "\tDMA err flags        0x%x\n"
                         "\tRead DMA errs:       %u\n"
                         "\tWrite DMA errs:      %u\n"
                         "\tECC invalidates:     %u\n"
                         "\tECC data RAM Corr:   %u\n"
                         "\tECC Admin RAM Corr:  %u\n",
                         (u32)ce_status.trc_stats[0].prefetch_exec,
                         (u32)ce_status.trc_stats[0].prefetch_block,
                         (u32)ce_status.trc_stats[0].prefetch_dma,
                         (u32)ce_status.trc_stats[0].select_ops,
                         (u32)ce_status.trc_stats[0].select_dma,
                         (u32)ce_status.trc_stats[0].int_dma_write,
                         (u32)ce_status.trc_stats[0].ext_dma_write,
                         (u32)ce_status.trc_stats[0].invalidate_ops,
                         (u32)ce_status.trc_stats[0].read_dma_err_flags,
                         (u32)ce_status.trc_stats[0].read_dma_errors,
                         (u32)ce_status.trc_stats[0].write_dma_errors,
                         (u32)ce_status.trc_stats[0].invalidate_ecc,
                         (u32)ce_status.trc_stats[0].data_ecc_corr,
                         (u32)ce_status.trc_stats[0].admin_ecc_corr);
            }
        }

        rc = global_control207_global_stats_get(i, &ce_global_stats);
        if (rc != EIP207_GLOBAL_CONTROL_NO_ERROR)
            LOG_CRIT("adapter_global_cs_status_report: "
                     "global_control207_global_stats_get() failed\n");
        else
            LOG_CRIT(
                 "\tICE Dropped Packets counter (low 32-bits):     0x%08x\n"
                 "\tICE Dropped Packets counter (high 32-bits):    0x%08x\n"
                 "\tICE Inbound Packets counter:                   0x%08x\n"
                 "\tICE Outbound Packets counter:                  0x%08x\n"
                 "\tICE Inbound Octets counter (low 32-bits):      0x%08x\n"
                 "\tICE Inbound Octets counter (high 32-bits):     0x%08x\n"
                 "\tICE Outbound Octets counter (low 32-bits):     0x%08x\n"
                 "\tICE Outbound Octets counter (high 32-bits):    0x%08x\n"
                 "\tOCE Dropped Packets counter (low 32-bits):     0x%08x\n"
                 "\tOCE Dropped Packets counter (high 32-bits):    0x%08x\n\n",
                 ce_global_stats.ice.dropped_packets_counter.value64_lo,
                 ce_global_stats.ice.dropped_packets_counter.value64_hi,
                 ce_global_stats.ice.inbound_packets_counter,
                 ce_global_stats.ice.outbound_packet_counter,
                 ce_global_stats.ice.inbound_octets_counter.value64_lo,
                 ce_global_stats.ice.inbound_octets_counter.value64_hi,
                 ce_global_stats.ice.outbound_octets_counter.value64_lo,
                 ce_global_stats.ice.outbound_octets_counter.value64_hi,
                 ce_global_stats.oce.dropped_packets_counter.value64_lo,
                 ce_global_stats.oce.dropped_packets_counter.value64_hi);

        rc = global_control207_clock_count_get(i, &ce_clock);
        if (rc != EIP207_GLOBAL_CONTROL_NO_ERROR)
            LOG_CRIT("adapter_global_cs_status_report: "
                     "global_control207_clock_count_get() failed\n");
        else
            LOG_CRIT(
                 "\tICE Clock count (low 32-bits):   0x%08x\n"
                 "\tICE Clock count (high 32-bits):  0x%08x\n"
                 "\tOCE Clock count (low 32-bits):   0x%08x\n"
                 "\tOCE Clock count (high 32-bits):  0x%08x\n\n",
                 ce_clock.ice.value64_lo,
                 ce_clock.ice.value64_hi,
                 ce_clock.oce.value64_lo,
                 ce_clock.oce.value64_hi);
    } // for
}


/*----------------------------------------------------------------------------
 * adapter_global_cs_init()
 *
 */
bool
adapter_global_cs_init(void)
{
    global_control207_error_t rc;
    global_control207_capabilities_t capabilities;
    global_control207_iv_t data;

    LOG_INFO("\n\t\t adapter_global_cs_init \n");

    data.iv[0] = global_iv_data[0];
    data.iv[1] = global_iv_data[1];
    data.iv[2] = global_iv_data[2];
    data.iv[3] = global_iv_data[3];

    // Request the classification firmware download during the initialization
    rc = global_control207_init(true, &data);
    if (rc != EIP207_GLOBAL_CONTROL_NO_ERROR)
    {
        LOG_CRIT("Adaptar_Global_Init: Classification initialization failed\n");
        return false;
    }

    capabilities.sz_text_description[0] = 0;

    global_control207_capabilities_get(&capabilities);

    LOG_CRIT("Global Classification capabilities: %s\n",
             capabilities.sz_text_description);

    adapter_global_cs_status_report();

    return true;
}


/*----------------------------------------------------------------------------
 * adapter_global_cs_uninit()
 *
 */
void
adapter_global_cs_uninit(void)
{
    LOG_INFO("\n\t\t adapter_global_cs_uninit \n");

    adapter_global_cs_status_report();

    global_control207_uninit();
}


/* end of file adapter_global_cs_init.c */
