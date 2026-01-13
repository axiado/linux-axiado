/* eip207_flow_generic.h
 *
 * EIP-207 Flow Control Generic API:
 *      Flow hash table initialization,
 *      Flow and Transform records creation and removal
 *
 * This API is not re-entrant.
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

#ifndef EIP207_FLOW_GENERIC_H_
#define EIP207_FLOW_GENERIC_H_


/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */

// Driver Framework Basic Definitions API
#include "basic_defs.h"         // u8, u32, bool

// Driver Framework device API
#include "device_types.h"       // device_handle_t

// Driver Framework DMA Resource API
#include "dmares_types.h"       // dma_resource_handle_t


/*----------------------------------------------------------------------------
 * Definitions and macros
 */

#define EIP207_FLOW_SELECT_IPV4                BIT_0
#define EIP207_FLOW_SELECT_IPV6                BIT_1

// Hash ID size in 32-bit words (128 bits)
#define EIP207_FLOW_HASH_ID_WORD_COUNT           4

// Flag bitmask for the flags field in the flow record,
// see EIP207_Flow_OutputData_t
#define EIP207_FLOW_FLAG_INBOUND    BIT_1

// Place holder for device specific internal I/O data
typedef struct
{
    // Pointer to the internal data storage to be allocated by the API user,
    // eip207_flow_io_area_byte_count_get() provides the size for the allocation
    void * data_p;

} eip207_flow_io_area_t;

/*----------------------------------------------------------------------------
 * eip207_flow_error_t
 *
 * status (error) code type returned by these API functions
 * See each function "Return value" for details.
 *
 * EIP207_FLOW_NO_ERROR : successful completion of the call.
 * EIP207_FLOW_RECORD_REMOVE_BUSY: flow record removal is ongoing
 * EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR : not supported.
 * EIP207_FLOW_ARGUMENT_ERROR :  invalid argument for a function parameter.
 * EIP207_FLOW_BUSY_RETRY_LATER : device is busy.
 * EIP207_FLOW_OUT_OF_MEMORY_ERROR : no more free entries in Hash Table to add
 *                                   new record (or out of overflow buckets).
 * EIP207_FLOW_ILLEGAL_IN_STATE : illegal state transition
 */
typedef enum
{
    EIP207_FLOW_NO_ERROR = 0,
    EIP207_FLOW_RECORD_REMOVE_BUSY,
    EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR,
    EIP207_FLOW_ARGUMENT_ERROR,
    EIP207_FLOW_INTERNAL_ERROR,
    EIP207_FLOW_OUT_OF_MEMORY_ERROR,
    EIP207_FLOW_ILLEGAL_IN_STATE
} eip207_flow_error_t;

// Table/group size encoding in entries
typedef enum
{
    EIP207_FLOW_HASH_TABLE_ENTRIES_32 = 0,
    EIP207_FLOW_HASH_TABLE_ENTRIES_64,
    EIP207_FLOW_HASH_TABLE_ENTRIES_128,
    EIP207_FLOW_HASH_TABLE_ENTRIES_256,
    EIP207_FLOW_HASH_TABLE_ENTRIES_512,
    EIP207_FLOW_HASH_TABLE_ENTRIES_1024,
    EIP207_FLOW_HASH_TABLE_ENTRIES_2048,
    EIP207_FLOW_HASH_TABLE_ENTRIES_4096,
    EIP207_FLOW_HASH_TABLE_ENTRIES_8192,
    EIP207_FLOW_HASH_TABLE_ENTRIES_16384,
    EIP207_FLOW_HASH_TABLE_ENTRIES_32768,
    EIP207_FLOW_HASH_TABLE_ENTRIES_65536,
    EIP207_FLOW_HASH_TABLE_ENTRIES_131072,
    EIP207_FLOW_HASH_TABLE_ENTRIES_262144,
    EIP207_FLOW_HASH_TABLE_ENTRIES_524288,
    EIP207_FLOW_HASH_TABLE_ENTRIES_1048576,
    EIP207_FLOW_HASH_TABLE_ENTRIES_MAX = EIP207_FLOW_HASH_TABLE_ENTRIES_1048576
} eip207_flow_hash_table_entry_count_t;

