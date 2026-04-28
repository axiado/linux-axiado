# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Axiado EIP Crypto Driver
#

HOST_HW_PLATFORM := fpga_vcu118_pci
FW_EIP207_API_DIR := firmware/fw3.3.1/safexcel-firmware/eip197/firmware/production_firmware/api
EIP197_BUS_TYPE := of
EIP197_BUS_VERSION := AXI4
ifndef CONFIG_ARCH_AX3005
EIP197_NOF_RINGS := 4
EIP197_NOF_RDR_PER_RING := 65
EIP197_NOF_CDR_PER_RING := 65
else
EIP197_NOF_RINGS := 12
EIP197_NOF_RDR_PER_RING := 33
EIP197_NOF_CDR_PER_RING := 33
endif

ccflags-y += \
  -DDRIVER_64BIT_DEVICE \
  -DDRIVER_MAX_NOF_RING_TO_USE=$(EIP197_NOF_RINGS) \
  -DARCH_ARM64 \
  -DDRIVER_64BIT_HOST \
  -DDRIVER_MAX_TX_DESCR=$(EIP197_NOF_CDR_PER_RING) \
  -DDRIVER_MAX_RX_DESCR=$(EIP197_NOF_RDR_PER_RING) \
  -DDRIVER_PEC_MAX_PACKETS=$(EIP197_NOF_RDR_PER_RING) \
  -DDRIVER_MAX_PECLOGICDESCR=$(EIP197_NOF_RDR_PER_RING) \
  -DDRIVER_PERFORMANCE \
  -UCONFIG_EIP_IPSEC_OFFLOAD

ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_B),y)
  ccflags-y += -DDRIVER_EIP197_HW_CONF_B
  ccflags-y += -DEIP_MAX_PE=1
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_C),y)
  ccflags-y += -DDRIVER_EIP197_HW_CONF_C
  ccflags-y += -DEIP_MAX_PE=2
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_D),y)
  ccflags-y += -DDRIVER_EIP197_HW_CONF_D
  ccflags-y += -DEIP_MAX_PE=4
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_E),y)
  ccflags-y += -DDRIVER_EIP197_HW_CONF_E
  ccflags-y += -DEIP_MAX_PE=8
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_F),y)
  ccflags-y += -DDRIVER_EIP197_HW_CONF_F
  ccflags-y += -DEIP_MAX_PE=12
endif

ifeq (${EIP197_BUS_VERSION},AXI3)
  ccflags-y += -DEIP197_BUS_VERSION_AXI3
else ifeq (${EIP197_BUS_VERSION},AXI4)
  ccflags-y += -DEIP197_BUS_VERSION_AXI4
else ifeq (${EIP197_BUS_VERSION},PLB)
  ccflags-y += -DEIP197_BUS_VERSION_PLB
else
  ERROR_EIP197_BUS_VERSION_NOT_SET := YES
endif

# Root for EIP sources
EIP_ROOT := $(srctree)/$(src)

# Updated subdirectory paths
SRC_APP := ddk/integration/slad_adapter
SRC_KIT := ddk/kit
SRC_DRV_FWK := ddk/integration/driverframework
SABUILDER_DIR := ddk/sabuilder
TOKENBUILDER_DIR := ddk/tokenbuilder

# Include paths
PATH_BUILD := $(EIP_ROOT)/ddk/include/eip_core/build
PATH_APP := $(EIP_ROOT)/$(SRC_APP)
PATH_KIT := $(EIP_ROOT)/$(SRC_KIT)
PATH_DRV_FWK := $(EIP_ROOT)/$(SRC_DRV_FWK)
PATH_FW_API := $(EIP_ROOT)/$(FW_EIP207_API_DIR)

# Include Paths
INCLUDE_APP_CONFIG := \
  -I$(PATH_BUILD) \
  -I$(PATH_BUILD)/$(HOST_HW_PLATFORM)

INCLUDE_APP := \
  -I$(PATH_APP)/src \
  -I$(PATH_APP)/incl \
  -I$(PATH_APP)/src/$(HOST_HW_PLATFORM) \
  -I$(PATH_APP)/src/lkm

