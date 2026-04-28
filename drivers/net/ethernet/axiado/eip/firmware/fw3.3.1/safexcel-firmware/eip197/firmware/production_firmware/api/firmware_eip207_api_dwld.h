/* firmware_eip207_api_dwld.h
 *
 * EIP-207 Firmware Download API:
 *
 * This API is defined by the EIP-207 Classification Firmware
 *
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : firmware_eip197                                          */
/*   Version       : 3.3.1                                                    */
/*   configuration : FIRMWARE-GENERIC                                         */
/*                                                                            */
/* Date          : 2021-Jun-11                                                */
/*                                                                            */
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

#ifndef FIRMWARE_EIP207_API_DWLD_H_
#define FIRMWARE_EIP207_API_DWLD_H_

/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

#include "basic_defs.h"         // u32


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

// IPUE firmware program counter value where the engine must be started from
// in order to perform the firmware version check in the debug mode
#define FIRMWARE_EIP207_DWLD_IPUE_VERSION_CHECK_DBG_PROG_CNTR      0x07E0

// IFPP firmware program counter value where the engine must be started from
// in order to perform the firmware version check in the debug mode
#define FIRMWARE_EIP207_DWLD_IFPP_VERSION_CHECK_DBG_PROG_CNTR      0x0FE0

// OPUE firmware program counter value where the engine must be started from
// in order to perform the firmware version check in the debug mode
#define FIRMWARE_EIP207_DWLD_OPUE_VERSION_CHECK_DBG_PROG_CNTR      0x04E0

// OFPP firmware program counter value where the engine must be started from
// in order to perform the firmware version check in the debug mode
#define FIRMWARE_EIP207_DWLD_OFPP_VERSION_CHECK_DBG_PROG_CNTR      0x04E0

// Administration RAM byte offsets as opposed to PE_n_ICE_SCRATCH_RAM
// 1 KB memory area base address in Classification Engine n
#define FIRMWARE_EIP207_DWLD_WORD_OFFS                            4
#define FIRMWARE_EIP207_DWLD_VERSION_BASE                         0

// Input Pull-Up micro-Engine version word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_IPUE_VERSION_BYTE_OFFSET    \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x00 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Input Flow Post-Processor micro-Engine version word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_IFPP_VERSION_BYTE_OFFSET    \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x02 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Output Pull-Up micro-Engine version word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_OPUE_VERSION_BYTE_OFFSET    \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x00 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Output Flow Post-Processor micro-Engine version word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_OFPP_VERSION_BYTE_OFFSET    \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x02 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Input Pull-Up micro-Engine control word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_IPUE_CTRL_BYTE_OFFSET       \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x05 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Input Flow Post-Processor micro-engine control word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_IFPP_CTRL_BYTE_OFFSET       \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x06 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Output Pull-Up micro-Engine control word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_OPUE_CTRL_BYTE_OFFSET       \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x05 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

// Output Flow Post-Processor micro-engine control word byte offset
#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_OFPP_CTRL_BYTE_OFFSET       \
                    ((FIRMWARE_EIP207_DWLD_VERSION_BASE) + (0x06 * FIRMWARE_EIP207_DWLD_WORD_OFFS))

#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_INPUT_LAST_BYTE_COUNT \
                    (FIRMWARE_EIP207_DWLD_ADMIN_RAM_IFPP_CTRL_BYTE_OFFSET + 4)

#define FIRMWARE_EIP207_DWLD_ADMIN_RAM_OUTPUT_LAST_BYTE_COUNT \
                    (FIRMWARE_EIP207_DWLD_ADMIN_RAM_OFPP_CTRL_BYTE_OFFSET + 4)

typedef struct
{
    unsigned int version_ma_mi_pa;
    unsigned int major;
    unsigned int minor;
    unsigned int patch_level;
    const u32 * image_p;
    unsigned int word_count;

} firmware_eip207_dwld_t;


#define FIRMWARE_EIP207_IPUE_NAME "firmware_eip207_ipue.bin"
#define FIRMWARE_EIP207_IFPP_NAME "firmware_eip207_ifpp.bin"
#define FIRMWARE_EIP207_OPUE_NAME "firmware_eip207_opue.bin"
#define FIRMWARE_EIP207_OFPP_NAME "firmware_eip207_ofpp.bin"
#define FIRMWARE_EIP207_VERSION_MAJOR 3
#ifndef CONFIG_ARCH_AX3005
#define FIRMWARE_EIP207_VERSION_MINOR 3
#define FIRMWARE_EIP207_VERSION_PATCH 1
#else
#define FIRMWARE_EIP207_VERSION_MINOR 5
#define FIRMWARE_EIP207_VERSION_PATCH 0
#endif


/*----------------------------------------------------------------------------
 * Firmware helper functions
 */

/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_ipue_get_references
 *
 * This function returns references to the IPUE firmware image
 *
 * fw_p (output)
 *     Pointer to the memory location where the IPUE firmware parameters
 *     as defined by the firmware_eip207_dwld_t data structure will be stored
 *
 * Return value
 *     None
 */
void
firmware_eip207_dwld_ipue_get_references(
        firmware_eip207_dwld_t * const fw_p);


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_ifpp_get_references
 *
 * This function returns references to the IFPP firmware image
 *
 * fw_p (output)
 *     Pointer to the memory location where the IFPP firmware parameters
 *     as defined by the firmware_eip207_dwld_t data structure will be stored
 *
 * Return value
 *     None
 */