// Flow hash ID
typedef struct
{
    // 128-bit flow hash ID value
    u32 word32 [EIP207_FLOW_HASH_ID_WORD_COUNT];

} eip207_flow_id_t;

// Physical (bus) address that can be used for DMA by the device
typedef struct
{
    // 32-bit physical bus address
    u32 addr;

    // upper 32-bit part of a 64-bit physical address
    // Note: this value has to be provided only for 64-bit addresses,
    // in this case addr field provides the lower 32-bit part
    // of the 64-bit address, for 32bit addresses this field is ignored,
    // and should be set to 0.
    u32 upper_addr;

} eip207_flow_address_t;

// This data structure represents the packet parameters (such as IP addresses
// and ports) that select a particular flow.
typedef struct
{
    // Bitwise or of zero or more EIP207_FLOW_SELECT_* flags
    u32  flags;

    // IPv4 (4 bytes) source address
    u8 * src_ip_p;

    // IPv4 (4 bytes) destination address
    u8 * dst_ip_p;

    // IP protocol (UDP, ESP) code as per RFC
    u8   ip_proto;

    // source port for UDP
    u16  src_port;

    // Destination port for UDP
    u16  dst_port;

    // spi in IPsec
    u32  spi;

    // epoch in DTLS
    u16  epoch;

    // Ether type parameter for MACsec.
    u16 ether_type;

    // an for MACsec.
    u8  an;
} eip207_flow_selector_params_t;

// Flow record input data required for the flow record creation
typedef struct
{
    // Bitwise of zero or more EIP207_FLOW_FLAG_* flags, see above
    u32 flags;

    // Flow record hash ID
    eip207_flow_id_t hash_id;

    // Physical (DMA/bus) address of the transform record
    eip207_flow_address_t xform_dma_addr;

    // Software flow record reference
    u32 sw_fr_reference;

    // Transform record type, true - large, false - small
    bool f_large;

} eip207_flow_fr_input_data_t;

// Transform record input data required for the Transform record creation
typedef struct
{
    // Transform record hash ID
    eip207_flow_id_t hash_id;

    // Transform record type, true - large, false - small
    bool f_large;

} eip207_flow_tr_input_data_t;

// data that can be read from flow record (flow output data)
// This data is written in the flow record by the firmware running
// inside the classification engine
typedef struct
{
    // The time stamp of the last record access, in engine clock cycles
    u32 last_time_lo;   // Low 32-bits
    u32 last_time_hi;   // High 32-bits

    // number of packets processed for this flow record, 32-bit integer
    u32 packets_counter;

    // number of octets processed for this flow record, 64-bit integer
    u32 octets_counter_lo;  // Low 32-bits
    u32 octets_counter_hi;  // High 32-bits

} eip207_flow_fr_output_data_t;

// data that can be read from transform record (output data)
// This data is written in the transform record by the firmware running
// inside the classification engine
typedef struct
{
    // The time stamp of the last record access, in engine clock cycles
    u32 last_time_lo;   // Low 32-bits
    u32 last_time_hi;   // High 32-bits

    // 32-bit sequence number of outbound transform
    u32 sequence_number;

    // number of packets processed for this transform record, 32-bit integer
    u32 packets_counter;

    // number of octets processed for this transform record, 64-bit integer
    u32 octets_counter_lo;   // Low 32-bits
    u32 octets_counter_hi;   // High 32-bits

} eip207_flow_tr_output_data_t;

// Hash table (HT) initialization structure
typedef struct
{
    // handle for the DMA resource describing the DMA-safe buffer for the Hash
    // Table. One HT entry is one hash bucket.
    dma_resource_handle_t ht_dma_handle;

    // HT DMA (physical) address
    eip207_flow_address_t * ht_dma_address_p;

    // HT maximum table size N in HW-specific encoding
    eip207_flow_hash_table_entry_count_t ht_table_size;

    // Descriptor Table (DT) host address,
    // this buffer does not have to be DMA-safe
    void * dt_p;

    // DT maximum entry count (size), one entry is one descriptor
    unsigned int dt_entry_count;

} eip207_flow_ht_t;

