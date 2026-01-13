/* eip202_rdr_dscr.h
 *
 * EIP-202 Ring Control Driver Library API Result Descriptor internal interface
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

#ifndef EIP202_RDR_DSCR_H_
#define EIP202_RDR_DSCR_H_


/*----------------------------------------------------------------------------
 * This module implements (provides) the following interface(s):
 */



/*----------------------------------------------------------------------------
 * This module uses (requires) the following interface(s):
 */


/*----------------------------------------------------------------------------
 * Definitions and macros
 */


/*----------------------------------------------------------------------------
 * Local variables
 */


/*----------------------------------------------------------------------------
 * eip202_rdr_write_cb
 * A write callback for the Ring Helper
 */
int
eip202_rdr_write_cb(
        void * const callback_param1_p,
        const int callback_param2,
        const unsigned int write_index,
        const unsigned int write_count,
        const unsigned int total_write_limit,
        const void * descriptors_p,
        const int descriptor_count,
        const unsigned int descriptor_skip_count);


/*----------------------------------------------------------------------------
 * eip202_rdr_read_cb
 * A read callback for the Ring Helper
 */
int
eip202_rdr_read_cb(
        void * const callback_param1_p,
        const int callback_param2,
        const unsigned int read_index,
        const unsigned int read_limit,
        void * descriptors_p,
        const unsigned int descriptor_skip_count);


/*----------------------------------------------------------------------------
 * eip202_rdr_status_cb
 * A status callback for the Ring Helper
 */

int
eip202_rdr_status_cb(
        void * const callback_param1_p,
        const int callback_param2,
        int * const device_read_pos_p);


#endif /* EIP202_RDR_DSCR_H_ */


/* end of file eip202_rdr_dscr.h */
