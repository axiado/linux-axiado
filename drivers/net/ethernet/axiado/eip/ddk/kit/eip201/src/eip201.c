/* eip201_sl.c
 *
 * Driver Library for the Security-IP-201 Advanced Interrupt Controller.
 */

/*****************************************************************************
* Copyright (c) 2007-2020 by Rambus, Inc. and/or its subsidiaries.
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

// Top-level Interrupt controller configuration
#include "c_eip201.h"           // configuration

// Driver Framework Basic Defs API
#include "basic_defs.h"         // u32, inline, etc.

// Interrupt controller API
#include "eip201.h"             // the API we will implement

// Driver Framework device API
#include "device_rw.h"          // device_read32/Write32

// create a constant where all unused interrupts are '1'
#if (EIP201_STRICT_ARGS_MAX_NUM_OF_INTERRUPTS < 32)
#define EIP201_NOTUSEDIRQ_MASK (u32) \
                    (~((1 << EIP201_STRICT_ARGS_MAX_NUM_OF_INTERRUPTS)-1))
#else
#define EIP201_NOTUSEDIRQ_MASK 0
#endif

#ifdef EIP201_STRICT_ARGS
#define EIP201_CHECK_IF_IRQ_SUPPORTED(_irqs) \
        if (_irqs & EIP201_NOTUSEDIRQ_MASK) \
            return EIP201_STATUS_UNSUPPORTED_IRQ;
#else
#define EIP201_CHECK_IF_IRQ_SUPPORTED(_irqs)
#endif /* EIP201_STRICT_ARGS */


/*----------------------------------------------------------------------------
 *  EIP201 registers
 */
enum
{
    EIP201_REGISTER_OFFSET_POL_CTRL     = EIP201_LO_REG_BASE+0,
    EIP201_REGISTER_OFFSET_TYPE_CTRL    = EIP201_LO_REG_BASE+4,
    EIP201_REGISTER_OFFSET_ENABLE_CTRL  = EIP201_LO_REG_BASE+8,
    EIP201_REGISTER_OFFSET_RAW_STAT     = EIP201_HI_REG_BASE+12,
    EIP201_REGISTER_OFFSET_ENABLE_SET   = EIP201_LO_REG_BASE+12,
    EIP201_REGISTER_OFFSET_ENABLED_STAT = EIP201_HI_REG_BASE+16,
    EIP201_REGISTER_OFFSET_ACK          = EIP201_LO_REG_BASE+16,
    EIP201_REGISTER_OFFSET_ENABLE_CLR   = EIP201_LO_REG_BASE+20,
    EIP201_REGISTER_OFFSET_OPTIONS      = EIP201_HI_REG_BASE+24,
    EIP201_REGISTER_OFFSET_VERSION      = EIP201_HI_REG_BASE+28
};

// this implementation supports only the EIP-201 HW1.1 and HW1.2
// 0xC9  = 201
// 0x39  = binary inverse of 0xC9
#define EIP201_SIGNATURE      0x36C9
#define EIP201_SIGNATURE_MASK 0xffff

/*----------------------------------------------------------------------------
 * eip201_read32
 *
 * This routine reads from a Register location in the EIP201, applying
 * endianness swapping when required (depending on configuration).
 */
static inline int
eip201_read32(
        device_handle_t device,
        const unsigned int offset,
        u32 * const value_p)
{
    return device_read32_check(device, offset, value_p);
}


/*----------------------------------------------------------------------------
 * eip201_write32
 *
 * This routine writes to a Register location in the EIP201, applying
 * endianness swapping when required (depending on configuration).
 */
static inline int
eip201_write32(
        device_handle_t device,
        const unsigned int offset,
        const u32 value)
{
    return device_write32(device, offset, value);
}


/*----------------------------------------------------------------------------
 * eip201_config_change
 */