// General record descriptor
typedef struct
{
    // Record DMA Resource handle
    dma_resource_handle_t dma_handle;

    // Record DMA (bus/physical) address
    eip207_flow_address_t dma_addr;

    // Pointer to internal data structure for internal use,
    // the API user must not write or read this field
    void * internal_data_p;

} eip207_flow_dscr_t;

// Flow record data structure,
// Note: while allocating this place holder for this data structure its size
//       must be obtained via the eip207_flow_fr_dscr_byte_count_get() function
typedef eip207_flow_dscr_t eip207_flow_fr_dscr_t;

// Transform record data structure
// Note: while allocating this place holder for this data structure its size
//       must be obtained via the eip207_flow_tr_dscr_byte_count_get() function
typedef eip207_flow_dscr_t eip207_flow_tr_dscr_t;


/*----------------------------------------------------------------------------
 * eip207_flow_io_area_byte_count_get
 *
 * This function returns the required size of the I/O Area (in bytes).
 */
unsigned int
eip207_flow_io_area_byte_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_ht_entry_word_count_get
 *
 * This function returns the required size of one Hash Table entry
 * (in 32-bit words).
 *
 * This function can be used for the memory allocation for the HT.
 */
unsigned int
eip207_flow_ht_entry_word_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_hte_dscr_byte_count_get
 *
 * This function returns the required size of one HT Entry Descriptor
 * (in bytes).
 *
 * This function can be used for the memory allocation for the DT.
 */
unsigned int
eip207_flow_hte_dscr_byte_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_fr_dscr_byte_count_get
 *
 * This function returns the required size of one Flow Record Descriptor
 * (in bytes).
 *
 * This function can be used for the memory allocation for the Flow Record
 * Descriptor.
 */
unsigned int
eip207_flow_fr_dscr_byte_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_tr_dscr_byte_count_get
 *
 * This function returns the required size of one Transform Record Descriptor
 * (in bytes).
 *
 * This function can be used for the memory allocation for the Transform Record
 * Descriptor.
 */
unsigned int
eip207_flow_tr_dscr_byte_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_record_dummy_addr_get
 *
 * This function returns the dummy record address.
 */
unsigned int
eip207_flow_record_dummy_addr_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_init
 *
 * This function performs the initialization of the EIP-207 Flow Control SW
 * interface and transits the API to the "Flow Control Initialized" state for
 * the requested classification device.
 *
 * This function returns the EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Flow Control Driver Library configuration
 * and the used EIP-207 HW capabilities.
 *
 * Note: This function should be called either after the HW Reset or
 *       after the Global SW Reset.
 *       This function should be called for the Classification device
 *       identified by the device parameter before any other functions of
 *       this API (except for eip207_flow_id_compute()) can be called
 *       for the IO Area returned for this device.
 *
 * io_area_p (output)
 *     Pointer to the place holder in memory for the IO Area
 *     for the Classification device identified by the device parameter.
 *     The memory must be allocated by the caller before invoking
 *     this function.
 *
 * device (input)
 *     handle for the EIP-207 Flow Control device instance returned
 *     by eip_device_find().
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_init(
        eip207_flow_io_area_t * const io_area_p,
        const device_handle_t device);


/*----------------------------------------------------------------------------
 * eip207_flow_hash_table_install
 *
 * This function installs the Hash Table as well as the Descriptor
 * Table and transits the API to the "Flow Control enabled" state for
 * the classification device identified by the IO Area.
 *
 * Note: This function should be called after the eip207_flow_init()
 *       function is called for the required Classification device.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table that must be set up.
 *
 * ht_p (Input)
 *     Pointer for the Hash Table (HT) identified by the hash_table_id.
 *     One HT entry is one hash bucket.
 *
 * f_lookup_cached (input)
 *     Set to true to enable the flue cache
 *     (improves performance)
 *
 * f_reset (input)
 *     Set to true to re-initialize the HT.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_hash_table_install(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_ht_t * ht_p,
        bool f_lookup_cached,
        bool f_reset);


/*----------------------------------------------------------------------------
 * eip207_flow_rc_base_addr_set
 *
 * Set the flue Record Cache base address for the records.
 *
 * Note: This function should be called after the eip207_flow_init()
 *       function is called for the required Classification device.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the Hash Table for which the Record Cache base address
 *     must be set up.
 *
 * flow_base_addr (input)
 *     Base address of the Flow Record Cache. All the flow records must be
 *     allocated within 4GB address region from this flow_base_addr base address.
 *
 * transform_base_addr (input)
 *     Base address of the Transform Record Cache. All the transform records
 *     must be allocated within 4GB address region from this transform_base_addr
 *     base address.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_rc_base_addr_set(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_address_t * const flow_base_addr,
        const eip207_flow_address_t * const transform_base_addr);


/*----------------------------------------------------------------------------
 * eip207_flow_id_compute
 *
 * This function performs the flow hash ID computation.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table.
 *
 * selector_params_p (input)
 *     Pointer to the data structure that contain parameters that should be
 *     used for the flow ID computation
 *
 * flow_id (output)
 *     Pointer to the data structure where the calculated flow hash ID will
 *     be stored
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 */
eip207_flow_error_t
eip207_flow_id_compute(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_selector_params_t * const selector_params_p,
        eip207_flow_id_t * const flow_id);


