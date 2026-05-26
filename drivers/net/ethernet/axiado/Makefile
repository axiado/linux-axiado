# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Axiado Network Drivers
#

# HCP main driver needs vendor DDK headers for device_internal.h.

HCP_EIP_HOST_PLATFORM := fpga_vcu118_pci
HCP_EIP_BUS_TYPE := of

HCP_DDK_INCLUDES := \
  -I$(srctree)/drivers/net/ethernet/axiado/eip/ddk/include/eip_core/build \
  -I$(srctree)/drivers/net/ethernet/axiado/eip/ddk/include/eip_core/build/$(HCP_EIP_HOST_PLATFORM) \
  -I$(srctree)/drivers/net/ethernet/axiado/eip/ddk/kit/driverframework/basic_defs_api/incl \
  -I$(srctree)/drivers/net/ethernet/axiado/eip/ddk/kit/driverframework/device_api/incl \
  -I$(srctree)/drivers/net/ethernet/axiado/eip/ddk/integration/driverframework/src \
  -I$(srctree)/drivers/net/ethernet/axiado/eip/ddk/integration/driverframework/src/lkm/$(HCP_EIP_BUS_TYPE)

HCP_DDK_DEFINES :=
ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_B),y)
  HCP_DDK_DEFINES += -DDRIVER_EIP197_HW_CONF_B
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_C),y)
  HCP_DDK_DEFINES += -DDRIVER_EIP197_HW_CONF_C
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_D),y)
  HCP_DDK_DEFINES += -DDRIVER_EIP197_HW_CONF_D
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_E),y)
  HCP_DDK_DEFINES += -DDRIVER_EIP197_HW_CONF_E
else ifeq ($(CONFIG_AXIADO_EIP197_HW_CONF_F),y)
  HCP_DDK_DEFINES += -DDRIVER_EIP197_HW_CONF_F
endif

CFLAGS_hcp_main.o += $(HCP_DDK_INCLUDES) $(HCP_DDK_DEFINES)

# HCP main driver (consolidated platform driver)
obj-$(CONFIG_AXIADO_HCP) += hcp_main.o

# HCP Subsystems (recurse into subdirectories)
obj-$(CONFIG_AXIADO_HCP) += shim/
obj-$(CONFIG_AXIADO_HCP) += eip/
obj-$(CONFIG_AXIADO_HCP) += hfifo/