INCLUDE_EIP_DEV := \
  -I$(EIP_ROOT)/$(SABUILDER_DIR)/include \
  -I$(EIP_ROOT)/$(TOKENBUILDER_DIR)/include \
  -I$(EIP_ROOT) \
  -I$(srctree)/drivers/net/ethernet/axiado \
  -I$(EIP_ROOT)/ddk/include/eip_dev/build \
  -I$(EIP_ROOT)/ddk/include/eip_dev/build/$(HOST_HW_PLATFORM)

INCLUDE_KIT := \
  -I$(PATH_KIT)/driverframework/basic_defs_api/incl \
  -I$(PATH_KIT)/driverframework/clib_abstraction_api/incl \
  -I$(PATH_KIT)/driverframework/device_api/incl \
  -I$(PATH_KIT)/driverframework/dmaresource_api/incl \
  -I$(PATH_DRV_FWK)/src/lkm \
  -I$(PATH_DRV_FWK)/src/lkm/$(EIP197_BUS_TYPE) \
  -I$(PATH_DRV_FWK)/src \
  -I$(PATH_KIT)/eip201/incl \
  -I$(PATH_KIT)/eip201/src \
  -I$(PATH_KIT)/eip74/incl \
  -I$(PATH_KIT)/eip74/src \
  -I$(PATH_FW_API) \
  -I$(PATH_KIT)/eip197/globalcontrol/incl \
  -I$(PATH_KIT)/eip197/globalcontrol/src \
  -I$(PATH_KIT)/eip197/flowcontrol/incl \
  -I$(PATH_KIT)/eip197/flowcontrol/src \
  -I$(PATH_KIT)/eip197/ringcontrol/incl \
  -I$(PATH_KIT)/eip197/ringcontrol/src \
  -I$(PATH_KIT)/iotoken/incl \
  -I$(PATH_KIT)/iotoken/src \
  -I$(PATH_KIT)/log/incl \
  -I$(PATH_KIT)/log/src/printk \
  -I$(PATH_KIT)/ringhelper/incl \
  -I$(PATH_KIT)/list/incl

INCLUDE_SHIM := \
  -I$(srctree)/drivers/net/ethernet/axiado/shim

INCLUDE_FLAGS_ALL := \
  $(INCLUDE_APP_CONFIG) \
  $(INCLUDE_APP) \
  $(INCLUDE_KIT) \
  $(INCLUDE_SHIM) \
  $(INCLUDE_EIP_DEV)

# Compiler Flags
WARNING_FLAGS += -Wall -g

# Prevent runtime error: "driver_197_k: Unknown symbol __stack_chk_guard"
ccflags-y += -fno-stack-protector
ccflags-y += $(BACKWARD_COMPATIBILITY_FLAGS)
ccflags-y += $(INCLUDE_FLAGS_ALL)
ccflags-y += $(WARNING_FLAGS)
ccflags-y += $(DEBUG_FLAGS)
ccflags-y += $(CODE_COVERAGE_FLAGS)
ccflags-y += $(PROFILING_FLAGS)
ccflags-y += $(OPTIMIZATION_FLAGS)

# Ensure subdirectories also see the include paths
subdir-ccflags-y += $(INCLUDE_FLAGS_ALL)

obj-$(CONFIG_AXIADO_HCP) += axiado_eip.o

# Adapter objects (from ddk/integration/slad_adapter)
app_objs := \
  $(SRC_APP)/src/adapter_driver197_init.o \
  $(SRC_APP)/src/$(HOST_HW_PLATFORM)/adapter_init.o \
  $(SRC_APP)/src/lkm/adapter_interrupts.o \
  $(SRC_APP)/src/lkm/adapter_sleep.o \
  $(SRC_APP)/src/lkm/adapter_lock.o \
  $(SRC_APP)/src/lkm/adapter_firmware.o \
  $(SRC_APP)/src/adapter_lock_internal.o \
  $(SRC_APP)/src/adapter_global_drbg_init.o \
  $(SRC_APP)/src/adapter_global_cs_init.o \
  $(SRC_APP)/src/adapter_global_eip207.o \
  $(SRC_APP)/src/adapter_global_init.o \
  $(SRC_APP)/src/adapter_global_eip97.o \
  $(SRC_APP)/src/adapter_global_eip74.o \
  $(SRC_APP)/src/adapter_pcl_generic.o \
  $(SRC_APP)/src/adapter_pcl_dtl.o \
  $(SRC_APP)/src/adapter_pec_dma.o \
  $(SRC_APP)/src/adapter_pec_pktbuf.o \
  $(SRC_APP)/src/adapter_ring_eip202.o \
  $(SRC_APP)/src/adapter_sglist.o \
  $(SRC_APP)/src/adapter_dmabuf.o