/*----------------------------------------------------------------------------
 * eip207_flow_fr_word_count_get
 *
 * This function returns the required size of one Flow Record
 * (in 32-bit words).
 */
unsigned int
eip207_flow_fr_word_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_fr_add
 *
 * This function adds the provided flow record to the Flow Hash Table
 * so that the classification device can look it up.
 *
 * This function returns the EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR error code
 * when it detects a mismatch in the Flow Control Driver Library configuration
 * and the used EIP-207 HW capabilities.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be added.
 *
 * fr_dscr_p (input)
 *     Pointer to the data structure that describes the flow record to be added.
 *     This descriptor must exist along with the flow record it describes until
 *     that flow record is removed by the EIP207_Flow_Record_Remove() function.
 *     The flow descriptor pointer cannot be 0.
 *
 * flow_in_data_p (input)
 *     Pointer to the data structure that contains the input data that will
 *     be used for filling in the flow record. The buffer holding this data
 *     structure can be freed after this function returns.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_fr_add(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_fr_dscr_t * const fr_dscr_p,
        const eip207_flow_fr_input_data_t * const flow_in_data_p);


/*----------------------------------------------------------------------------
 * eip207_flow_fr_read
 *
 * This function reads data from the provided flow record.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be removed from.
 *
 * fr_dscr_p (output)
 *     Pointer to the data structure that describes the flow record
 *     to be read. Cannot be 0.
 *
 * flow_data_p (output)
 *     Pointer to the data structure where the read flow data will be stored.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_fr_read(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        eip207_flow_fr_dscr_t * const fr_dscr_p,
        eip207_flow_fr_output_data_t * const flow_data_p);


/*----------------------------------------------------------------------------
 * eip207_flow_tr_word_count_get
 *
 * This function returns the required size of one Transform Record
 * (in 32-bit words).
 */
unsigned int
eip207_flow_tr_word_count_get(void);


/*----------------------------------------------------------------------------
 * eip207_flow_tr_read
 *
 * This function reads output data from the provided transform record.
 *
 * io_area_p (input)
 *     Pointer to the IO Area of the required Classification device.
 *
 * hash_table_id (input)
 *     index of the flow hash table where the flow record must be removed from.
 *
 * tr_dscr_p (input)
 *     Pointer to the transform record descriptor. The transform record
 *     descriptor is only used during this function execution.
 *     It can be discarded after this function returns.
 *
 * xform_data_p (output)
 *     Pointer to the data structure where the read transform data will
 *     be stored.
 *
 * Return value
 *     EIP207_FLOW_NO_ERROR
 *     EIP207_FLOW_UNSUPPORTED_FEATURE_ERROR
 *     EIP207_FLOW_ARGUMENT_ERROR
 *     EIP207_FLOW_ILLEGAL_IN_STATE
 */
eip207_flow_error_t
eip207_flow_tr_read(
        eip207_flow_io_area_t * const io_area_p,
        const unsigned int hash_table_id,
        const eip207_flow_tr_dscr_t * const tr_dscr_p,
        eip207_flow_tr_output_data_t * const xform_data_p);


#endif /* EIP207_FLOW_GENERIC_H_ */


/* end of file eip207_flow_generic.h */
