/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * MFD driver for Axiado SPI bridge.
 *
 * Copyright (c) 2021-25 Axiado Corporation (or its affiliates). All rights reserved.
 *
 * This header provides definitions and helper functions for child drivers
 * that need to interact with the Axiado MFD IRQ chip.
 */

#ifndef __MFD_AXIADO_MFD_H__
#define __MFD_AXIADO_MFD_H__

/* Map of IRQs to bits in the shared interrupt status register. */

/* I2C interrupts (bits 0-22) */
#define I2C0_IRQ  0
#define I2C1_IRQ  1
#define I2C2_IRQ  2
#define I2C3_IRQ  3
#define I2C4_IRQ  4
#define I2C5_IRQ  5
#define I2C6_IRQ  6
#define I2C7_IRQ  7
#define I2C8_IRQ  8
#define I2C9_IRQ  9
#define I2C10_IRQ 10
#define I2C11_IRQ 11
#define I2C12_IRQ 12
#define I2C13_IRQ 13
#define I2C14_IRQ 14
#define I2C15_IRQ 15
#define I2C16_IRQ 16
#define I2C17_IRQ 17
#define I2C18_IRQ 18
#define I2C19_IRQ 19
#define I2C20_IRQ 20
#define I2C21_IRQ 21
#define I2C22_IRQ 22

/* Peripheral interrupts */
#define SGPIO_IRQ 23
#define LTPI_IRQ  24
#define ESPI_IRQ  25

/* Total number of interrupts */
#define AXIADO_NUM_IRQS 26

#endif /* __MFD_AXIADO_MFD_H__ */