# Kit global control objects
kit_global_objs := \
  $(SRC_KIT)/eip197/globalcontrol/src/eip207_global_init.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip207_rc_internal.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip207_ice.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip207_oce.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip207_support.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip207_flue.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip97_global_init.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip202_global_init.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip97_global_event.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip97_global_prng.o \
  $(SRC_KIT)/eip197/globalcontrol/src/eip97_global_fsm.o

# Kit flow control objects
kit_flow_objs := \
  $(SRC_KIT)/eip197/flowcontrol/src/eip207_flow_internal.o \
  $(SRC_KIT)/eip197/flowcontrol/src/eip207_flow_generic.o \
  $(SRC_KIT)/eip197/flowcontrol/src/eip207_flow_dtl.o

# Kit ring control objects
kit_ring_objs := \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_cdr_init.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_cdr_dscr.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_cdr_event.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_cd_format.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_cdr_fsm.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_rdr_init.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_rdr_dscr.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_rdr_event.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_rd_format.o \
  $(SRC_KIT)/eip197/ringcontrol/src/eip202_rdr_fsm.o

# Kit miscellaneous objects
kit_misc_objs := \
  $(SRC_KIT)/iotoken/src/iotoken.o \
  $(SRC_KIT)/eip201/src/eip201.o \
  $(SRC_KIT)/eip74/src/eip74.o \
  $(SRC_KIT)/ringhelper/src/ringhelper.o \
  $(SRC_KIT)/list/src/list.o \
  $(SRC_KIT)/log/src/log.o

# Driver framework objects
drvfwk_objs := \
  $(SRC_DRV_FWK)/src/lkm/$(EIP197_BUS_TYPE)/lkm.o \
  $(SRC_DRV_FWK)/src/lkm/$(EIP197_BUS_TYPE)/device_lkm.o \
  $(SRC_DRV_FWK)/src/lkm/dmares_lkm.o \
  $(SRC_DRV_FWK)/src/dmares_gen.o \
  $(SRC_DRV_FWK)/src/device_generic.o

# SA Builder objects
sabuilder_objs := \
  $(SABUILDER_DIR)/src/sa_builder.o \
  $(SABUILDER_DIR)/src/sa_builder_ipsec.o \
  $(SABUILDER_DIR)/src/sa_builder_extended_ipsec.o \
  $(SABUILDER_DIR)/src/sa_builder_ssltls.o \
  $(SABUILDER_DIR)/src/sa_builder_basic.o

# Token Builder objects
tokenbuilder_objs := \
  $(TOKENBUILDER_DIR)/src/token_builder_core.o \
  $(TOKENBUILDER_DIR)/src/token_builder_context.o

# Driver objects (now at eip/ root level - no src/ subdirectory)
drv_objs := \
  adapter.o \
  da_pec_support.o \
  driver_197_api.o \
  dtl.o \
  eip_dev.o \
  eip_hw_config.o \
  eip_pkt_buff.o \
  eip_ring.o \
  eip_sa.o \
  interrupt.o \
  ioremap_handle.o \
  ipsec.o \
  pec_api.o

# Combine all objects
axiado_eip-objs := \
  $(app_objs) \
  $(kit_global_objs) \
  $(kit_flow_objs) \
  $(kit_ring_objs) \
  $(kit_misc_objs) \
  $(drvfwk_objs) \
  $(sabuilder_objs) \
  $(tokenbuilder_objs) \
  $(drv_objs)
