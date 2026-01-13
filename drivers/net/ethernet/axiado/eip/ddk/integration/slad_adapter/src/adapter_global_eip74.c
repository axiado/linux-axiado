/* api_global_eip74.c
 *
 * Deterministic Random Bit Generator (EIP-74) Global Control Initialization
 * Adapter. The EIP-74 is used to generate pseudo-random IVs for outbound
 * operations in CBC mode.
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
#include "api_global_eip74.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// configuration.
#include "c_adapter_eip74.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // bool, u8

// memcpy
#include "clib.h"

// EIP-73 Driver Library.
#include "eip74.h"

#include "device_types.h"       // device_handle_t
#include "device_mgmt.h"        // Device_find

// Logging API
#include "log.h"                // Log_*, LOG_*

// Adapter interrupts API
#include "adapter_interrupts.h" // Adapter_Interrupt_*

// Runtime power Management device Macros API
#include "rpm_device_macros.h"  // RPM_*


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */

/* Put all adapter local variables in one structure */
static struct {
    eip74_io_area_t io_area;
#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
    global_control74_notify_function_t notify_cb_func;
#endif
#ifdef ADAPTER_PEC_RPM_EIP74_DEVICE0_ID
    global_control74_configuration_t cached_config;
    bool f_initialized;
#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
    bool f_interrupt_enabled;
#endif
#endif
} eip74_state;


static const  global_control74_capabilities_t global_capabilities_string =
{
  "EIP-74 v_._p_"// sz_text_description
};


/*----------------------------------------------------------------------------
 * global_control74_lib_init
 */
static global_control74_error_t
global_control74_lib_init(
        const global_control74_configuration_t * const configuration_p,
        const u8 * const entropy_p);


#ifdef ADAPTER_PEC_RPM_EIP74_DEVICE0_ID
/*----------------------------------------------------------------------------
 * global_control74_lib_resune
 */
static int
global_control74_lib_resune(
        void *p)
{
    u8 entropy[48];
    IDENTIFIER_NOT_USED(p);
    if (eip74_state.f_initialized)
    {
        /* Note we should add fresh random data here */
        zeroinit(entropy);
        if (global_control74_lib_init(&eip74_state.cached_config, entropy) !=
            GLOBAL_CONTROL_EIP74_NO_ERROR)
        {
            return -1;
        }

#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
        if (eip74_state.f_initialized && eip74_state.f_interrupt_enabled)
        {
            adapter_interrupt_disable(ADAPTER_EIP74_ERR_IRQ, 0);
            adapter_interrupt_disable(ADAPTER_EIP74_RES_IRQ, 0);
        }
#endif
    }
    return 0;
}


/*----------------------------------------------------------------------------
 * global_control74_lib_suspend
 */
static int
global_control74_lib_suspend(
        void *p)
{
    IDENTIFIER_NOT_USED(p);
#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
    if (eip74_state.f_initialized && eip74_state.f_interrupt_enabled)
    {
        adapter_interrupt_disable(ADAPTER_EIP74_ERR_IRQ, 0);
        adapter_interrupt_disable(ADAPTER_EIP74_RES_IRQ, 0);
    }
#endif
    return 0;
}

#endif

/*----------------------------------------------------------------------------
 * global_control74_lib_copy_key_mat
 *
 * Copy a key represented as a byte array into a word array..
 *
 * destination_p (input)
 *   Destination (word-aligned) of the word array
 *
 * source_p (input)
 *   source (byte aligned) of the data.
 *
 * key_byte_count (input)
 *   size of the key in bytes.
 *
 * destination_p is allowed to be a null pointer, in which case no key
 * will be written.
 */
static void
global_control74_lib_copy_key_mat(
        u32 * const destination_p,
        const u8 * const source_p,
        const unsigned int key_byte_count)
{
    u32 *dst = destination_p;
    const u8 *src = source_p;
    unsigned int i,j;
    u32 w;
    if (destination_p == NULL)
        return;
    for(i=0; i < key_byte_count / sizeof(u32); i++)
    {
        w=0;
        for(j=0; j<sizeof(u32); j++)
            w=(w<<8)|(*src++);
        *dst++ = w;
    }
}

