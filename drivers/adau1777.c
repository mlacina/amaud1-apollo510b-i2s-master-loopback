/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Mariusz Łacina
 *
 * This software is provided as an independent technical example for
 * developers using Ambiq Micro devices. It is not part of the AmbiqSuite SDK
 * and is not an official Ambiq Micro or Analog Devices software release.
 */

/**
 * @file adau1777.c
 * @brief Minimal register-level ADAU1777 driver for the AMAUD1 loopback demo.
 *
 * This driver configures the codec directly through the public ADAU1777
 * control-register interface.
 *
 * Register addresses and bit-field meanings used here are taken from the
 * publicly available Analog Devices ADAU1777 Data Sheet, Revision 0.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

#include "adau1777.h"

//-----------------------------------------------------------------------------
// Board / bus configuration
//-----------------------------------------------------------------------------

#define ADAU1777_IOM_MODULE                     2U
#define ADAU1777_I2C_ADDRESS                    0x3CU
#define ADAU1777_I2C_SDA_PIN                    26U
#define ADAU1777_I2C_SCL_PIN                    25U

/*
 * This hardware configuration provides a 24.576 MHz clock at ADAU1777 MCLKIN.
 * The codec bypasses its PLL and divides MCLKIN by two to obtain the
 * 12.288 MHz internal clock used by this configuration.
 */
#define ADAU1777_MCLKIN_HZ                       24576000U

//-----------------------------------------------------------------------------
// ADAU1777 control-register addresses used by this example
//-----------------------------------------------------------------------------

#define ADAU1777_REG_CLK_CONTROL                 0x0000U
#define ADAU1777_REG_CORE_CONTROL                0x0009U
#define ADAU1777_REG_CORE_ENABLE                 0x000BU
#define ADAU1777_REG_DAC_SOURCE_0_1              0x0011U
#define ADAU1777_REG_SOUT_SOURCE_0_1             0x0013U
#define ADAU1777_REG_ASRCO_SOURCE_0_1            0x0018U
#define ADAU1777_REG_ASRC_MODE                   0x001AU
#define ADAU1777_REG_ADC_CONTROL1                0x001CU
#define ADAU1777_REG_ADC_CONTROL3                0x001EU
#define ADAU1777_REG_PGA_CONTROL_2               0x0025U
#define ADAU1777_REG_PGA_CONTROL_3               0x0026U
#define ADAU1777_REG_PGA_10DB_BOOST              0x0028U
#define ADAU1777_REG_POP_SUPPRESS                0x0029U
#define ADAU1777_REG_MIC_BIAS                    0x002DU
#define ADAU1777_REG_DAC_CONTROL1                0x002EU
#define ADAU1777_REG_OP_STAGE_MUTES              0x0031U
#define ADAU1777_REG_SAI_0                       0x0032U
#define ADAU1777_REG_SAI_1                       0x0033U
#define ADAU1777_REG_MODE_MP1                    0x0039U
#define ADAU1777_REG_OP_STAGE_CTRL               0x0043U
#define ADAU1777_REG_DECIM_PWR_MODES             0x0044U
#define ADAU1777_REG_INTERP_PWR_MODES            0x0045U
#define ADAU1777_REG_BIAS_CONTROL0               0x0046U
#define ADAU1777_REG_BIAS_CONTROL1               0x0047U
#define ADAU1777_REG_DAC_CONTROL0                0x004FU
#define ADAU1777_REG_VOL_BYPASS                  0x0054U

//-----------------------------------------------------------------------------
// Register values for this application
//-----------------------------------------------------------------------------

/* External MCLK, crystal disabled, I2C spike filter disabled,
 * PLL bypassed, /2 core and master clocks.
 */
#define ADAU1777_CLK_CONTROL_24M576_BYPASS        0x31U

/*
 * Internal codec processing rate: 96 kHz.
 * This is independent of the 24 kHz external I2S sample rate.
 * DSP CORE_RUN remains disabled.
 */
#define ADAU1777_CORE_CONTROL_CODEC_ONLY          0x02U
#define ADAU1777_CORE_ENABLE_CODEC_ONLY           0x00U

