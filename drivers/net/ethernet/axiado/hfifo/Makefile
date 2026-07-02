# SPDX-License-Identifier: GPL-2.0
#
# Makefile for Axiado Host FIFO Driver
#

obj-$(CONFIG_AXIADO_HCP) += axiado_hfifo.o

axiado_hfifo-y := \
	hfifo_dev.o

ccflags-y += -I$(src)
ccflags-y += -I$(srctree)/drivers/net/ethernet/axiado
ccflags-y += -I$(srctree)/drivers/net/ethernet/axiado/shim