#ifndef EIP201_REMOVE_CONFIG_CHANGE
eip201_status_t
eip201_config_change(
        device_handle_t device,
        const eip201_source_bitmap_t sources,
        const eip201_config_t config)
{
    u32 value;
    u32 new_pol = 0;
    u32 new_type = 0;
    int rc;
    EIP201_CHECK_IF_IRQ_SUPPORTED(sources);

    /*
        EIP201_CONFIG_ACTIVE_LOW,       // type=0, Pol=0
        EIP201_CONFIG_ACTIVE_HIGH,      // type=0, Pol=1
        EIP201_CONFIG_FALLING_EDGE,     // type=1, Pol=0
        EIP201_CONFIG_RISING_EDGE       // type=1, Pol=1
    */

    // do we want type=1?
    if (config == EIP201_CONFIG_FALLING_EDGE ||
        config == EIP201_CONFIG_RISING_EDGE)
    {
        new_type = sources;
    }

    // do we want Pol=1?
    if (config == EIP201_CONFIG_ACTIVE_HIGH ||
        config == EIP201_CONFIG_RISING_EDGE)
    {
        new_pol = sources;
    }

    if (sources)
    {
        // modify polarity register
        rc = eip201_read32(device, EIP201_REGISTER_OFFSET_POL_CTRL, &value);
        if (rc) return rc;
        value &= ~sources;
        value |= new_pol;
        rc = eip201_write32(device, EIP201_REGISTER_OFFSET_POL_CTRL, value);
        if (rc) return rc;

        // modify type register
        rc = eip201_read32(device, EIP201_REGISTER_OFFSET_TYPE_CTRL, &value);
        if (rc) return rc;
        value &= ~sources;
        value |= new_type;
        rc = eip201_write32(device, EIP201_REGISTER_OFFSET_TYPE_CTRL, value);
        if (rc) return rc;
    }

    return EIP201_STATUS_SUCCESS;
}
#endif /* EIP201_REMOVE_CONFIG_CHANGE */


/*----------------------------------------------------------------------------
 * eip201_config_read
 */
#ifndef EIP201_REMOVE_CONFIG_READ

static const eip201_config_t eip201_setting2_config[4] =
{
    EIP201_CONFIG_ACTIVE_LOW,       // type=0, Pol=0
    EIP201_CONFIG_ACTIVE_HIGH,      // type=0, Pol=1
    EIP201_CONFIG_FALLING_EDGE,     // type=1, Pol=0
    EIP201_CONFIG_RISING_EDGE       // type=1, Pol=1
};

eip201_config_t
eip201_config_read(
        device_handle_t device,
        const eip201_source_t source)
{
    u32 value;
    unsigned char setting = 0;
    int rc = 0;

    rc = eip201_read32(device, EIP201_REGISTER_OFFSET_TYPE_CTRL, &value);
    if (rc) return rc;
    if (value & source)
    {
        // type=1, thus edge
        setting += 2;
    }

    eip201_read32(device, EIP201_REGISTER_OFFSET_POL_CTRL, &value);
    if (value & source)
    {
        // Pol=1, this rising edge or active high
        setting++;
    }

    return eip201_setting2_config[setting];
}
#endif /* EIP201_REMOVE_CONFIG_READ */


/*----------------------------------------------------------------------------
 * eip201_source_mask_enable_source
 *
 * See header file for function specifications.
 */
#ifndef EIP201_REMOVE_SOURCEMASK_ENABLESOURCE
eip201_status_t
eip201_source_mask_enable_source(
        device_handle_t device,
        const eip201_source_bitmap_t sources)
{
    int rc;
    EIP201_CHECK_IF_IRQ_SUPPORTED(sources);

    rc = eip201_write32(
            device,
            EIP201_REGISTER_OFFSET_ENABLE_SET,
            sources);

    return rc;
}
#endif /* EIP201_REMOVE_SOURCEMASK_ENABLESOURCE */


/*----------------------------------------------------------------------------
 * eip201_source_mask_disable_source
 */
#ifndef EIP201_REMOVE_SOURCEMASK_DISABLESOURCE
eip201_status_t
eip201_source_mask_disable_source(
        device_handle_t device,
        const eip201_source_bitmap_t sources)
{
    int rc;
    rc = eip201_write32(
            device,
            EIP201_REGISTER_OFFSET_ENABLE_CLR,
            sources);

    return rc;
}
#endif /* EIP201_REMOVE_SOURCEMASK_DISABLESOURCE */


/*----------------------------------------------------------------------------
 * eip201_source_mask_source_is_enabled
 */
#ifndef EIP201_REMOVE_SOURCEMASK_SOURCEISENABLED
bool
eip201_source_mask_source_is_enabled(
        device_handle_t device,
        const eip201_source_t source)
{
    int rc;
    u32 source_masks;

    rc =  eip201_read32(
                        device,
        EIP201_REGISTER_OFFSET_ENABLE_CTRL, &source_masks);

    if (rc) return false;
    if (source_masks & source)
        return true;

    return false;
}
#endif /* EIP201_REMOVE_SOURCEMASK_SOURCEISENABLED */


/*----------------------------------------------------------------------------
 * eip201_source_mask_read_all
 */