#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
/*----------------------------------------------------------------------------
 * global_control74_interrupt_handler_notify
 */
static void
global_control74_interrupt_handler_notify(
        const int n_irq,
        const unsigned int flags)
{
    global_control74_notify_function_t cb_func = eip74_state.notify_cb_func;

    IDENTIFIER_NOT_USED(n_irq);
    IDENTIFIER_NOT_USED(flags);

    LOG_INFO("global_control74_interrupt_handler_notify\n");
#ifdef ADAPTER_PEC_RPM_EIP74_DEVICE0_ID
    eip74_state.f_interrupt_enabled = true;
#endif

    eip74_state.notify_cb_func = NULL;
    if (cb_func != NULL)
    {
        LOG_INFO("\t Invoking callback\n");
        cb_func();
    }
}
#endif
/*----------------------------------------------------------------------------
 * global_control74_lib_init
 *
 * This function performs the initialization of the EIP-74 Deterministic
 * Random Bit Generator.
 *
 * Note: the device was already found and the io_area is already initialized.
 *
 * configuration_p (input)
 *     configuration parameters of the DRBG.
 *
 * entropy_p (input)
 *     Pointer to a string of exactly 48 bytes that serves as the entropy.
 *     to initialize the DRBG.
 *
 * Return value
 *     GLOBAL_CONTROL_EIP74_NO_ERROR : initialization performed successfully
 *     GLOBAL_CONTROL_EIP74_ERROR_INTERNAL : initialization failed
 */
