/* cs_hwpal_lkm.h
 *
 * configuration Settings for Driver Framework Implementation for
 * Linux kernel-space.
 */

/*****************************************************************************
* Copyright (c) 2010-2020 by Rambus, Inc. and/or its subsidiaries.
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

#ifndef INCLUDE_GUARD_CS_HWPAL_LKM_H
#define INCLUDE_GUARD_CS_HWPAL_LKM_H

// we accept a few settings from the top-level configuration file
#include "cs_hwpal.h"

//Define this to override the overall HWPAL log level.
//#define HWPAL_LKM_LOG_SEVERITY  LOG_SEVERITY_WARN
#ifdef HWPAL_LKM_LOG_SEVERITY
#undef LOG_SEVERITY_MAX
#define LOG_SEVERITY_MAX HWPAL_LKM_LOG_SEVERITY
#endif

#ifdef ARCH_ARM64
//  dma_alloc_coherent allocated memory cant be used to build_skb
#ifndef HWPAL_DMARESOURCE_FORCE_NON_COHERENT
// Enable cache-coherent DMA buffer allocation
#define HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT
#endif
#endif

#if defined(ARCH_ARM) || defined(ARCH_ARM64)
// Use non-cached DMA buffer mapping
#define HWPAL_DMARESOURCE_UNCACHED_MAPPING

// Use non-cached DMA buffer allocation
//#define HWPAL_DMARESOURCE_ALLOC_CACHE_COHERENT

// Use minimum required cache-control functionality for DMA-safe buffers
//#define HWPAL_DMARESOURCE_MINIMUM_CACHE_CONTROL

//#define HWPAL_DMARESOURCE_DIRECT_DCHACHE_CONTROL
//#define HWPAL_DMARESOURCE_DSB_ENABLE
#endif // defined(ARCH_ARM) || defined(ARCH_ARM64)

#if defined(DRIVER_SWAPENDIAN) && defined(DRIVER_ENABLE_SWAP_SLAVE)
#define HWPAL_DEVICE_ENABLE_SWAP
#endif // DRIVER_SWAPENDIAN


#endif // INCLUDE_GUARD_CS_HWPAL_LKM_H


/* end of file cs_hwpal_lkm.h */