/*
 * ADC2 -> output ASRC0, ADC3 -> output ASRC1.
 * Serial input channels 0/1 -> input ASRC0/1 -> DAC0/DAC1.
 */
#define ADAU1777_ASRCO_ADC2_ADC3                  0x76U
#define ADAU1777_SOUT_ASRC0_ASRC1                 0x54U
#define ADAU1777_ASRC_ENABLE_IN_OUT_CH01          0x03U
#define ADAU1777_DAC_FROM_INPUT_ASRC0_ASRC1       0xDCU

/*
 * I2S, stereo, 24 kHz, 32 BCLK cycles/channel.
 * Apollo510B is the I2S clock master and generates BCLK and LRCLK/WS.
 * ADAU1777 operates as the I2S clock slave.
 */
#define ADAU1777_SAI0_I2S_STEREO_24KHZ            0x04U
#define ADAU1777_SAI1_SLAVE_32BCLK                0x00U
#define ADAU1777_MP1_SERIAL_OUTPUT0               0x00U

/* ADC2/ADC3: 96 kHz, fourth-order sinc, unmuted, analog ADC source. */
#define ADAU1777_ADC23_RATE_96KHZ                 0x00U
#define ADAU1777_ADC23_ENABLE                     0x03U

/* AIN2/AIN3 as microphone inputs, +35.25 dB PGA gain, plus +10 dB boost. */
#define ADAU1777_PGA_MIC_GAIN_35P25DB             0xBFU
#define ADAU1777_PGA23_BOOST_10DB                 0x0CU

/* Pop suppression active for the two used PGAs and headphone outputs. */
#define ADAU1777_POP_SUPPRESS_USED_PATHS          0x03U

/* Enable both microphone-bias outputs at 0.9 x AVDD. */
#define ADAU1777_MICBIAS0_1_ENABLE                0x30U

/* DAC0/DAC1 enabled and unmuted. */
#define ADAU1777_DAC01_ENABLE                     0x03U

/* Both differential headphone output pairs muted / unmuted. */
#define ADAU1777_HEADPHONE_MUTE_ALL               0x0FU
#define ADAU1777_HEADPHONE_UNMUTE_ALL             0x00U

/* Headphone mode, both left and right output stages enabled. */
#define ADAU1777_HEADPHONE_ENABLE                 0x30U

/* Filter/ASRC power settings used by the validated loopback configuration. */
#define ADAU1777_DECIM_POWER_BASELINE             0x3FU
#define ADAU1777_INTERP_POWER_USED_PATHS          0x0FU

/* Analog-bias settings used by the validated loopback configuration. */
#define ADAU1777_BIAS0_BASELINE                   0x2AU
#define ADAU1777_BIAS1_ACTIVE_PATH                0x2BU

/* DAC at core sample rate, compensated interpolation. */
#define ADAU1777_DAC_RATE_CORE_FS                 0x00U

/* Unity-gain signal path: bypass converter volume-control blocks. */
#define ADAU1777_VOL_BYPASS_BASELINE              0x3FU

//-----------------------------------------------------------------------------
// Module state
//-----------------------------------------------------------------------------

static void *g_pIomHandle;

static am_hal_iom_config_t g_sIomI2cConfig =
{
    .eInterfaceMode      = AM_HAL_IOM_I2C_MODE,
    .ui32ClockFreq       = AM_HAL_IOM_100KHZ,
    .pNBTxnBuf           = NULL,
    .ui32NBTxnBufLength  = 0U,
};

static const am_hal_gpio_pincfg_t g_sI2cSdaPinCfg =
{
    .GP.cfg_b.uFuncSel   = AM_HAL_PIN_26_M2SDAWIR3,
    .GP.cfg_b.ePullup    = AM_HAL_GPIO_PIN_PULLUP_1_5K,
    .GP.cfg_b.eGPOutCfg  = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN,
    .GP.cfg_b.eGPInput   = AM_HAL_GPIO_PIN_INPUT_NONE,
};

