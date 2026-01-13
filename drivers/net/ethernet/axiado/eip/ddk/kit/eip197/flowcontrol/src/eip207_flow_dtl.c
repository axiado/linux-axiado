/* eip207_flow_dtl.c
 *
 * Partial EIP-207 Flow Control Generic API implementation and
 * full EIP-207 Flow Control DTL API implementation
 */

/* -------------------------------------------------------------------------- */
/*                                                                            */
/*   Module        : ddk197                                                   */
/*   Version       : 5.7.2                                                    */
/*   configuration : DDK-197-BSD                                              */
/*                                                                            */
/*   Date          : 2025-Nov-13                                              */
/*                                                                            */
/* Copyright (c) 2008-2025 by Rambus, Inc. and/or its subsidiaries.           */
/*                                                                            */
/* Redistribution and use in source and binary forms, with or without         */
/* modification, are permitted provided that the following conditions are     */
/* met:                                                                       */
/*                                                                            */
/* 1. Redistributions of source code must retain the above copyright          */
/* notice, this list of conditions and the following disclaimer.              */
/*                                                                            */
/* 2. Redistributions in binary form must reproduce the above copyright       */
/* notice, this list of conditions and the following disclaimer in the        */
/* documentation and/or other materials provided with the distribution.       */
/*                                                                            */
/* 3. Neither the name of the copyright holder nor the names of its           */
/* contributors may be used to endorse or promote products derived from       */
/* this software without specific prior written permission.                   */
/*                                                                            */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS        */
/* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT          */
/* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR      */
/* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT       */
/* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     */
/* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT           */
/* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,      */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY      */
/* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT        */
/* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE      */
/* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.       */
/* -------------------------------------------------------------------------- */

/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */

// EIP-207 Driver Library Flow Control Generic API
#include "eip207_flow_dtl.h"

// EIP-207 Driver Library Flow Control Generic API
#include "eip207_flow_generic.h"


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Default configuration
#include "c_eip207_flow.h"

// Driver Framework Basic Definitions API
#include "basic_defs.h"                 // u32

// Driver Framework C Run-time Library API
#include "clib.h"                       // zeroinit, memset, memcpy

// Driver Framework device API
#include "device_types.h"               // device_handle_t

// Driver Framework DMA Resource API
#include "dmares_types.h"
#include "dmares_rw.h"

// EIP-207 Flow Control Driver Library internal interfaces
#include "eip207_flow_level0.h"         // EIP-207 Level 0 macros
#include "eip207_flow_hte_dscr_dtl.h"   // HTE descriptor DTL
#include "eip207_flow_internal.h"

// EIP-207 Firmware API
#include "firmware_eip207_api_flow_cs.h" // Classification API: Flow Control
#include "firmware_eip207_api_cs.h"      // Classification API: General


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_FLOW_MAX_RECORDS_PER_BUCKET              3


// Flow record input data required for the flow record creation
typedef struct
{
    // Bitwise of zero or more EIP207_FLOW_FLAG_* flags, see above
    u32 flags;

    // Physical (DMA/bus) address of the transform record
    eip207_flow_address_t xform_dma_addr;

    // Software flow record reference
    u32 sw_fr_reference;

    // Transform record type, true - large, false - small
    bool f_large;
} eip207_flow_fr_data_t;

// Transform record input data required for the transform record creation
typedef struct
{
    // Transform record type, true - large, false - small
    bool f_large;

} eip207_flow_tr_data_t;

// Transform record input data required for the Transform record creation
typedef struct
{
    // Record hash ID
    const eip207_flow_id_t * hash_id_p;

    // Flow record input data, fill with NULL if not used
    const eip207_flow_fr_data_t * fr_data_p;

    // Transform record input data, fill with NULL if not used
    const eip207_flow_tr_data_t * tr_data_p;

    // Note: fr_data_p and tr_data_p cannot be both NULL!

} eip207_flow_record_input_data_t;

// Full record descriptor
typedef struct
{
    eip207_flow_dscr_t                  rec_dscr;

    eip207_flow_hte_dscr_rec_data_t      rec_data;

    void *                              reserved_p;
} eip207_flow_rec_dscr_t;


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip207_lib_flow_entry_lookup
 */