static global_control74_error_t
global_control74_lib_init(
        const global_control74_configuration_t * const configuration_p,
        const u8 * const entropy_p)
{
    eip74_error_t rc;
    eip74_configuration_t conf;
    unsigned loop_counter = ADAPTER_EIP74_RESET_MAX_RETRIES;
    u32 entropy[12];

    rc = eip74_reset(&eip74_state.io_area);
    do
    {
        if (rc == EIP74_BUSY_RETRY_LATER)
        {
            loop_counter--;
            if (loop_counter == 0)
            {
                LOG_CRIT("%s EIP74 reset timed out\n",__func__);
                return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
            }
            rc = eip74_reset_is_done(&eip74_state.io_area);
        }
        else if (rc != EIP74_NO_ERROR)
        {
            LOG_CRIT("%s EIP74 reset error\n",__func__);
            return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
        }
    } while (rc != EIP74_NO_ERROR);

    if (configuration_p->generate_block_size == 0)
    {
        conf.generate_block_size =  ADAPTER_EIP74_GEN_BLK_SIZE;
    }
    else
    {
        conf.generate_block_size = configuration_p->generate_block_size;
    }

    if (configuration_p->reseed_thr == 0)
    {
        conf.reseed_thr =  ADAPTER_EIP74_RESEED_THR;
    }
    else
    {
        conf.reseed_thr = configuration_p->reseed_thr;
    }

    if (configuration_p->reseed_thr_early == 0)
    {
        conf.reseed_thr_early =  ADAPTER_EIP74_RESEED_THR_EARLY;
    }
    else
    {
        conf.reseed_thr_early = configuration_p->reseed_thr_early;
    }

    rc = eip74_configure(&eip74_state.io_area, &conf);
    if (rc != EIP74_NO_ERROR)
    {
        LOG_CRIT("%s EIP74 could not be configured\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
    }

    global_control74_lib_copy_key_mat(entropy, entropy_p, 48);

    rc = eip74_instantiate(&eip74_state.io_area, entropy, configuration_p->f_stuck_out);

    if (rc != EIP74_NO_ERROR)
    {
        LOG_CRIT("%s EIP74 could not be instantiated\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
    }

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * global_control74_capabilities_get
 */
void
global_control74_capabilities_get(
        global_control74_capabilities_t * const capabilities_p)
{
    device_handle_t device;
    u8 versions[3];
    LOG_INFO("\n\t\t\t global_control74_capabilities_get\n");

    memcpy(capabilities_p, &global_capabilities_string,
           sizeof(global_capabilities_string));

    device = eip_device_find(ADAPTER_EIP74_DEVICE_NAME);
    if (device == NULL)
    {
        LOG_CRIT("%s EIP74 device not found\n",__func__);
        return;
    }

    {
        eip74_capabilities_t capabilities;
        eip74_error_t rc;

        if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                RPM_FLAG_SYNC) != RPM_SUCCESS)
            return;

        rc = eip74_hw_revision_get(device, &capabilities);

        (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                       RPM_FLAG_ASYNC);

        if (rc != EIP74_NO_ERROR)
        {
            LOG_CRIT("%s EIP74_Capaboilities_Get() failed\n",__func__);
            return;
        }

        log_formatted_message(
            "EIP74 options: Nof Clients=%u Nof AESCores=%u\n"
            "\t\tAESSpeed=%u fifo_depth=%u\n",
            capabilities.hw_options.client_count,
            capabilities.hw_options.aes_core_count,
            capabilities.hw_options.aes_speed,
            capabilities.hw_options.fifo_depth);


        versions[0] = capabilities.hw_revision.maj_hw_revision;
        versions[1] = capabilities.hw_revision.min_hw_revision;
        versions[2] = capabilities.hw_revision.hw_patch_level;
    }

    {
        char * p = capabilities_p->sz_text_description;
        int ver_index = 0;
        int i = 0;

        while(p[i])
        {
            if (p[i] == '_' && ver_index < 3)
            {
                if (versions[ver_index] > 9)
                    p[i] = '?';
                else
                    p[i] = '0' + versions[ver_index];

                ver_index++;
            }

            i++;
        }
    }
}


/*----------------------------------------------------------------------------
 * global_control74_init
 */
global_control74_error_t
global_control74_init(
        const global_control74_configuration_t * const configuration_p,
        const u8 * const entropy_p)
{
    eip74_error_t rc;
    device_handle_t device;

    LOG_INFO("\n\t\t\t global_control74_init\n");

    device = eip_device_find(ADAPTER_EIP74_DEVICE_NAME);
    if (device == NULL)
    {
        LOG_CRIT("%s EIP74 device not found\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_NOT_IMPLEMENTED;
    }


    rc = eip74_init(&eip74_state.io_area, device);
    if (rc != EIP74_NO_ERROR)
    {
        LOG_CRIT("%s EIP74 could not be initialized\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_NOT_IMPLEMENTED;
    }

    if (RPM_DEVICE_INIT_START_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                    global_control74_lib_suspend,
                                    GlobalControl74Lib_Resume) != RPM_SUCCESS)
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;

    if (global_control74_lib_init(configuration_p,entropy_p) !=
        GLOBAL_CONTROL_EIP74_NO_ERROR)
    {
        (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID);

        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
    }


#ifdef ADAPTER_PEC_RPM_EIP74_DEVICE0_ID
    eip74_state.f_initialized = true;
    eip74_state.cached_config = *configuration_p;
#endif
    (void)RPM_DEVICE_INIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID);


#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
    adapter_interrupt_set_handler(ADAPTER_EIP74_ERR_IRQ,
                                 global_control74_interrupt_handler_notify);
    adapter_interrupt_set_handler(ADAPTER_EIP74_RES_IRQ,
                                 global_control74_interrupt_handler_notify);
#endif

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}



/*----------------------------------------------------------------------------
 * global_control74_uninit
 */
global_control74_error_t
global_control74_uninit(void)
{
    eip74_error_t rc;
    unsigned loop_counter = ADAPTER_EIP74_RESET_MAX_RETRIES;

    LOG_INFO("\n\t\t\t global_control74_uninit\n");

#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
    adapter_interrupt_set_handler(ADAPTER_EIP74_ERR_IRQ, NULL);
    adapter_interrupt_set_handler(ADAPTER_EIP74_RES_IRQ, NULL);
    adapter_interrupt_disable(ADAPTER_EIP74_ERR_IRQ, 0);
    adapter_interrupt_disable(ADAPTER_EIP74_RES_IRQ, 0);
#endif
    (void)RPM_DEVICE_UNINIT_START_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID, false);

    rc = eip74_reset(&eip74_state.io_area);
    do
    {
        if (rc == EIP74_BUSY_RETRY_LATER)
        {
            loop_counter--;
            if (loop_counter == 0)
            {
                LOG_CRIT("%s EIP74 reset timed out\n",__func__);
                (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID);
                return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
            }
            rc = eip74_reset_is_done(&eip74_state.io_area);
        }
        else if (rc != EIP74_NO_ERROR)
        {
            LOG_CRIT("%s EIP74 reset error\n",__func__);
            (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID);

            return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
        }
    } while (rc != EIP74_NO_ERROR);
    (void)RPM_DEVICE_UNINIT_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID);