static const am_hal_gpio_pincfg_t g_sI2cSclPinCfg =
{
    .GP.cfg_b.uFuncSel   = AM_HAL_PIN_25_M2SCL,
    .GP.cfg_b.ePullup    = AM_HAL_GPIO_PIN_PULLUP_1_5K,
    .GP.cfg_b.eGPOutCfg  = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN,
    .GP.cfg_b.eGPInput   = AM_HAL_GPIO_PIN_INPUT_NONE,
};

//-----------------------------------------------------------------------------
// Low-level register access
//-----------------------------------------------------------------------------

uint32_t adau1777_write_register(uint16_t reg, uint8_t value)
{
    am_hal_iom_transfer_t sTransaction = {0};
    uint32_t ui32Data = value;

    if (g_pIomHandle == NULL)
    {
        return AM_HAL_STATUS_FAIL;
    }

    sTransaction.uPeerInfo.ui32I2CDevAddr = ADAU1777_I2C_ADDRESS;
    sTransaction.eDirection               = AM_HAL_IOM_TX;
    sTransaction.ui32NumBytes             = 1U;
    sTransaction.pui32TxBuffer            = &ui32Data;
    sTransaction.pui32RxBuffer            = &ui32Data;
    sTransaction.bContinue                = false;
    sTransaction.ui64Instr                = reg;
    sTransaction.ui32InstrLen             = 2U;
    sTransaction.ui32PauseCondition       = 0U;
    sTransaction.ui8Priority              = 0U;
    sTransaction.ui32StatusSetClr         = 0U;

    return am_hal_iom_blocking_transfer(g_pIomHandle, &sTransaction);
}

uint32_t adau1777_read_register(uint16_t reg, uint8_t *value)
{
    am_hal_iom_transfer_t sTransaction = {0};
    uint32_t ui32Data = 0U;
    uint32_t status;

    if ((g_pIomHandle == NULL) || (value == NULL))
    {
        return AM_HAL_STATUS_FAIL;
    }

    sTransaction.uPeerInfo.ui32I2CDevAddr = ADAU1777_I2C_ADDRESS;
    sTransaction.eDirection               = AM_HAL_IOM_RX;
    sTransaction.ui32NumBytes             = 1U;
    sTransaction.pui32TxBuffer            = &ui32Data;
    sTransaction.pui32RxBuffer            = &ui32Data;
    sTransaction.bContinue                = false;
    sTransaction.ui64Instr                = reg;
    sTransaction.ui32InstrLen             = 2U;
    sTransaction.ui32PauseCondition       = 0U;
    sTransaction.ui8Priority              = 0U;
    sTransaction.ui32StatusSetClr         = 0U;

    status = am_hal_iom_blocking_transfer(g_pIomHandle, &sTransaction);
    if (status == AM_HAL_STATUS_SUCCESS)
    {
        *value = (uint8_t)ui32Data;
    }

    return status;
}

static uint32_t adau1777_write_checked(uint16_t reg, uint8_t value)
{
    return adau1777_write_register(reg, value);
}

//-----------------------------------------------------------------------------
// Public initialization
//-----------------------------------------------------------------------------