static void
eip207_lib_flow_entry_lookup(
        const u32 hash_id_word0,
        const u32 table_size,
        unsigned int * const ht_entry_byte_offset,
        unsigned int * const dt_entry_index)
{
    u32 hash_id_w0, mask;

    // From the EIP-197 Programmer Manual:
    // The hash engine completes the hash ID calculation and
    // adds bits [(table_size+10):6] (with table_size coming from
    // the FLUE_SIZE(_VM)_f register) of the hash ID, multiplied by 64,
    // to the table base address given in the FLUE_HASHBASE(_VM)_f_* registers.

    // Calculate the entry byte offset in the HT for this record
    hash_id_w0 = (hash_id_word0 & (~MASK_6_BITS));

    mask = (1 << (table_size + 10 + 1)) - 1;

    // Calculate the HT entry byte offset
    *ht_entry_byte_offset = (unsigned int)(hash_id_w0 & mask);

    // Translate the HT entry byte offset to the index in the HT
    // HT entry size is one hash bucket (16 words = 64 bytes)
    *dt_entry_index = (*ht_entry_byte_offset) >> 6; // divide by 64
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_table_size_to_entry_count
 *
 * Convert eip207_flow_hash_table_entry_count_t to a number
 */
static inline unsigned int
eip207_lib_flow_table_size_to_entry_count(
        const unsigned int table_size)
{
    return (1 << (table_size + 5));
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_hte_dscr_byte_count_get
 */
static inline unsigned int
eip207_lib_flow_hte_dscr_byte_count_get(void)
{
    return sizeof(eip207_flow_hte_dscr_t);
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_hb_record_add
 *
 * Updates Hash ID and record offset in the hash bucket,
 * first Hash ID then record byte offset
 */
static inline void
eip207_lib_flow_hb_record_add(
        const dma_resource_handle_t ht_dma_handle,
        const unsigned int hb_byte_offset,
        const unsigned int slot,
        const u32 rec_byte_offset,
        const u32 rec_type,
        const eip207_flow_id_t * const hash_id_p)
{
    unsigned int i;
    unsigned int hb_word_offset = hb_byte_offset >> 2;

    // Write record Hash ID words
    for (i = 0; i < EIP207_FLOW_HASH_ID_WORD_COUNT; i++)
        dma_resource_write32(ht_dma_handle,
                            hb_word_offset +
                             EIP207_FLOW_HB_HASH_ID_1_WORD_OFFSET + i +
                             EIP207_FLOW_HASH_ID_WORD_COUNT * (slot - 1),
                            hash_id_p->word32[i]);

    // Write record offset
    dma_resource_write32(ht_dma_handle,
                        hb_word_offset +
                        EIP207_FLOW_HB_REC_1_WORD_OFFSET + (slot - 1),
                        rec_byte_offset | rec_type);

    // Perform pre-DMA for this hash bucket
    dma_resource_pre_dma(ht_dma_handle,
                       hb_byte_offset,
                       EIP207_FLOW_HT_ENTRY_WORD_COUNT *
                         sizeof(u32));
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_hb_record_remove
 *
 * Updates Hash ID and record offset in the hash bucket,
 * first record byte offset then Hash ID
 */
static inline void
eip207_lib_flow_hb_record_remove(
        const dma_resource_handle_t ht_dma_handle,
        const unsigned int hb_byte_offset,
        const unsigned int slot,
        eip207_flow_id_t * hash_id_p)
{
    unsigned int i;
    unsigned int hb_word_offset = hb_byte_offset >> 2;
    eip207_flow_id_t hash_id;

    // Write record offset
    dma_resource_write32(ht_dma_handle,
                        hb_word_offset +
                        EIP207_FLOW_HB_REC_1_WORD_OFFSET + (slot - 1),
                        EIP207_FLOW_RECORD_DUMMY_ADDRESS);

    // Read old record Hash ID words
    for (i = 0; i < EIP207_FLOW_HASH_ID_WORD_COUNT; i++)
        hash_id.word32[i] = dma_resource_read32(
                             ht_dma_handle,
                             hb_word_offset +
                             EIP207_FLOW_HB_HASH_ID_1_WORD_OFFSET + i +
                             EIP207_FLOW_HASH_ID_WORD_COUNT * (slot - 1));

    // Write new record Hash ID words
    for (i = 0; i < EIP207_FLOW_HASH_ID_WORD_COUNT; i++)
    {
        dma_resource_write32(ht_dma_handle,
                            hb_word_offset +
                             EIP207_FLOW_HB_HASH_ID_1_WORD_OFFSET + i +
                             EIP207_FLOW_HASH_ID_WORD_COUNT * (slot - 1),
                            hash_id_p->word32[i]);

        // Store old record Hash ID words
        hash_id_p->word32[i] = hash_id.word32[i];
    }

    // Perform pre-DMA for this hash bucket
    dma_resource_pre_dma(ht_dma_handle,
                       hb_byte_offset,
                       EIP207_FLOW_HT_ENTRY_WORD_COUNT *
                         sizeof(u32));
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_hb_buck_offs_update
 *
 * Update bucket offset in the hash bucket (identified by hb_byte_offset)
 * by writing the update_value in there
 */
static inline void
eip207_lib_flow_hb_buck_offs_update(
        const dma_resource_handle_t ht_dma_handle,
        const unsigned int hb_byte_offset,
        const u32 update_value)
{
    unsigned int hb_word_offset = hb_byte_offset >> 2;

    // Write record offset
    dma_resource_write32(ht_dma_handle,
                        hb_word_offset +
                        EIP207_FLOW_HB_OVFL_BUCKET_WORD_OFFSET,
                        update_value);

    // Perform pre-DMA for this hash bucket
    dma_resource_pre_dma(ht_dma_handle,
                       hb_byte_offset,
                       EIP207_FLOW_HT_ENTRY_WORD_COUNT *
                         sizeof(u32));
}


#ifdef EIP207_FLOW_CONSISTENCY_CHECK
/*----------------------------------------------------------------------------
 * eip207_lib_flow_hb_hash_id_match
 *
 * Checks if the hash bucket already contains this Hash ID
 */
static bool
eip207_lib_flow_hb_hash_id_match(
        const dma_resource_handle_t ht_dma_handle,
        const unsigned int hb_byte_offset,
        const u32 rec_type,
        const eip207_flow_id_t * const hash_id_p)
{
    u32 word32;
    bool f_match;
    unsigned int i, j;
    unsigned int hb_word_offset = hb_byte_offset >> 2;

    // Read Hash ID's from the bucket
    // (this DMA resource is read-only for the flue device)
    for (i = 0; i < EIP207_FLOW_HTE_BKT_NOF_REC_MAX; i++)
    {
        for (j = 0; j < EIP207_FLOW_HASH_ID_WORD_COUNT; j++)
        {
            f_match = true;

            word32 = dma_resource_read32(
                                 ht_dma_handle,
                                 hb_word_offset +
                                  EIP207_FLOW_HB_HASH_ID_1_WORD_OFFSET +
                                    i * EIP207_FLOW_HASH_ID_WORD_COUNT + j);

            if (word32 != hash_id_p->word32[j])
            {
                f_match = false;
                break; // No match, skip this hash ID
            }
        } // for

        if (f_match)
            break; // Matching Hash ID found, stop searching
    } // for

    if (f_match)
    {
        u32 match_rec_type;

        // Check the record type in the record offset for Hash ID with index i
        // to confirm the match
        word32 = dma_resource_read32(
                             ht_dma_handle,
                             hb_word_offset +
                              EIP207_FLOW_HB_REC_1_WORD_OFFSET + i);

        // Note: small transform record and large transform record
        //       fall under the same record type for this check!
        match_rec_type = word32 & rec_type;

        if ((match_rec_type == EIP207_FLOW_RECORD_FR_ADDRESS  &&
             (rec_type == EIP207_FLOW_RECORD_TR_ADDRESS         ||
              rec_type == EIP207_FLOW_RECORD_TR_LARGE_ADDRESS))    ||

            (rec_type == EIP207_FLOW_RECORD_FR_ADDRESS        &&
             (match_rec_type == EIP207_FLOW_RECORD_TR_ADDRESS   ||
              match_rec_type == EIP207_FLOW_RECORD_TR_LARGE_ADDRESS)))
        {
            // Match not confirmed, a different record type is used
            f_match = false;
        }
    }

    return f_match;
}
#endif // EIP207_FLOW_CONSISTENCY_CHECK


/*----------------------------------------------------------------------------
 * eip207_lib_flow_hb_slot_get
 *
 * Determine the free offset slot number (1,2 or 3) and claim it
 */
static inline eip207_flow_error_t
eip207_lib_flow_hb_slot_get(
        u32 * const rec_offs_mask_p,
        unsigned int * free_slot_p)
{
    if ((*rec_offs_mask_p & EIP207_FLOW_HTE_REC_OFFSET_1) == 0)
    {
        *free_slot_p = 1;
        *rec_offs_mask_p |= EIP207_FLOW_HTE_REC_OFFSET_1;
    }
    else if((*rec_offs_mask_p & EIP207_FLOW_HTE_REC_OFFSET_2) == 0)
    {
        *free_slot_p = 2;
        *rec_offs_mask_p |= EIP207_FLOW_HTE_REC_OFFSET_2;
    }
    else if((*rec_offs_mask_p & EIP207_FLOW_HTE_REC_OFFSET_3) == 0)
    {
        *free_slot_p = 3;
        *rec_offs_mask_p |= EIP207_FLOW_HTE_REC_OFFSET_3;
    }
    else
        // Consistency checks for the found HTE descriptor
        return EIP207_FLOW_INTERNAL_ERROR;

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_hb_slot_put
 *
 * Determine the used offset slot number (1,2 or 3) and release it
 */
static inline eip207_flow_error_t
eip207_lib_flow_hb_slot_put(
        u32 * const rec_offs_mask_p,
        const unsigned int slot)
{
    switch (slot)
    {
        case 1:
            *rec_offs_mask_p &= ~EIP207_FLOW_HTE_REC_OFFSET_1;
            break;

        case 2:
            *rec_offs_mask_p &= ~EIP207_FLOW_HTE_REC_OFFSET_2;
            break;

        case 3:
            *rec_offs_mask_p &= ~EIP207_FLOW_HTE_REC_OFFSET_3;
            break;

        default:
            // Consistency checks for the slot to release
            return EIP207_FLOW_INTERNAL_ERROR;
    }

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_dscr_internal_data_set
 */
static inline void
eip207_lib_flow_dscr_internal_data_set(
        eip207_flow_dscr_t * const dscr_p)
{
    eip207_flow_rec_dscr_t * p = (eip207_flow_rec_dscr_t*)dscr_p;

    dscr_p->internal_data_p = &p->rec_data;
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_fr_write
 */
static void
eip207_lib_flow_fr_write(
        const dma_resource_handle_t dma_handle,
        const eip207_flow_fr_data_t * const fr_data_p,
        const u32 xform_byte_offset,
        const u32 xform_addr,
        const u32 xform_upper_addr)
{
    unsigned int i;
    u32 record_type;

    // Write flow record with 0's
    for (i = 0; i < FIRMWARE_EIP207_CS_FRC_RECORD_WORD_COUNT; i++)
        dma_resource_write32(dma_handle, i, 0);

#ifdef EIP207_FLOW_NO_TYPE_BITS_IN_FLOW_RECORD
    record_type = 0;
#else
    record_type = fr_data_p->f_large ?
        EIP207_FLOW_RECORD_TR_LARGE_ADDRESS :
        EIP207_FLOW_RECORD_TR_ADDRESS;
#endif

    // Write Transform Record 32-bit offset
    dma_resource_write32(dma_handle,
                        FIRMWARE_EIP207_CS_FLOW_FR_XFORM_OFFS_WORD_OFFSET,
                        xform_byte_offset + record_type);

    // Write Transform Record low half of 64-bit address
    dma_resource_write32(dma_handle,
                        FIRMWARE_EIP207_CS_FLOW_FR_XFORM_ADDR_WORD_OFFSET,
                        xform_addr + record_type);

    // Write Transform Record high half of 64-bit address
    dma_resource_write32(dma_handle,
                        FIRMWARE_EIP207_CS_FLOW_FR_XFORM_ADDR_HI_WORD_OFFSET,
                        xform_upper_addr);

    // Not supported yet
    // Write arc4 state Record physical address
    //dma_resource_write32(fr_dscr_p->data.dma_handle,
    //                    FIRMWARE_EIP207_CS_FLOW_FR_ARC4_ADDR_WORD_OFFSET,
    //                    EIP207_FLOW_RECORD_DUMMY_ADDRESS);

    // Write Software Flow Record Reference
    // NOTE: this is a 32-bit value!
    dma_resource_write32(dma_handle,
                        FIRMWARE_EIP207_CS_FLOW_FR_SW_ADDR_WORD_OFFSET,
                        fr_data_p->sw_fr_reference);

    // Write the flags field
    dma_resource_write32(dma_handle,
                        FIRMWARE_EIP207_CS_FLOW_FR_FLAGS_WORD_OFFSET,
                        fr_data_p->flags);

    // Perform pre-DMA for the entire flow record
    dma_resource_pre_dma(dma_handle, 0, 0);
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_dtl_record_add
 */
static eip207_flow_error_t
eip207_lib_flow_dtl_record_add(
        volatile eip207_flow_ht_params_t * const ht_params_p,
        eip207_flow_dscr_t * const rec_dscr_p,
        const eip207_flow_record_input_data_t * const rec_data_p)
{
    u32 rec_byte_offset;
    unsigned int slot = 0;
    eip207_flow_hte_dscr_t * hte_dscr_updated_p = NULL;

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
    // Consistency check for the provided record descriptor
    if (rec_dscr_p->dma_addr.addr == EIP207_FLOW_RECORD_DUMMY_ADDRESS)
        return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_CONSISTENCY_CHECK

    // Add the record to the Hash Table
    {
        eip207_flow_hte_dscr_t * hte_dscr_p;
        unsigned int entry_index, hte_byte_offset, ht_table_size;
        u32 record_type = EIP207_FLOW_RECORD_DUMMY_ADDRESS;
        dma_resource_handle_t ht_dma_handle = ht_params_p->ht_dma_handle;

        // Transform record
        if (rec_data_p->tr_data_p != NULL)
            record_type = rec_data_p->tr_data_p->f_large ?
                            EIP207_FLOW_RECORD_TR_LARGE_ADDRESS :
                                       EIP207_FLOW_RECORD_TR_ADDRESS;
        // Flow record
        else if (rec_data_p->fr_data_p != NULL)
            record_type = EIP207_FLOW_RECORD_FR_ADDRESS;
        else
            return EIP207_FLOW_INTERNAL_ERROR; // Unknown record, error

        ht_table_size = ht_params_p->ht_table_size;

        hte_byte_offset  = 0;
        entry_index     = 0;

        eip207_lib_flow_entry_lookup(
                rec_data_p->hash_id_p->word32[0], // Word 0 is used for lookup
                ht_table_size,
                &hte_byte_offset,
                &entry_index);

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
        {
            unsigned int ht_entry_count =
                    eip207_lib_flow_table_size_to_entry_count(ht_table_size);

            if (entry_index >= ht_entry_count)
                return EIP207_FLOW_INTERNAL_ERROR;
        }
#endif

        // Calculate the record byte offset,
        // ht_params_p->base_addr.addr is the base
        // address of the DMA bank where the record
        // must have been allocated
        rec_byte_offset = rec_dscr_p->dma_addr.addr - ht_params_p->base_addr.addr;

        // Convert the DT entry index to the HTE descriptor pointer
        {
            void * p;
            unsigned char * dt_p = (unsigned char*)ht_params_p->dt_p;

            // Read the entry value at the calculated offset from the DT
            p = dt_p + entry_index * eip207_lib_flow_hte_dscr_byte_count_get();

            hte_dscr_p = (eip207_flow_hte_dscr_t*)p;
        }

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
        // Consistency checks for the found HTE descriptor
        if (hte_dscr_p->bucket_byte_offset != (u32)hte_byte_offset)
            return EIP207_FLOW_INTERNAL_ERROR;

        // Check that this HTE is not an overflow one
        if (hte_dscr_p->f_overflow_bucket)
            return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_CONSISTENCY_CHECK

        do // Walk the bucket chain, look for bucket where record can be added
        {
#ifdef EIP207_FLOW_CONSISTENCY_CHECK
            if (hte_dscr_p->record_count > EIP207_FLOW_MAX_RECORDS_PER_BUCKET)
                return EIP207_FLOW_INTERNAL_ERROR;

            // Check if the hash bucket does not contain this Hash ID already
            if (hte_dscr_updated_p == NULL && eip207_lib_flow_hb_hash_id_match(
                     ht_dma_handle,
                     hte_dscr_p->bucket_byte_offset,
                     record_type,
                     rec_data_p->hash_id_p))
                return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_CONSISTENCY_CHECK

            // Check if the found hash bucket can be used to add
            // the record or a next bucket in the chain must be used
            if (hte_dscr_p->record_count < EIP207_FLOW_MAX_RECORDS_PER_BUCKET)
            {
                u32 temp_mask = hte_dscr_p->used_rec_offs_mask;
                u32 temp_slot = slot;

                // Use the found bucket to add the record

                // Determine the offset slot (1,2 or 3) where the record offset
                // and Hash ID can be stored
                {
                    eip207_flow_error_t flow_rc =
                            eip207_lib_flow_hb_slot_get(
                                    &temp_mask,
                                    &temp_slot);

                    if(flow_rc != EIP207_FLOW_NO_ERROR)
                        return flow_rc;
                }

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
                // Check if the record offset in the HTE slot is a dummy pointer
                {
                    // Read record offset
                    // (this DMA resource is read-only for the flue device)
                    u32 record_byte_offset =
                                dma_resource_read32(
                                         ht_dma_handle,
                                         (hte_dscr_p->bucket_byte_offset >> 2) +
                                           EIP207_FLOW_HB_REC_1_WORD_OFFSET +
                                            (temp_slot - 1));

                    if ((record_byte_offset &
                         EIP207_FLOW_RECORD_ADDRESS_TYPE_BITS) !=
                            EIP207_FLOW_RECORD_DUMMY_ADDRESS)
                        return EIP207_FLOW_INTERNAL_ERROR;
                }
#endif // EIP207_FLOW_CONSISTENCY_CHECK

                // Add the record only if it has not been done already
                if (hte_dscr_updated_p == NULL)
                {
                    hte_dscr_p->used_rec_offs_mask = temp_mask;
                    slot = temp_slot;

                    // If flow record is added then write it
                    if (rec_data_p->fr_data_p != NULL)
                        eip207_lib_flow_fr_write(
                                 rec_dscr_p->dma_handle,
                                 rec_data_p->fr_data_p,
                                 rec_data_p->fr_data_p->xform_dma_addr.addr -
                                 ht_params_p->base_addr.addr,
                                 rec_data_p->fr_data_p->xform_dma_addr.addr,
                                 rec_data_p->fr_data_p->xform_dma_addr.upper_addr);

                    // Update the found hash bucket with the
                    // record Hash ID and offset

                    // CDS point: after this operation is done the flue
                    //            hardware can find the transform record!
                    eip207_lib_flow_hb_record_add(ht_dma_handle,
                                             hte_dscr_p->bucket_byte_offset,
                                             slot,
                                             rec_byte_offset,
                                             record_type,
                                             rec_data_p->hash_id_p);

                    // Increase the HTE descriptor record count
                    // for the added record
                    hte_dscr_p->record_count++;
                }

                // The found HTE is updated, record is added
                if (hte_dscr_updated_p == NULL)
                    hte_dscr_updated_p = hte_dscr_p;

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
                // Get the next HTE descriptor in the chain
                hte_dscr_p = eip207_flow_hte_dscr_list_next_get(hte_dscr_p);
                continue; // Validate the entire chain
#else
                break;
#endif
            }
            else // Found bucket record count is max possible
            {
                eip207_flow_hte_dscr_t * hte_dscr_next_p;

                // Use a next overflow bucket to add the record, find a bucket
                // in the chain with a free slot for the new record

                // Get the next HTE descriptor in the chain
                hte_dscr_next_p =
                           eip207_flow_hte_dscr_list_next_get(hte_dscr_p);

                // Check if we have one and
                // that the record has not been added already
                if (hte_dscr_next_p == NULL && hte_dscr_updated_p == NULL)
                {
                    // No chain is present for hte_dscr_p.
                    // Link a new HTE descriptor from the free list to
                    // the found HTE descriptor

                    eip207_flow_hte_dscr_t * free_list_head_p;

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
                    // Check if the overflow bucket offset is a dummy pointer
                    {
                        // Read the overflow bucket offset
                        // (this DMA resource is read-only for the flue device)
                        u32 bucket_byte_offset =
                                dma_resource_read32(
                                    ht_dma_handle,
                                    (hte_dscr_p->bucket_byte_offset >> 2) +
                                      EIP207_FLOW_HB_OVFL_BUCKET_WORD_OFFSET);

                        if (bucket_byte_offset !=
                                EIP207_FLOW_RECORD_DUMMY_ADDRESS)
                            return EIP207_FLOW_INTERNAL_ERROR;
                    }
#endif // EIP207_FLOW_CONSISTENCY_CHECK

                    free_list_head_p =
                        (eip207_flow_hte_dscr_t*)ht_params_p->free_list_head_p;

                    // Check if the record can be added
                    if (free_list_head_p == NULL)
                        // Out of descriptors for overflow buckets
                        return EIP207_FLOW_OUT_OF_MEMORY_ERROR;

                    slot = 1; // start with slot 1 in the new bucket

                    // Get a free HTE descriptor from the free list head
                    hte_dscr_next_p =
                          eip207_flow_hte_dscr_list_next_get(free_list_head_p);

                    // Check if the last descriptor is present in the free list
                    if (hte_dscr_next_p == NULL)
                    {
                        hte_dscr_next_p = free_list_head_p;
                        ht_params_p->free_list_head_p = NULL; // list is empty
                    }
                    else
                    {
                        // Remove the got HTE descriptor from the free list
                        eip207_flow_hte_dscr_list_remove(hte_dscr_next_p);
                    }

                    // Add the got HTE descriptor to the overflow chain,
                    // we know that hte_dscr_p is the last descriptor
                    // in the chain
                    eip207_flow_hte_dscr_list_next_set(hte_dscr_p,
                                                       hte_dscr_next_p);
                    eip207_flow_hte_dscr_list_prev_set(hte_dscr_next_p,
                                                       hte_dscr_p);

                    // We add first record to the bucket which HTE descriptor
                    // just fresh-taken from the free list
                    hte_dscr_next_p->used_rec_offs_mask =
                                       EIP207_FLOW_HTE_REC_OFFSET_1;

                    // If flow record is added then write it
                    if (rec_data_p->fr_data_p != NULL)
                        eip207_lib_flow_fr_write(
                                 rec_dscr_p->dma_handle,
                                 rec_data_p->fr_data_p,
                                 rec_data_p->fr_data_p->xform_dma_addr.addr -
                                 ht_params_p->base_addr.addr,
                                 rec_data_p->fr_data_p->xform_dma_addr.addr,
                                 rec_data_p->fr_data_p->xform_dma_addr.upper_addr);

                    // Update the overflow bucket with hash ID and record offset
                    eip207_lib_flow_hb_record_add(ht_dma_handle,
                                             hte_dscr_next_p->bucket_byte_offset,
                                             slot,
                                             rec_byte_offset,
                                             record_type,
                                             rec_data_p->hash_id_p);

                    // CDS point: after the next write32() operation is done
                    //            the flue HW can find the overflow bucket!

                    // Update bucket offset to the found bucket
                    // Note: bucket offset can use any address (pointer) type
                    //       but not NULL!
                    eip207_lib_flow_hb_buck_offs_update(
                                           ht_dma_handle,
                                           hte_dscr_p->bucket_byte_offset,
                                           hte_dscr_next_p->bucket_byte_offset |
                                             EIP207_FLOW_RECORD_TR_ADDRESS);

                    // Increment record count for the added record
                    hte_dscr_next_p->record_count++;

                    // Bucket is updated, record is added
                    if (hte_dscr_updated_p == NULL)
                        hte_dscr_updated_p = hte_dscr_next_p;

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
                    hte_dscr_p = hte_dscr_next_p;
                    continue; // Validate the entire chain
#else
                    break;
#endif
                }
                else // Overflow chain is present for the found hte_dscr_p
                {
                    // hte_dscr_p - last found HTE descriptor in the chain
                    // hte_dscr_next_p - next HTE descriptor for hte_dscr_p

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
                    if (hte_dscr_next_p != NULL)
                    {
                        // Consistency checks: next HTE descriptor must be on
                        //                     the overflow chain
                        if (!hte_dscr_next_p->f_overflow_bucket)
                            return EIP207_FLOW_INTERNAL_ERROR;

                        // Check if the overflow bucket offset is
                        // a dummy pointer
                        {
                            // Read the overflow bucket offset
                            // from the previous bucket
                            u32 bucket_byte_offset =
                                dma_resource_read32(
                                       ht_dma_handle,
                                       (hte_dscr_p->bucket_byte_offset >> 2) +
                                       EIP207_FLOW_HB_OVFL_BUCKET_WORD_OFFSET);

                            if (bucket_byte_offset !=
                                (hte_dscr_next_p->bucket_byte_offset|
                                EIP207_FLOW_RECORD_TR_ADDRESS))
                                return EIP207_FLOW_INTERNAL_ERROR;
                        }
                    }
#endif // EIP207_FLOW_CONSISTENCY_CHECK

                    hte_dscr_p = hte_dscr_next_p;
                    continue;

                } // overflow bucket is found

            } // "bucket full" handling is done

        } while(hte_dscr_p != NULL);

    } // Record is added to the Hash Table

    // Fill in record descriptor internal data
    {
        eip207_flow_hte_dscr_rec_data_t * rec_dscr_data_p;

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
        // Check if the free slot number was found and
        // the HTE descriptor to update was found
        if (slot == 0 || hte_dscr_updated_p == NULL)
            return EIP207_FLOW_INTERNAL_ERROR;
#endif

        eip207_lib_flow_dscr_internal_data_set(rec_dscr_p);
        rec_dscr_data_p =
              (eip207_flow_hte_dscr_rec_data_t*)rec_dscr_p->internal_data_p;

        rec_dscr_data_p->slot         = slot;
        rec_dscr_data_p->hte_dscr_p   = hte_dscr_updated_p;

        if (rec_data_p->tr_data_p != NULL)
        {
            if (rec_data_p->tr_data_p->f_large)
                rec_dscr_data_p->type = EIP207_FLOW_REC_TRANSFORM_LARGE;
            else
                rec_dscr_data_p->type = EIP207_FLOW_REC_TRANSFORM_SMALL;
        }
        else if (rec_data_p->fr_data_p != NULL)
            rec_dscr_data_p->type = EIP207_FLOW_REC_FLOW;
        else
            return EIP207_FLOW_INTERNAL_ERROR;
    }

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_lib_flow_dtl_record_remove
 */
static eip207_flow_error_t
eip207_lib_flow_dtl_record_remove(
        const device_handle_t device,
        const unsigned int hash_table_id,
        volatile eip207_flow_ht_params_t * const ht_params_p,
        eip207_flow_dscr_t * const rec_dscr_p)
{
    eip207_flow_id_t hash_id;
    dma_resource_handle_t ht_dma_handle;
    eip207_flow_hte_dscr_t * hte_dscr_p;
    eip207_flow_hte_dscr_rec_data_t * rec_dscr_data_p =
               (eip207_flow_hte_dscr_rec_data_t*)rec_dscr_p->internal_data_p;

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
    // Consistency check for the provided TR descriptor
    if (rec_dscr_p->dma_addr.addr == EIP207_FLOW_RECORD_DUMMY_ADDRESS)
        return EIP207_FLOW_INTERNAL_ERROR;

    if (rec_dscr_data_p == NULL)
        return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_CONSISTENCY_CHECK

    // get the HTE descriptor for the record to remove
    hte_dscr_p = rec_dscr_data_p->hte_dscr_p;

    ht_dma_handle = ht_params_p->ht_dma_handle;

    zeroinit(hash_id);

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
    // Check record count is sane
    if (hte_dscr_p->record_count > EIP207_FLOW_MAX_RECORDS_PER_BUCKET ||
        hte_dscr_p->record_count == 0)
        return EIP207_FLOW_INTERNAL_ERROR;

    // Check the record descriptor slot number
    if (rec_dscr_data_p->slot < 1 || rec_dscr_data_p->slot > 3)
        return EIP207_FLOW_INTERNAL_ERROR; // Incorrect record slot number

    // Check the record offset calculated from the record descriptor
    // matches the one on the bucket
    {
        u32 rec_byte_offset2;
        u32 rec_byte_offset1 =
                dma_resource_read32(ht_dma_handle,
                                   (hte_dscr_p->bucket_byte_offset >> 2) +
                                     EIP207_FLOW_HB_REC_1_WORD_OFFSET +
                                       (rec_dscr_data_p->slot - 1));

        // Clear the bits that specify the record type
        rec_byte_offset1 &= (~EIP207_FLOW_RECORD_ADDRESS_TYPE_BITS);

        // Calculate the record byte offset,
        // ht_params_p->base_addr.addr is the base
        // address of the DMA bank where the record
        // must have been allocated
        rec_byte_offset2 = rec_dscr_p->dma_addr.addr -
                                            ht_params_p->base_addr.addr;

        if (rec_byte_offset1 != rec_byte_offset2)
            return EIP207_FLOW_INTERNAL_ERROR; // Incorrect record offset
    }
#endif // EIP207_FLOW_CONSISTENCY_CHECK

    if (hte_dscr_p->record_count > 1)
    {
        // Remove the non-last record from the bucket

        eip207_flow_error_t flow_rc;

        // CDS point: record is removed when the next function returns!

        // Update the record offset in the bucket.
        // Update the Hash ID (optional)
        eip207_lib_flow_hb_record_remove(ht_dma_handle,
                                        hte_dscr_p->bucket_byte_offset,
                                        rec_dscr_data_p->slot,
                                        &hash_id);

        // mark this record slot as free in the HTE descriptor
        flow_rc = eip207_lib_flow_hb_slot_put(&hte_dscr_p->used_rec_offs_mask,
                                             rec_dscr_data_p->slot);
        if (flow_rc != EIP207_FLOW_NO_ERROR)
            return flow_rc;

        // Decrease record count in the HTE descriptor
        hte_dscr_p->record_count--;
    }
    else // record count = 1, so when removing the record also remove the bucket
    {
        // Remove the last record from the removed bucket.
        // Also remove the bucket from the chain first.

        if (hte_dscr_p->f_overflow_bucket)
        {
            // Remove the last record from the overflow bucket

            eip207_flow_hte_dscr_t * free_list_head_p;

            // Get the neighboring HTE descriptors in the chain
            eip207_flow_hte_dscr_t * hte_dscr_prev_p =
                           eip207_flow_hte_dscr_list_prev_get(hte_dscr_p);
            eip207_flow_hte_dscr_t * hte_dscr_next_p =
                           eip207_flow_hte_dscr_list_next_get(hte_dscr_p);

#ifdef EIP207_FLOW_CONSISTENCY_CHECK
            // Check if the previous HTE descriptor is not NULL,
            // it may not be NULL for the overflow bucket
            if (hte_dscr_prev_p == NULL)
                return EIP207_FLOW_INTERNAL_ERROR;
#endif

            // CDS point: bucket (and record) is removed when
            //            the next function returns and the wait loop is done!

            // Remove hte_dscr_p bucket from the chain,
            // make the hte_dscr_prev_p bucket refer to hte_dscr_next_p bucket
            // Note: bucket offset can use any address (pointer) type
            //       but not NULL!
            eip207_lib_flow_hb_buck_offs_update(
                                     ht_dma_handle,
                                     hte_dscr_prev_p->bucket_byte_offset,
                                     hte_dscr_next_p ? // the last in chain?
                                         (hte_dscr_next_p->bucket_byte_offset |
                                             EIP207_FLOW_RECORD_TR_ADDRESS) :
                                          EIP207_FLOW_RECORD_DUMMY_ADDRESS);

            // Wait loop: this is to wait for the EIP-207 packet classification
            //            engine to complete processing using this bucket
            //            before it can be re-used for another record lookup
            {
                unsigned int loop_count, value = 1;

                for (loop_count = 0;
                     loop_count < EIP207_FLOW_RECORD_REMOVE_WAIT_COUNT;
                     loop_count++)
                {
                    // Do some work in the loop
                    if (loop_count & BIT_0)
                        value <<= 1;
                    else
                        value >>= 1;
                }
            } // HB remove wait loop is done!

            // Remove record from the hte_dscr_p bucket
            // which is not in the chain anymore
            eip207_lib_flow_hb_record_remove(ht_dma_handle,
                                            hte_dscr_p->bucket_byte_offset,
                                            rec_dscr_data_p->slot,
                                            &hash_id);

            // Update the bucket offset in the removed from the chain
            // hte_dscr_p bucket
            eip207_lib_flow_hb_buck_offs_update(
                                     ht_dma_handle,
                                     hte_dscr_p->bucket_byte_offset,
                                     EIP207_FLOW_RECORD_DUMMY_ADDRESS);

            // Remove HTE descriptor from the chain
            eip207_flow_hte_dscr_list_remove(hte_dscr_p);

            // Add the HTE descriptor to the free list
            free_list_head_p =
                    (eip207_flow_hte_dscr_t*)ht_params_p->free_list_head_p;

            // Insert the HTE descriptor at the free list head
            if (free_list_head_p != NULL)
                eip207_flow_hte_dscr_list_insert(free_list_head_p, hte_dscr_p);
        }
        else
        {
            // Remove the last record from the HT bucket
            // Note: the chain may still be present for this hte_dscr_p bucket

            // CDS point: record is removed when the next function returns!

            // Update the record offset in the bucket.
            // Update the Hash ID (optional)
            eip207_lib_flow_hb_record_remove(ht_dma_handle,
                                            hte_dscr_p->bucket_byte_offset,
                                            rec_dscr_data_p->slot,
                                            &hash_id);

            // Wait loop: this is to wait for the EIP-207 packet classification
            //            engine to complete processing using this bucket
            //            before it can be re-used for another record lookup
            {
                unsigned int loop_count, value = 1;

                for (loop_count = 0;
                     loop_count < EIP207_FLOW_RECORD_REMOVE_WAIT_COUNT;
                     loop_count++)
                {
                    // Do some work in the loop
                    if (loop_count & BIT_0)
                        value <<= 1;
                    else
                        value >>= 1;
                }
            } // HB remove wait loop is done!

            // mark this record slot as free in the HTE descriptor
            eip207_lib_flow_hb_slot_put(&hte_dscr_p->used_rec_offs_mask,
                                       rec_dscr_data_p->slot);
        }

        // Update the hte_dscr_p record offset mask and record count
        hte_dscr_p->used_rec_offs_mask = 0;
        hte_dscr_p->record_count = 0;
    }

#ifndef EIP207_FLUE_RC_HP
    // Invalidate lookup result in the FLUEC
    eip207_fluec_invalidate(device,
                            (u8)hash_table_id,
                            hash_id.word32[0],
                            hash_id.word32[1],
                            hash_id.word32[2],
                            hash_id.word32[3]);
#else
    IDENTIFIER_NOT_USED(device);
    IDENTIFIER_NOT_USED(hash_table_id);
#endif

    // Invalidate the internal data of the DTL transform record descriptor
    rec_dscr_data_p->slot         = 0;
    rec_dscr_data_p->hte_dscr_p   = NULL;
    rec_dscr_data_p->type         = EIP207_FLOW_REC_INVALID;

    return EIP207_FLOW_NO_ERROR;
}

#ifdef EIP207_FLOW_STRICT_ARGS
/*----------------------------------------------------------------------------
 * eip207_lib_flow_is32bit_addressable
 */
static bool
eip207_lib_flow_is32bit_addressable(
        const u32 base_addr_lo,
        const u32 base_addr_hi,
        const u32 addr_lo,
        const u32 addr_hi)
{
    if(base_addr_hi == addr_hi)
    {
        if(base_addr_lo > addr_lo)
            return false;
    }
    else if (base_addr_hi < addr_hi && (base_addr_hi == addr_hi + 1))
    {
        if(base_addr_lo <= addr_lo)
            return false;
    }
    else
        return false;

    return true;
}
#endif // EIP207_FLOW_STRICT_ARGS


/*****************************************************************************
 * Generic API functions implemented in DTL-specific way
 */

/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_byte_count_get
 */
unsigned int
eip207_flow_hte_dscr_byte_count_get(void)
{
    return eip207_lib_flow_hte_dscr_byte_count_get();
}


/*----------------------------------------------------------------------------
 * eip207_flow_fr_dscr_byte_count_get
 */
unsigned int
eip207_flow_fr_dscr_byte_count_get(void)
{
    return sizeof(eip207_flow_rec_dscr_t);
}


/*----------------------------------------------------------------------------
 * eip207_flow_tr_dscr_byte_count_get
 */
unsigned int
eip207_flow_tr_dscr_byte_count_get(void)
{
    return sizeof(eip207_flow_rec_dscr_t);
}


/*----------------------------------------------------------------------------
 * eip207_flow_record_dummy_addr_get
 */
unsigned int
eip207_flow_record_dummy_addr_get(void)
{
    return EIP207_FLOW_RECORD_DUMMY_ADDRESS;
}


/*----------------------------------------------------------------------------
 * eip207_flow_hash_table_install
 */
eip207_flow_error_t
eip207_flow_hash_table_install(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_ht_t * ht_p,
        bool f_lookup_cached,
        bool f_reset)
{
    unsigned int i, entry_count;
    device_handle_t device;
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_POINTER(ht_p->ht_dma_address_p);
    EIP207_FLOW_CHECK_POINTER(ht_p->dt_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);

    entry_count = (unsigned int)ht_p->ht_table_size;
    EIP207_FLOW_CHECK_INT_ATMOST(entry_count,
                                 EIP207_FLOW_HASH_TABLE_ENTRIES_MAX);

    // Convert eip207_flow_hash_table_entry_count_t to a number
    entry_count = eip207_lib_flow_table_size_to_entry_count(entry_count);
    EIP207_FLOW_CHECK_INT_ATMOST(entry_count, ht_p->dt_entry_count);

    // Check Flow Hash Table address alignment
    if (ht_p->ht_dma_address_p->addr == EIP207_FLOW_RECORD_DUMMY_ADDRESS)
        return EIP207_FLOW_ARGUMENT_ERROR;

    // Save the flag that indicates whether the FLUEC must be used
    // Note: FLUEC is enabled and configured via the Global Control interface
    true_io_area_p->ht_params[hash_table_id].f_lookup_cached = f_lookup_cached;

    // Save the host address of the Flow Descriptor Table and its size
    true_io_area_p->ht_params[hash_table_id].dt_p          = ht_p->dt_p;
    true_io_area_p->ht_params[hash_table_id].ht_table_size  =
                                            (unsigned int)ht_p->ht_table_size;

    // Save the DMA resource handle of the Flow Hash Table
    true_io_area_p->ht_params[hash_table_id].ht_dma_handle =
                                                    ht_p->ht_dma_handle;

    device = true_io_area_p->device;

    // Initialize the HT and the DT free list with initial values
    if (f_reset)
    {
        eip207_flow_hte_dscr_t * hte_dscr_p =
                                        (eip207_flow_hte_dscr_t*)ht_p->dt_p;

        // Initialize the Descriptor Table with initial values
        memset(ht_p->dt_p,
               0,
               ht_p->dt_entry_count * sizeof(eip207_flow_hte_dscr_t));

        // number of HTE descriptors is equal to the number of hash buckets
        // in the HT plus the number of overflow hash buckets.

        for (i = 0; i < ht_p->dt_entry_count; i++)
        {
            unsigned int j;

            // Initialize with dummy address all the words (j)
            // in HTE descriptor i
            for (j = 0; j < EIP207_FLOW_HT_ENTRY_WORD_COUNT; j++)
                dma_resource_write32(ht_p->ht_dma_handle,
                                    i * EIP207_FLOW_HT_ENTRY_WORD_COUNT + j,
                                    EIP207_FLOW_RECORD_DUMMY_ADDRESS);

            // Add only the HTE descriptors for the overflow hash buckets
            // to the free list.
            if (i >= entry_count)
            {
                // Descriptor Table free list initialization,
                // all the HTE descriptors are added to the free list

                // First overflow HTE descriptor in DT?
                if (i == entry_count)
                {
                    eip207_flow_hte_dscr_list_prev_set(hte_dscr_p, NULL);
                    eip207_flow_hte_dscr_list_next_set(hte_dscr_p,
                                                       hte_dscr_p + 1);

                    // Set the free list head
                    true_io_area_p->ht_params[hash_table_id].free_list_head_p =
                                                                    hte_dscr_p;
                }

                // Last overflow HTE descriptor in DT?
                if (i == ht_p->dt_entry_count - 1)
                {
                    // First and last overflow HTE descriptor in DT?
                    if (i != entry_count)
                        eip207_flow_hte_dscr_list_prev_set(hte_dscr_p,
                                                           hte_dscr_p - 1);
                    eip207_flow_hte_dscr_list_next_set(hte_dscr_p, NULL);
                }

                // Non-first and non-last overflow HTE descriptor in DT?
                if (i != entry_count && i != (ht_p->dt_entry_count - 1))
                {
                    eip207_flow_hte_dscr_list_prev_set(hte_dscr_p,
                                                       hte_dscr_p - 1);
                    eip207_flow_hte_dscr_list_next_set(hte_dscr_p,
                                                       hte_dscr_p + 1);
                }

                // mark overflow hash buckets
                hte_dscr_p->f_overflow_bucket = true;
            }
            else
                // mark non-overflow hash buckets
                hte_dscr_p->f_overflow_bucket = false;

            // Set hash bucket byte offset
            hte_dscr_p->bucket_byte_offset = i *
                                        EIP207_FLOW_HT_ENTRY_WORD_COUNT *
                                            sizeof(u32);

            hte_dscr_p++; // Next HTE descriptor in the DT
        } // for

        // Perform pre-DMA for the entire HT including the overflow buckets,
        // both are expected to be in one linear contiguous DMA-safe buffer
        dma_resource_pre_dma(ht_p->ht_dma_handle, 0, 0);
    }

    // Install the FHT
    EIP207_FLUE_HASHBASE_LO_WR(device,
                               hash_table_id,
                               ht_p->ht_dma_address_p->addr);
    EIP207_FLUE_HASHBASE_HI_WR(device,
                               hash_table_id,
                               ht_p->ht_dma_address_p->upper_addr);
    EIP207_FLUE_SIZE_UPDATE(device,
                            hash_table_id,
                            (u8)ht_p->ht_table_size);

#ifdef EIP207_FLOW_DEBUG_FSM
    if (f_reset)
    {
        eip207_flow_error_t rv;
        u32 state = true_io_area_p->state;

        // Transit to a new state
        rv = eip207_flow_state_set(
                (eip207_flow_state_t* const)&state,
                EIP207_FLOW_STATE_ENABLED);

        true_io_area_p->state = state;

        if (rv != EIP207_FLOW_NO_ERROR)
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_flow_fr_add

 */
eip207_flow_error_t
eip207_flow_fr_add(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_fr_dscr_t * const fr_dscr_p,
        const eip207_flow_fr_input_data_t * const flow_in_data_p)
{
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    eip207_flow_record_input_data_t rec_data;
    eip207_flow_fr_data_t fr_data;
    eip207_flow_error_t flow_rc;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);
    EIP207_FLOW_CHECK_POINTER(fr_dscr_p);
    EIP207_FLOW_CHECK_POINTER(flow_in_data_p);

#ifdef EIP207_FLOW_STRICT_ARGS
    // Provided flow record physical DMA address must be addressable by
    // a 32-bit offset to the Base address installed via
    // the eip207_flow_rc_base_addr_set() function
    if(!eip207_lib_flow_is32bit_addressable(
            true_io_area_p->ht_params[hash_table_id].base_addr.addr,
            true_io_area_p->ht_params[hash_table_id].base_addr.upper_addr,
            fr_dscr_p->dma_addr.addr,
            fr_dscr_p->dma_addr.upper_addr))
        return EIP207_FLOW_ARGUMENT_ERROR;

    // Provided transform record physical DMA address must be addressable by
    // a 32-bit offset to the Base address installed via
    // the eip207_flow_rc_base_addr_set() function
    if(flow_in_data_p->xform_dma_addr.addr != EIP207_FLOW_RECORD_DUMMY_ADDRESS &&
       !eip207_lib_flow_is32bit_addressable(
                true_io_area_p->ht_params[hash_table_id].base_addr.addr,
                true_io_area_p->ht_params[hash_table_id].base_addr.upper_addr,
                flow_in_data_p->xform_dma_addr.addr,
                flow_in_data_p->xform_dma_addr.upper_addr))
        return EIP207_FLOW_ARGUMENT_ERROR;
#endif // EIP207_FLOW_STRICT_ARGS

    zeroinit(rec_data);
    zeroinit(fr_data);

    fr_data.flags           = flow_in_data_p->flags;
    fr_data.sw_fr_reference = flow_in_data_p->sw_fr_reference;
    fr_data.xform_dma_addr  = flow_in_data_p->xform_dma_addr;
    fr_data.f_large          = flow_in_data_p->f_large;

    rec_data.hash_id_p       = &flow_in_data_p->hash_id;
    rec_data.fr_data_p      = &fr_data;

    flow_rc = eip207_lib_flow_dtl_record_add(
                            &true_io_area_p->ht_params[hash_table_id],
                            (eip207_flow_dscr_t*)fr_dscr_p,
                            &rec_data);
    if (flow_rc != EIP207_FLOW_NO_ERROR)
        return flow_rc;

#ifdef EIP207_FLOW_DEBUG_FSM
    {
        eip207_flow_error_t rv;
        u32 state = true_io_area_p->state;

        true_io_area_p->rec_installed_counter++;

        // Transit to a new state
        rv = eip207_flow_state_set((eip207_flow_state_t* const)&state,
                                   EIP207_FLOW_STATE_INSTALLED);

        true_io_area_p->state = state;

        if (rv != EIP207_FLOW_NO_ERROR)
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


/*----------------------------------------------------------------------------
 * eip207_flow_rc_base_addr_set
 */
eip207_flow_error_t
eip207_flow_rc_base_addr_set(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_address_t * const flow_base_addr_p,
        const eip207_flow_address_t * const transform_base_addr_p)
{
    device_handle_t device;
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);

    IDENTIFIER_NOT_USED(transform_base_addr_p);

    device = true_io_area_p->device;

    // Set the common base address for all the records which can be looked up
    // Note: Standard (legacy) record cache should have
    //       EIP207_RC_SET_NR_DEFAULT set (which is 0, just one Hash Table);
    //       High-performance (HP) record cache should have
    //       hash_table_id set
#ifndef EIP207_FLUE_RC_HP
    EIP207_RC_BaseAddr_Set(device,
                           EIP207_FRC_REG_BASE,
                           hash_table_id, // EIP207_RC_SET_NR_DEFAULT
                           flow_base_addr_p->addr,
                           flow_base_addr_p->upper_addr);
#else
    EIP207_FLUE_CACHEBASE_LO_WR(device, hash_table_id, flow_base_addr_p->addr);
    EIP207_FLUE_CACHEBASE_HI_WR(device, hash_table_id, flow_base_addr_p->upper_addr);
#endif

    true_io_area_p->ht_params[hash_table_id].base_addr.addr = flow_base_addr_p->addr;
    true_io_area_p->ht_params[hash_table_id].base_addr.upper_addr =
                                                    flow_base_addr_p->upper_addr;

    return  EIP207_FLOW_NO_ERROR;
}


/*****************************************************************************
 * DTL-specific API functions
 */

/*----------------------------------------------------------------------------
 * eip207_flow_dtl_fr_remove
 */
eip207_flow_error_t
eip207_flow_dtl_fr_remove(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_fr_dscr_t * const fr_dscr_p)
{
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    eip207_flow_hte_dscr_rec_data_t * fr_data_p;
    eip207_flow_error_t flow_rc;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);
    EIP207_FLOW_CHECK_POINTER(fr_dscr_p);

    fr_data_p = (eip207_flow_hte_dscr_rec_data_t*)fr_dscr_p->internal_data_p;

    EIP207_FLOW_CHECK_POINTER(fr_data_p);
    EIP207_FLOW_CHECK_POINTER(fr_data_p->hte_dscr_p);

#ifdef EIP207_FLOW_STRICT_ARGS
    // Provided flow record physical DMA address must be addressable by
    // a 32-bit offset to the Based address installed via
    // the eip207_flow_rc_base_addr_set() function
    if(!eip207_lib_flow_is32bit_addressable(
                true_io_area_p->ht_params[hash_table_id].base_addr.addr,
                true_io_area_p->ht_params[hash_table_id].base_addr.upper_addr,
                fr_dscr_p->dma_addr.addr,
                fr_dscr_p->dma_addr.upper_addr))
        return EIP207_FLOW_ARGUMENT_ERROR;

    if (fr_data_p->type != EIP207_FLOW_REC_FLOW)
        return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_STRICT_ARGS

    flow_rc = eip207_lib_flow_dtl_record_remove(
                          true_io_area_p->device,
                          hash_table_id,
                          &true_io_area_p->ht_params[hash_table_id],
                          (eip207_flow_dscr_t*)fr_dscr_p);
    if (flow_rc != EIP207_FLOW_NO_ERROR)
        return flow_rc;

    IDENTIFIER_NOT_USED(fr_data_p);

#ifdef EIP207_FLOW_DEBUG_FSM
    {
        eip207_flow_error_t rv;
        u32 state = true_io_area_p->state;

        true_io_area_p->rec_installed_counter--;

        // Transit to a new state
        if (true_io_area_p->rec_installed_counter == 0)
            // Last record is removed
            rv = eip207_flow_state_set(
                    (eip207_flow_state_t* const)&state,
                    EIP207_FLOW_STATE_ENABLED);
        else
            // Non-last record is removed
            rv = eip207_flow_state_set(
                    (eip207_flow_state_t* const)&state,
                    EIP207_FLOW_STATE_INSTALLED);

        true_io_area_p->state = state;

        if (rv != EIP207_FLOW_NO_ERROR)
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


#ifndef EIP207_FLOW_REMOVE_TR_LARGE_SUPPORT
/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_large_word_count_get
 */
unsigned int
eip207_flow_dtl_tr_large_word_count_get(void)
{
    return FIRMWARE_EIP207_CS_FLOW_TRC_RECORD_WORD_COUNT_LARGE;
}
#endif // !EIP207_FLOW_REMOVE_TR_LARGE_SUPPORT


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_add
 */
eip207_flow_error_t
eip207_flow_dtl_tr_add(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_tr_dscr_t * const tr_dscr_p,
        const eip207_flow_tr_input_data_t * const xform_in_data_p)
{
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    eip207_flow_record_input_data_t rec_data;
    eip207_flow_tr_data_t tr_data;
    eip207_flow_error_t flow_rc;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);
    EIP207_FLOW_CHECK_POINTER(tr_dscr_p);
    EIP207_FLOW_CHECK_POINTER(xform_in_data_p);

#ifdef EIP207_FLOW_STRICT_ARGS
    // Provided transform record physical DMA address must be addressable by
    // a 32-bit offset to the Transform Based address installed via
    // the eip207_flow_rc_base_addr_set() function
    if(!eip207_lib_flow_is32bit_addressable(
                    true_io_area_p->ht_params[hash_table_id].base_addr.addr,
                    true_io_area_p->ht_params[hash_table_id].base_addr.upper_addr,
                    tr_dscr_p->dma_addr.addr,
                    tr_dscr_p->dma_addr.upper_addr))
        return EIP207_FLOW_ARGUMENT_ERROR;
#endif // EIP207_FLOW_STRICT_ARGS

    zeroinit(rec_data);
    zeroinit(tr_data);

    tr_data.f_large     = xform_in_data_p->f_large;

    rec_data.hash_id_p  = &xform_in_data_p->hash_id;
    rec_data.tr_data_p = &tr_data;

    flow_rc = eip207_lib_flow_dtl_record_add(
                            &true_io_area_p->ht_params[hash_table_id],
                            (eip207_flow_dscr_t*)tr_dscr_p,
                            &rec_data);
    if (flow_rc != EIP207_FLOW_NO_ERROR)
        return flow_rc;

#ifdef EIP207_FLOW_DEBUG_FSM
    {
        eip207_flow_error_t rv;
        u32 state = true_io_area_p->state;

        true_io_area_p->rec_installed_counter++;

        // Transit to a new state
        rv = eip207_flow_state_set((eip207_flow_state_t* const)&state,
                                   EIP207_FLOW_STATE_INSTALLED);

        true_io_area_p->state = state;

        if (rv != EIP207_FLOW_NO_ERROR)
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


#ifndef EIP207_FLOW_REMOVE_TR_LARGE_SUPPORT
/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_large_read
 */
eip207_flow_error_t
eip207_flow_dtl_tr_large_read(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_tr_dscr_t * const tr_dscr_p,
        eip207_flow_tr_output_data_t * const xform_data_p)
{
    eip207_flow_error_t rv;
    u32 value32, seq_nr_word_offset;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_POINTER(tr_dscr_p);
    EIP207_FLOW_CHECK_POINTER(xform_data_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);

    IDENTIFIER_NOT_USED(io_area_p);
    IDENTIFIER_NOT_USED(hash_table_id);

    // Prepare the transform record for reading
    dma_resource_post_dma(tr_dscr_p->dma_handle, 0, 0);

    // Read the transform record data

    // Recent record Packets counter
    xform_data_p->packets_counter =
            dma_resource_read32(
                    tr_dscr_p->dma_handle,
                    FIRMWARE_EIP207_CS_FLOW_TR_STAT_PKT_WORD_OFFSET_LARGE);

    // Read Token Context Instruction word
    value32 =
        dma_resource_read32(tr_dscr_p->dma_handle,
                FIRMWARE_EIP207_CS_FLOW_TR_TK_CTX_INST_WORD_OFFSET_LARGE);

    // Extract the Sequence number word offset from the read value
    firmware_eip207_cs_flow_seq_num_offset_read(value32, &seq_nr_word_offset);

    // Read the sequence number
    xform_data_p->sequence_number =
            dma_resource_read32(tr_dscr_p->dma_handle, seq_nr_word_offset);

    // Recent record time stamp
    rv = eip207_flow_internal_read64(
                   tr_dscr_p->dma_handle,
                   FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_LO_WORD_OFFSET_LARGE,
                   FIRMWARE_EIP207_CS_FLOW_TR_TIME_STAMP_HI_WORD_OFFSET_LARGE,
                   &xform_data_p->last_time_lo,
                   &xform_data_p->last_time_hi);
    if (rv != EIP207_FLOW_NO_ERROR)
        return rv;

    // Recent record Octets counter
    rv = eip207_flow_internal_read64(
                   tr_dscr_p->dma_handle,
                   FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_LO_WORD_OFFSET_LARGE,
                   FIRMWARE_EIP207_CS_FLOW_TR_STAT_OCT_HI_WORD_OFFSET_LARGE,
                   &xform_data_p->octets_counter_lo,
                   &xform_data_p->octets_counter_hi);
    if (rv != EIP207_FLOW_NO_ERROR)
        return rv;

    return EIP207_FLOW_NO_ERROR;
}
#endif // !EIP207_FLOW_REMOVE_TR_LARGE_SUPPORT


/*----------------------------------------------------------------------------
 * eip207_flow_dtl_tr_remove
 */
eip207_flow_error_t
eip207_flow_dtl_tr_remove(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_tr_dscr_t * const tr_dscr_p)
{
    volatile eip207_flow_true_io_area_t * const true_io_area_p = ioarea(io_area_p);
    eip207_flow_hte_dscr_rec_data_t * tr_data_p;
    eip207_flow_error_t flow_rc;

    EIP207_FLOW_CHECK_POINTER(io_area_p);
    EIP207_FLOW_CHECK_INT_ATMOST(hash_table_id + 1,
                                 EIP207_FLOW_MAX_NOF_FLOW_HASH_TABLES_TO_USE);
    EIP207_FLOW_CHECK_POINTER(tr_dscr_p);

    tr_data_p = (eip207_flow_hte_dscr_rec_data_t*)tr_dscr_p->internal_data_p;

    EIP207_FLOW_CHECK_POINTER(tr_data_p);
    EIP207_FLOW_CHECK_POINTER(tr_data_p->hte_dscr_p);

#ifdef EIP207_FLOW_STRICT_ARGS
    // Provided transform record physical DMA address must be addressable by
    // a 32-bit offset to the Transform Based address installed via
    // the eip207_flow_rc_base_addr_set() function
    if(!eip207_lib_flow_is32bit_addressable(
                    true_io_area_p->ht_params[hash_table_id].base_addr.addr,
                    true_io_area_p->ht_params[hash_table_id].base_addr.upper_addr,
                    tr_dscr_p->dma_addr.addr,
                    tr_dscr_p->dma_addr.upper_addr))
        return EIP207_FLOW_ARGUMENT_ERROR;

    if (tr_data_p->type != EIP207_FLOW_REC_TRANSFORM_SMALL &&
        tr_data_p->type != EIP207_FLOW_REC_TRANSFORM_LARGE)
        return EIP207_FLOW_INTERNAL_ERROR;
#endif // EIP207_FLOW_STRICT_ARGS

    flow_rc = eip207_lib_flow_dtl_record_remove(
                          true_io_area_p->device,
                          hash_table_id,
                          &true_io_area_p->ht_params[hash_table_id],
                          (eip207_flow_dscr_t*)tr_dscr_p);
    if (flow_rc != EIP207_FLOW_NO_ERROR)
        return flow_rc;

    IDENTIFIER_NOT_USED(tr_data_p);

#ifdef EIP207_FLOW_DEBUG_FSM
    {
        eip207_flow_error_t rv;
        u32 state = true_io_area_p->state;

        true_io_area_p->rec_installed_counter--;

        // Transit to a new state
        if (true_io_area_p->rec_installed_counter == 0)
            // Last record is removed
            rv = eip207_flow_state_set(
                    (eip207_flow_state_t* const)&state,
                    EIP207_FLOW_STATE_ENABLED);
        else
            // Non-last record is removed
            rv = eip207_flow_state_set(
                    (eip207_flow_state_t* const)&state,
                    EIP207_FLOW_STATE_INSTALLED);

        true_io_area_p->state = state;

        if (rv != EIP207_FLOW_NO_ERROR)
            return EIP207_FLOW_ILLEGAL_IN_STATE;
    }
#endif // EIP207_FLOW_DEBUG_FSM

    return EIP207_FLOW_NO_ERROR;
}


/* end of file eip207_flow_dtl.c */