#ifdef ADAPTER_PEC_RPM_EIP74_DEVICE0_ID
    eip74_state.f_initialized = false;
#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
    eip74_state.f_interrupt_enabled = false;
#endif
#endif

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * global_control74_reseed
 */
global_control74_error_t
global_control74_reseed(
        const u8 * const entropy_p)
{
    eip74_error_t rc;
    u32 entropy[12];
    LOG_INFO("\n\t\t\t global_control74_reseed\n");


    global_control74_lib_copy_key_mat(entropy, entropy_p, 48);

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;

    rc = eip74_reseed(&eip74_state.io_area, entropy);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc != EIP74_NO_ERROR)
    {
        LOG_CRIT("%s EIP74 could not be reseeded\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
    }

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * global_control74_status_get
 */
global_control74_error_t
global_control74_status_get(
        global_control74_status_t * const status_p)
{
    eip74_error_t rc;
    eip74_status_t status;
    LOG_INFO("\n\t\t\t global_control74_status_get\n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;

    rc = eip74_status_get(&eip74_state.io_area, &status);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc != EIP74_NO_ERROR)
    {
        LOG_CRIT("%s EIP74 status could not be read\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
    }

    status_p->generate_block_count = status.generate_block_count;
    status_p->f_stuck_out = status.f_stuck_out;
    status_p->f_not_initialized = status.f_not_initialized;
    status_p->f_reseed_error = status.f_reseed_error;
    status_p->f_reseed_warning = status.f_reseed_warning;
    status_p->f_instantiated = status.f_instantiated;
    status_p->available_count = status.available_count;

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}

/*----------------------------------------------------------------------------
 * global_control74_clear
 */
global_control74_error_t
global_control74_clear(void)
{
    eip74_error_t rc;
    LOG_INFO("\n\t\t\t global_control74_clear\n");

    if (RPM_DEVICE_IO_START_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                  RPM_FLAG_SYNC) != RPM_SUCCESS)
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;

    rc = eip74_clear(&eip74_state.io_area);

    (void)RPM_DEVICE_IO_STOP_MACRO(ADAPTER_GLOBAL_RPM_EIP74_DEVICE_ID,
                                   RPM_FLAG_ASYNC);

    if (rc != EIP74_NO_ERROR)
    {
        LOG_CRIT("%s EIP74 could not be cleared\n",__func__);
        return GLOBAL_CONTROL_EIP74_ERROR_INTERNAL;
    }

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}


#ifdef ADAPTER_EIP74_INTERRUPTS_ENABLE
/*----------------------------------------------------------------------------
 * global_control74_notify_request
 */
global_control74_error_t
global_control74_notify_request(
        global_control74_notify_function_t cb_func_p)
{
    LOG_INFO("\n\t\t\t global_control74_notify_request\n");
    IDENTIFIER_NOT_USED(cb_func_p);

    eip74_state.notify_cb_func = cb_func_p;
    if (cb_func_p != NULL)
    {
        adapter_interrupt_enable(ADAPTER_EIP74_ERR_IRQ, 0);
        adapter_interrupt_enable(ADAPTER_EIP74_RES_IRQ, 0);
#ifdef ADAPTER_PEC_RPM_EIP74_DEVICE0_ID
        eip74_state.f_interrupt_enabled = true;
#endif
    }

    return GLOBAL_CONTROL_EIP74_NO_ERROR;
}
#endif


/* end of file adapter_global_eip74.c */
