# SPDX-License-Identifier: GPL-2.0

# define_trace.h needs to know how to find our trace header
CFLAGS_crg-udc-trace.o		:= -I$(src)

obj-$(CONFIG_USB_CRG_UDC)	+= crg-udc.o

crg-udc-y			:= crg-udc-core.o crg-udc-ring.o crg-udc-ep0.o crg-udc-gadget.o
crg-udc-$(CONFIG_TRACING)	+= crg-udc-trace.o