uint32_t adau1777_init(void)
{
    uint32_t status;

    status = am_hal_iom_initialize(ADAU1777_IOM_MODULE, &g_pIomHandle);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = am_hal_iom_power_ctrl(g_pIomHandle, AM_HAL_SYSCTRL_WAKE, false);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = am_hal_iom_configure(g_pIomHandle, &g_sIomI2cConfig);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    am_hal_gpio_pinconfig(ADAU1777_I2C_SDA_PIN, g_sI2cSdaPinCfg);
    am_hal_gpio_pinconfig(ADAU1777_I2C_SCL_PIN, g_sI2cSclPinCfg);

    status = am_hal_iom_enable(g_pIomHandle);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Allow the codec power-on reset sequence to complete. */
    am_util_delay_ms(15U);

    /* Establish the clock domain before touching rate-dependent registers. */
    status = adau1777_write_checked(ADAU1777_REG_CLK_CONTROL,
                                    ADAU1777_CLK_CONTROL_24M576_BYPASS);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Keep the analog outputs muted throughout signal-path configuration. */
    status = adau1777_write_checked(ADAU1777_REG_OP_STAGE_MUTES,
                                    ADAU1777_HEADPHONE_MUTE_ALL);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Codec-only mode: retain a 96 kHz internal rate but keep the DSP stopped. */
    status = adau1777_write_checked(ADAU1777_REG_CORE_CONTROL,
                                    ADAU1777_CORE_CONTROL_CODEC_ONLY);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_CORE_ENABLE,
                                    ADAU1777_CORE_ENABLE_CODEC_ONLY);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Apollo510B is the clock master; ADAU1777 is the 24 kHz I2S slave. */
    status = adau1777_write_checked(ADAU1777_REG_SAI_0,
                                    ADAU1777_SAI0_I2S_STEREO_24KHZ);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_SAI_1,
                                    ADAU1777_SAI1_SLAVE_32BCLK);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* ADC_SDATA0/PDMOUT/MP1 must explicitly be selected as Serial Output 0. */
    status = adau1777_write_checked(ADAU1777_REG_MODE_MP1,
                                    ADAU1777_MP1_SERIAL_OUTPUT0);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Microphone path: AIN2/AIN3 -> ADC2/3 -> ASRC0/1 -> serial output 0/1. */
    status = adau1777_write_checked(ADAU1777_REG_ASRCO_SOURCE_0_1,
                                    ADAU1777_ASRCO_ADC2_ADC3);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_SOUT_SOURCE_0_1,
                                    ADAU1777_SOUT_ASRC0_ASRC1);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Playback path: serial input 0/1 -> input ASRC0/1 -> DAC0/DAC1. */
    status = adau1777_write_checked(ADAU1777_REG_ASRC_MODE,
                                    ADAU1777_ASRC_ENABLE_IN_OUT_CH01);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_DAC_SOURCE_0_1,
                                    ADAU1777_DAC_FROM_INPUT_ASRC0_ASRC1);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Configure and enable only the two analog microphone channels in use. */
    status = adau1777_write_checked(ADAU1777_REG_ADC_CONTROL1,
                                    ADAU1777_ADC23_RATE_96KHZ);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_PGA_CONTROL_2,
                                    ADAU1777_PGA_MIC_GAIN_35P25DB);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_PGA_CONTROL_3,
                                    ADAU1777_PGA_MIC_GAIN_35P25DB);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_PGA_10DB_BOOST,
                                    ADAU1777_PGA23_BOOST_10DB);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_POP_SUPPRESS,
                                    ADAU1777_POP_SUPPRESS_USED_PATHS);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_MIC_BIAS,
                                    ADAU1777_MICBIAS0_1_ENABLE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Power the digital conversion paths and use unity-gain volume bypasses. */
    status = adau1777_write_checked(ADAU1777_REG_DECIM_PWR_MODES,
                                    ADAU1777_DECIM_POWER_BASELINE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_INTERP_PWR_MODES,
                                    ADAU1777_INTERP_POWER_USED_PATHS);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_BIAS_CONTROL0,
                                    ADAU1777_BIAS0_BASELINE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_BIAS_CONTROL1,
                                    ADAU1777_BIAS1_ACTIVE_PATH);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_VOL_BYPASS,
                                    ADAU1777_VOL_BYPASS_BASELINE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_DAC_CONTROL0,
                                    ADAU1777_DAC_RATE_CORE_FS);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_ADC_CONTROL3,
                                    ADAU1777_ADC23_ENABLE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    status = adau1777_write_checked(ADAU1777_REG_DAC_CONTROL1,
                                    ADAU1777_DAC01_ENABLE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    /* Enable both headphone output stages, then observe the required settling time. */
    status = adau1777_write_checked(ADAU1777_REG_OP_STAGE_CTRL,
                                    ADAU1777_HEADPHONE_ENABLE);
    if (status != AM_HAL_STATUS_SUCCESS)
    {
        return status;
    }

    am_util_delay_ms(6U);

    return adau1777_write_checked(ADAU1777_REG_OP_STAGE_MUTES,
                                  ADAU1777_HEADPHONE_UNMUTE_ALL);
}
