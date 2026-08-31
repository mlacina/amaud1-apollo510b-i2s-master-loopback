/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Mariusz Łacina
 *
 * This software is provided as an independent technical example for
 * developers using Ambiq Micro devices. It is not part of the AmbiqSuite SDK
 * and is not an official Ambiq Micro or Analog Devices software release.
 */

#ifndef ADAU1777_H
#define ADAU1777_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ADAU1777 used on the AMAUD1 audio board.
 *
 * The configuration used by this example is:
 * - external 24.576 MHz MCLKIN, PLL bypassed
 * - Apollo510B generates I2S BCLK and LRCLK/WS
 * - ADAU1777 serial port operates in I2S slave mode
 * - 24 kHz stereo serial sample rate
 * - 24-bit audio in 32-BCLK slots
 * - AIN2/AIN3 microphone inputs routed through the output ASRCs to I2S TX
 * - I2S RX routed through the input ASRCs directly to DAC0/DAC1
 * - DSP core disabled; no program or parameter RAM download is required
 * - headphone output enabled
 *
 * @return AM_HAL_STATUS_SUCCESS on success, otherwise an Ambiq HAL status.
 */
uint32_t adau1777_init(void);

/**
 * @brief Write one ADAU1777 control register.
 *
 * @param reg   16-bit ADAU1777 register address.
 * @param value Register value.
 *
 * @return AM_HAL_STATUS_SUCCESS on success, otherwise an Ambiq HAL status.
 */
uint32_t adau1777_write_register(uint16_t reg, uint8_t value);

/**
 * @brief Read one ADAU1777 control register.
 *
 * @param reg   16-bit ADAU1777 register address.
 * @param value Destination for the register value.
 *
 * @return AM_HAL_STATUS_SUCCESS on success, otherwise an Ambiq HAL status.
 */
uint32_t adau1777_read_register(uint16_t reg, uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* ADAU1777_H */
