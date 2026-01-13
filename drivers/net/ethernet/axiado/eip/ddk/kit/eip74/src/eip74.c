/* eip74.c
 *
 * Implementation of the EIP-74 Driver Library.
 */

/*****************************************************************************
* Copyright (c) 2017-2020 by Rambus, Inc. and/or its subsidiaries.
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

// EIP76 initialization API
#include "eip74.h"

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip74.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u32

// Driver Framework device API
#include "device_types.h"       // device_handle_t

// EIP-76 Driver Library internal interfaces
#include "eip74_level0.h"       // Level 0 macros

/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// I/O Area, used internally
typedef struct
{
    device_handle_t device;
} eip74_true_io_area_t;

#define TEST_SIZEOF(type, size) \
    extern int size##_must_bigger[1 - 2*((int)(sizeof(type) > size))]


#ifdef EIP74_STRICT_ARGS
#define EIP74_CHECK_POINTER(_p) \
    if (NULL == (_p)) \
        return EIP74_ARGUMENT_ERROR;
#define EIP74_CHECK_INT_INRANGE(_i, _min, _max) \
    if ((_i) < (_min) || (_i) > (_max)) \
        return EIP74_ARGUMENT_ERROR;
#define EIP74_CHECK_INT_ATLEAST(_i, _min) \
    if ((_i) < (_min)) \
        return EIP74_ARGUMENT_ERROR;
#define EIP74_CHECK_INT_ATMOST(_i, _max) \
    if ((_i) > (_max)) \
        return EIP74_ARGUMENT_ERROR;
#else
/* EIP74_STRICT_ARGS undefined */
#define EIP74_CHECK_POINTER(_p)
#define EIP74_CHECK_INT_INRANGE(_i, _min, _max)
#define EIP74_CHECK_INT_ATLEAST(_i, _min)
#define EIP74_CHECK_INT_ATMOST(_i, _max)
#endif /*end of EIP74_STRICT_ARGS */


// validate the size of the fake and real io_area structures
TEST_SIZEOF(eip74_true_io_area_t, EIP74_IOAREA_REQUIRED_SIZE);


/*----------------------------------------------------------------------------
 * eip74_lib_detect
 *
 * Checks the presence of EIP-74 device. Returns true when found.
 */
static bool
eip74_lib_detect(
        const device_handle_t device)
{
    u32 value;

    value = eip74_read32(device, EIP74_REG_VERSION);
    if (!EIP74_REV_SIGNATURE_MATCH( value ))
        return false;

    return true;
}


/*----------------------------------------------------------------------------
 * eip74_lib_reset_is_done
 */
static bool
eip74_lib_reset_is_done(
        const device_handle_t device)
{
    bool f_ready, f_psai_write_ok, f_stuck_out, f_early_reseed;
    bool f_test_ready,f_gen_pkt_error, f_instantiated, f_test_stuck_out, f_need_clock;
    u8 blocks_available;

    EIP74_STATUS_RD(device,
                    &f_ready,
                    &f_psai_write_ok,
                    &f_stuck_out,
                    &f_early_reseed,
                    &f_test_ready,
                    &f_gen_pkt_error,
                    &f_instantiated,
                    &f_test_stuck_out,
                    &blocks_available,
                    &f_need_clock);

    return f_psai_write_ok;
}


/*----------------------------------------------------------------------------
 * eip74_init
 */
eip74_error_t
eip74_init(
        eip74_io_area_t * const io_area_p,
        const device_handle_t device)
{
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);

    true_io_area_p->device = device;

    if (!eip74_lib_detect(device))
    {
        return EIP74_UNSUPPORTED_FEATURE_ERROR;
    }

    return EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip74_reset
 */
eip74_error_t
eip74_reset(
        eip74_io_area_t * const io_area_p)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);

    device = true_io_area_p->device;

    EIP74_CONTROL_WR(device,
                     false, /* f_ready_mask */
                     false, /* f_stuck_out */
                     false, /* f_test_mode */
                     false, /* f_host_mode */
                     false, /* f_enable_drbg */
                     false, /* f_force_stuck_out */
                     false, /* fRequestdata */
                     0); /* data_blocks */

    if (eip74_lib_reset_is_done(device))
    {
        return EIP74_NO_ERROR;
    }
    else
    {
        return EIP74_BUSY_RETRY_LATER;
    }
}


/*----------------------------------------------------------------------------
 * eip74_reset_is_done
 */
eip74_error_t
eip74_reset_is_done(
        eip74_io_area_t * const io_area_p)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);

    device = true_io_area_p->device;

    if (eip74_lib_reset_is_done(device))
    {
        return EIP74_NO_ERROR;
    }
    else
    {
        return EIP74_BUSY_RETRY_LATER;
    }
}