#ifndef EIP201_REMOVE_SOURCEMASK_READALL
eip201_source_bitmap_t
eip201_source_mask_read_all(
        device_handle_t device)
{
    u32 value;
    eip201_read32(device, EIP201_REGISTER_OFFSET_ENABLE_CTRL, &value);
    return value;
}
#endif /* EIP201_REMOVE_SOURCEMASK_READALL */


/*----------------------------------------------------------------------------
 * eip201_source_status_is_enabled_source_pending
 */
#ifndef EIP201_REMOVE_SOURCESTATUS_ISENABLEDSOURCEPENDING
bool
eip201_source_status_is_enabled_source_pending(
        device_handle_t device,
        const eip201_source_t source)
{
    u32 statuses;
    int rc;

    rc = eip201_read32(device, EIP201_REGISTER_OFFSET_ENABLED_STAT, &statuses);
    if (rc) return false;

    if (statuses & source)
        return true;

    return false;
}
#endif /* EIP201_REMOVE_SOURCESTATUS_ISENABLEDSOURCEPENDING */


/*----------------------------------------------------------------------------
 * eip201_source_status_is_raw_source_pending
 */
#ifndef EIP201_REMOVE_SOURCESTATUS_ISRAWSOURCEPENDING
bool
eip201_source_status_is_raw_source_pending(
        device_handle_t device,
        const eip201_source_t source)
{
    u32 statuses;
    int rc;

    rc = eip201_read32(device, EIP201_REGISTER_OFFSET_RAW_STAT, &statuses);
    if (rc) return false;

    if (statuses & source)
        return true;

    return false;
}
#endif /* EIP201_REMOVE_SOURCESTATUS_ISRAWSOURCEPENDING */


/*----------------------------------------------------------------------------
 * eip201_source_status_read_all_enabled
 */
#ifndef EIP201_REMOVE_SOURCESTATUS_READALLENABLED
eip201_source_bitmap_t
eip201_source_status_read_all_enabled(
        device_handle_t device)
{
    u32 value;
    eip201_read32(device, EIP201_REGISTER_OFFSET_ENABLED_STAT, &value);
    return value;
}
#endif /* EIP201_REMOVE_SOURCESTATUS_READALLENABLED */


/*----------------------------------------------------------------------------
 * eip201_source_status_read_all_raw
 */
#ifndef EIP201_REMOVE_SOURCESTATUS_READALLRAW
eip201_source_bitmap_t
eip201_source_status_read_all_raw(
        device_handle_t device)
{
    u32 value;
    eip201_read32(device, EIP201_REGISTER_OFFSET_RAW_STAT, &value);
    return value;
}
#endif /* EIP201_REMOVE_SOURCESTATUS_READALLRAW */


/*----------------------------------------------------------------------------
 * eip201_source_status_read_all_enabled_check
 */
#ifndef EIP201_REMOVE_SOURCESTATUS_READALLENABLED
eip201_status_t
eip201_source_status_read_all_enabled_check(
        device_handle_t device,
        eip201_source_bitmap_t * const statuses_p)
{
    return eip201_read32(device, EIP201_REGISTER_OFFSET_ENABLED_STAT, statuses_p);
}
#endif /* EIP201_REMOVE_SOURCESTATUS_READALLENABLED */


/*----------------------------------------------------------------------------
 * eip201_source_status_read_all_raw_check
 */
#ifndef EIP201_REMOVE_SOURCESTATUS_READALLRAW
eip201_status_t
eip201_source_status_read_all_raw_check(
        device_handle_t device,
        eip201_source_bitmap_t * const statuses_p)
{
    return eip201_read32(device, EIP201_REGISTER_OFFSET_RAW_STAT, statuses_p);
}
#endif /* EIP201_REMOVE_SOURCESTATUS_READALLRAW */


/*----------------------------------------------------------------------------
 * eip201_lib_detect
 *
 *  Detect the presence of EIP201 hardware.
 */
#ifndef EIP201_REMOVE_INITIALIZE
static eip201_status_t
eip201_lib_detect(
        device_handle_t device)
{
    u32 value;
    int rc;

    rc = eip201_read32(device, EIP201_REGISTER_OFFSET_VERSION, &value);
    if (rc) return rc;
    value &= EIP201_SIGNATURE_MASK;
    if ( value != EIP201_SIGNATURE)
        return EIP201_STATUS_UNSUPPORTED_HARDWARE_VERSION;

    // Prevent interrupts going of by disabling them
    rc = eip201_write32(device, EIP201_REGISTER_OFFSET_ENABLE_CTRL, 0);
    if (rc) return rc;

    // Get the number of interrupt sources
    rc = eip201_read32(device, EIP201_REGISTER_OFFSET_OPTIONS, &value);
    if (rc) return rc;
    // lowest 6 bits contain the number of inputs, which should be between 1-32
    value &= MASK_6_BITS;
    if (value == 0 || value > 32)
        return EIP201_STATUS_UNSUPPORTED_HARDWARE_VERSION;

    return EIP201_STATUS_SUCCESS;
}
#endif /* EIP201_REMOVE_INITIALIZE */