void
firmware_eip207_dwld_ifpp_get_references(
        firmware_eip207_dwld_t * const fw_p);


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_opue_get_references
 *
 * This function returns references to the OPUE firmware image
 *
 * fw_p (output)
 *     Pointer to the memory location where the OPUE firmware parameters
 *     as defined by the firmware_eip207_dwld_t data structure will be stored
 *
 * Return value
 *     None
 */
void
firmware_eip207_dwld_opue_get_references(
        firmware_eip207_dwld_t * const fw_p);


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_ofpp_get_references
 *
 * This function returns references to the OFPP firmware image
 *
 * fw_p (output)
 *     Pointer to the memory location where the IFPP firmware parameters
 *     as defined by the firmware_eip207_dwld_t data structure will be stored
 *
 * Return value
 *     None
 */
void
firmware_eip207_dwld_ofpp_get_references(
        firmware_eip207_dwld_t * const fw_p);


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_version_read
 *
 * This function reads the EIP-207 firmware major, minor and patch level
 * numbers from the provided 32-bit value
 *
 * value (input)
 *     32-bit value that can be read from the Administration RAM byte offset
 *     FIRMWARE_EIP207_DWLD_ADMIN_RAM_IPUE_VERSION_BYTE_OFFSET or
 *     FIRMWARE_EIP207_DWLD_ADMIN_RAM_IFPP_VERSION_BYTE_OFFSET
 *
 * major_p (input)
 *     Pointer to the memory where the firmware major number will be stored
 *
 * minor_p (input)
 *     Pointer to the memory where the firmware minor number will be stored
 *
 * patch_level_p (input)
 *     Pointer to the memory where the firmware patch level number will
 *     be stored
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_dwld_version_read(
        const u32 value,
        unsigned int * const major_p,
        unsigned int * const minor_p,
        unsigned int * const patch_level_p)
{
    *major_p       = (unsigned int)((value >> 8) & MASK_4_BITS);
    *minor_p       = (unsigned int)((value >> 4) & MASK_4_BITS);
    *patch_level_p  = (unsigned int)((value)      & MASK_4_BITS);
}


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_ipue_version_updated_read
 *
 * This function reads the IPUE firmware version updated bit
 * from the provided 32-bit value
 *
 * value (input)
 *     32-bit value that can be read from the Administration RAM byte offset
 *     FIRMWARE_EIP207_DWLD_ADMIN_RAM_IPUE_CTRL_BYTE_OFFSET
 *
 * f_version_updated_p (output)
 *     Pointer to the memory where the firmware version update flag
 *     will be stored. The firmware updates this flag when started in
 *     the debug mode at the Program counter
 *     FIRMWARE_EIP207_DWLD_IPUE_VERSION_CHECK_DBG_PROG_CNTR
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_dwld_ipue_version_updated_read(
        const u32 value,
        bool * const f_version_updated_p)
{
    *f_version_updated_p  = ((value & BIT_0) != 0);
}


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_ifpp_version_updated_read
 *
 * This function reads the IFPP firmware version updated bit
 * from the provided 32-bit value
 *
 * value (input)
 *     32-bit value that can be read from the Administration RAM byte offset
 *     FIRMWARE_EIP207_DWLD_ADMIN_RAM_IFPP_CTRL_BYTE_OFFSET
 *
 * f_version_updated_p (output)
 *     Pointer to the memory where the firmware version update flag
 *     will be stored. The firmware updates this flag when started in
 *     the debug mode at the Program counter
 *     FIRMWARE_EIP207_DWLD_IFPP_VERSION_CHECK_DBG_PROG_CNTR
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_dwld_ifpp_version_updated_read(
        const u32 value,
        bool * const f_version_updated_p)
{
    *f_version_updated_p  = ((value & BIT_0) != 0);
}


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_opue_version_updated_read
 *
 * This function reads the OPUE firmware version updated bit
 * from the provided 32-bit value
 *
 * value (input)
 *     32-bit value that can be read from the Administration RAM byte offset
 *     FIRMWARE_EIP207_DWLD_ADMIN_RAM_OPUE_CTRL_BYTE_OFFSET
 *
 * f_version_updated_p (output)
 *     Pointer to the memory where the firmware version update flag
 *     will be stored. The firmware updates this flag when started in
 *     the debug mode at the Program counter
 *     FIRMWARE_EIP207_DWLD_OPUE_VERSION_CHECK_DBG_PROG_CNTR
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_dwld_opue_version_updated_read(
        const u32 value,
        bool * const f_version_updated_p)
{
    *f_version_updated_p  = ((value & BIT_0) != 0);
}


/*----------------------------------------------------------------------------
 * firmware_eip207_dwld_ofpp_version_updated_read
 *
 * This function reads the OFPP firmware version updated bit
 * from the provided 32-bit value
 *
 * value (input)
 *     32-bit value that can be read from the Administration RAM byte offset
 *     FIRMWARE_EIP207_DWLD_ADMIN_RAM_OFPP_CTRL_BYTE_OFFSET
 *
 * f_version_updated_p (output)
 *     Pointer to the memory where the firmware version update flag
 *     will be stored. The firmware updates this flag when started in
 *     the debug mode at the Program counter
 *     FIRMWARE_EIP207_DWLD_OFPP_VERSION_CHECK_DBG_PROG_CNTR
 *
 * Return value
 *     None
 */
static inline void
firmware_eip207_dwld_ofpp_version_updated_read(
        const u32 value,
        bool * const f_version_updated_p)
{
    *f_version_updated_p  = ((value & BIT_0) != 0);
}

#endif /* FIRMWARE_EIP207_API_DWLD_H_ */
/* end of file firmware_eip207_api_dwld.h */