/*----------------------------------------------------------------------------
 * eip74_hw_revision_get
  */
eip74_error_t
eip74_hw_revision_get(
        const device_handle_t device,
        eip74_capabilities_t * const capabilities_p)
{
    EIP74_CHECK_POINTER(capabilities_p);

    EIP74_VERSION_RD(device,
                     &capabilities_p->hw_revision.eip_number,
                     &capabilities_p->hw_revision.complmt_eip_number,
                     &capabilities_p->hw_revision.hw_patch_level,
                     &capabilities_p->hw_revision.min_hw_revision,
                     &capabilities_p->hw_revision.maj_hw_revision);
    EIP74_OPTIONS_RD(device,
                     &capabilities_p->hw_options.client_count,
                     &capabilities_p->hw_options.aes_core_count,
                     &capabilities_p->hw_options.aes_speed,
                     &capabilities_p->hw_options.fifo_depth);

    return EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip74_configure
 */
eip74_error_t
eip74_configure(
        eip74_io_area_t * const io_area_p,
        const eip74_configuration_t * const configuration_p)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);
    EIP74_CHECK_POINTER(configuration_p);
    EIP74_CHECK_INT_INRANGE(configuration_p->generate_block_size, 1, 4095);

    device = true_io_area_p->device;

    EIP74_GEN_BLK_SIZE_WR(device, configuration_p->generate_block_size);
    EIP74_RESEED_THR_WR(device, configuration_p->reseed_thr);
    EIP74_RESEED_THR_EARLY_WR(device, configuration_p->reseed_thr_early);

    return EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip74_instantiate
 */
eip74_error_t
eip74_instantiate(
        eip74_io_area_t * const io_area_p,
        const u32 * const entropy_p,
        bool f_stuck_out)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);
    EIP74_CHECK_POINTER(entropy_p);

    device = true_io_area_p->device;

    EIP74_PS_AI_WR(device, entropy_p, 12);

    EIP74_CONTROL_WR(device,
                     false, /* f_ready_mask */
                     f_stuck_out,
                     false, /* f_test_mode */
                     false, /* f_host_mode */
                     true, /* f_enable_drbg */
                     false, /* f_force_stuck_out */
                     false, /* fRequestdata */
                     0); /* data_blocks */

    return EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip74_reseed
 */
eip74_error_t
eip74_reseed(
        eip74_io_area_t * const io_area_p,
        const u32 * const entropy_p)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);
    EIP74_CHECK_POINTER(entropy_p);

    device = true_io_area_p->device;

    EIP74_PS_AI_WR(device, entropy_p, 12);

    return EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip74_status_get
 */
eip74_error_t
eip74_status_get(
        eip74_io_area_t * const io_area_p,
        eip74_status_t * const status_p)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    bool f_ready, f_psai_write_ok, f_stuck_out, f_early_reseed;
    bool f_test_ready,f_gen_pkt_error, f_instantiated, f_test_stuck_out, f_need_clock;
    u8 blocks_available;
    u32 gen_block_count, reseed_thr;
    EIP74_CHECK_POINTER(io_area_p);
    EIP74_CHECK_POINTER(status_p);

    device = true_io_area_p->device;

    EIP74_STATUS_RD(device,
                    &f_ready,
                    &f_psai_write_ok,
                    &f_stuck_out,
                    &f_early_reseed,
                    &f_test_ready,
                    &f_gen_pkt_error,
                    &f_instantiated,
                    &f_test_stuck_out,
                    &blocks_available,
                    &f_need_clock);

    EIP74_GENERATE_CNT_RD(device, &gen_block_count);
    EIP74_RESEED_THR_RD(device, &reseed_thr);

    status_p->generate_block_count = gen_block_count;
    status_p->f_stuck_out = f_stuck_out;
    status_p->f_not_initialized = f_gen_pkt_error;
    status_p->f_reseed_error = gen_block_count == reseed_thr;
    status_p->f_reseed_warning = f_early_reseed;
    status_p->f_instantiated = f_instantiated;
    status_p->available_count = blocks_available;

    return EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip74_clear
 */
eip74_error_t
eip74_clear(
        eip74_io_area_t * const io_area_p)
{
    device_handle_t device;
    volatile eip74_true_io_area_t * const true_io_area_p =
        (eip74_true_io_area_t *)io_area_p;
    EIP74_CHECK_POINTER(io_area_p);

    device = true_io_area_p->device;

    EIP74_INTACK_WR(device, false, true, false);

    return EIP74_NO_ERROR;
}


/* end of file eip74.c */