/*----------------------------------------------------------------------------
 * eip201_initialize API
 *
 *  See header file for function specification.
 */
#ifndef EIP201_REMOVE_INITIALIZE
eip201_status_t
eip201_initialize(
        device_handle_t device,
        const eip201_source_settings_t * settings_array_p,
        const unsigned int settings_count)
{
    eip201_source_bitmap_t active_low_sources = 0;
    eip201_source_bitmap_t active_high_sources = 0;
    eip201_source_bitmap_t falling_edge_sources = 0;
    eip201_source_bitmap_t rising_edge_sources = 0;
    eip201_source_bitmap_t enabled_sources = 0;
    int rc;

    // check presence of EIP201 hardware
    rc = eip201_lib_detect(device);
    if (rc) return rc;

    // disable all interrupts and set initial configuration
    rc = eip201_write32(device, EIP201_REGISTER_OFFSET_ENABLE_CTRL, 0);
    if (rc) return rc;
    rc = eip201_write32(device, EIP201_REGISTER_OFFSET_POL_CTRL, 0);
    if (rc) return rc;
    rc = eip201_write32(device, EIP201_REGISTER_OFFSET_TYPE_CTRL, 0);
    if (rc) return rc;

    // process the setting, if provided
    if (settings_array_p != NULL)
    {
        unsigned int i;

        for (i = 0; i < settings_count; i++)
        {
            // check
            const eip201_source_t source = settings_array_p[i].source;
            EIP201_CHECK_IF_IRQ_SUPPORTED(source);

            // determine polarity
            switch(settings_array_p[i].config)
            {
                case EIP201_CONFIG_ACTIVE_LOW:
                    active_low_sources |= source;
                    break;

                case EIP201_CONFIG_ACTIVE_HIGH:
                    active_high_sources |= source;
                    break;

                case EIP201_CONFIG_FALLING_EDGE:
                    falling_edge_sources |= source;
                    break;

                case EIP201_CONFIG_RISING_EDGE:
                    rising_edge_sources |= source;
                    break;

                default:
                    // invalid parameter
                    break;
            } // switch

            // determine enabled mask
            if (settings_array_p[i].f_enable)
                enabled_sources |= source;
        } // for
    }

    // program source configuration
    rc = eip201_config_change(
            device,
            active_low_sources,
            EIP201_CONFIG_ACTIVE_LOW);
    if (rc) return rc;

    rc = eip201_config_change(
            device,
            active_high_sources,
            EIP201_CONFIG_ACTIVE_HIGH);
    if (rc) return rc;

    rc = eip201_config_change(
            device,
            falling_edge_sources,
            EIP201_CONFIG_FALLING_EDGE);
    if (rc) return rc;

    rc = eip201_config_change(
            device,
            rising_edge_sources,
            EIP201_CONFIG_RISING_EDGE);
    if (rc) return rc;

    // the configuration change could have triggered the edge-detection logic
    // so acknowledge all edge-based interrupts immediately
    {
        const u32 value = falling_edge_sources | rising_edge_sources;
        rc = eip201_write32(device, EIP201_REGISTER_OFFSET_ACK, value);
        if (rc) return rc;
    }

    // set mask (enable required interrupts)
    rc = eip201_source_mask_enable_source(device, enabled_sources);

    return rc;
}
#endif /* EIP201_REMOVE_INITIALIZE */


/*----------------------------------------------------------------------------
 * eip201_acknowledge
 *
 * See header file for function specification.
 */
#ifndef EIP201_REMOVE_ACKNOWLEDGE
eip201_status_t
eip201_acknowledge(
        device_handle_t device,
        const eip201_source_bitmap_t sources)
{
    int rc;
    EIP201_CHECK_IF_IRQ_SUPPORTED(sources);

    rc = eip201_write32(device, EIP201_REGISTER_OFFSET_ACK, sources);

    return rc;
}
#endif /* EIP201_REMOVE_ACKNOWLEDGE */

/* end of file eip201_sl.c */
