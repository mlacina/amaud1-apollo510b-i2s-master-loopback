/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Mariusz Łacina
 *
 * This software is provided as a technical example for developers using
 * Ambiq Micro devices. It is not part of the AmbiqSuite SDK and should not
 * be interpreted as an official Ambiq Micro software release unless stated
 * otherwise.
 */

/**
 * @file i2s_loopback.c
 * @brief Full-duplex I2S DMA loopback for Apollo510B and ADAU1777.
 *
 * Apollo510B operates as the I2S master and provides BCLK and WS to the
 * ADAU1777. Audio data is transferred in both directions using ping-pong DMA.
 *
 * Signal path:
 *
 *   ADAU1777 -> I2S RX -> DMA -> Apollo510B
 *   Apollo510B -> DMA -> I2S TX -> ADAU1777
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"

#include "adau1777.h"


//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

#define I2S_MODULE              0U
#define I2S_BUFFER_SAMPLES      256U
#define I2S_BUFFER_SIZE_BYTES   (I2S_BUFFER_SAMPLES * sizeof(uint32_t))

#define I2S_SDIN_PIN            95U
#define I2S_SDOUT_PIN           101U
#define I2S_BCLK_PIN            100U
#define I2S_WS_PIN              102U

//-----------------------------------------------------------------------------
// Module state
//-----------------------------------------------------------------------------

static void *g_pI2S0Handle;

static volatile bool g_bI2STxDmaComplete = false;
static volatile bool g_bI2SRxDmaComplete = false;

/*
 * DMA ping-pong buffers.
 *
 * AM_SHARED_RW places the buffers in shared SRAM, which is accessible by the
 * I2S DMA engine. The 32-byte alignment matches the Cortex-M55 D-cache line
 * size used by the cache-maintenance operations below.
 */
AM_SHARED_RW uint32_t g_ui32I2S0RxPingDataBuffer[I2S_BUFFER_SAMPLES]
    __attribute__((aligned(32)));
AM_SHARED_RW uint32_t g_ui32I2S0RxPongDataBuffer[I2S_BUFFER_SAMPLES]
    __attribute__((aligned(32)));
AM_SHARED_RW uint32_t g_ui32I2S0TxPingDataBuffer[I2S_BUFFER_SAMPLES]
    __attribute__((aligned(32)));
AM_SHARED_RW uint32_t g_ui32I2S0TxPongDataBuffer[I2S_BUFFER_SAMPLES]
    __attribute__((aligned(32)));

//-----------------------------------------------------------------------------
// GPIO configuration
//-----------------------------------------------------------------------------

static const am_hal_gpio_pincfg_t g_sI2SSdoutPinCfg =
{
    .GP.cfg_b.uFuncSel       = AM_HAL_PIN_101_I2S0_SDOUT,
    .GP.cfg_b.eGPInput       = AM_HAL_GPIO_PIN_INPUT_NONE,
    .GP.cfg_b.eGPRdZero      = AM_HAL_GPIO_PIN_RDZERO_READPIN,
    .GP.cfg_b.eIntDir        = AM_HAL_GPIO_PIN_INTDIR_NONE,
    .GP.cfg_b.eGPOutCfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
    .GP.cfg_b.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_0P5X,
    .GP.cfg_b.ePullup        = AM_HAL_GPIO_PIN_PULLUP_NONE,
    .GP.cfg_b.uNCE           = 0,
    .GP.cfg_b.eCEpol         = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW,
    .GP.cfg_b.uRsvd_0        = 0,
    .GP.cfg_b.ePowerSw       = AM_HAL_GPIO_PIN_POWERSW_NONE,
    .GP.cfg_b.eForceInputEn  = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.eForceOutputEn = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.uRsvd_1        = 0,
};

static const am_hal_gpio_pincfg_t g_sI2SSdinPinCfg =
{
    .GP.cfg_b.uFuncSel       = AM_HAL_PIN_95_I2S0_SDIN,
    .GP.cfg_b.eGPInput       = AM_HAL_GPIO_PIN_INPUT_NONE,
    .GP.cfg_b.eGPRdZero      = AM_HAL_GPIO_PIN_RDZERO_READPIN,
    .GP.cfg_b.eIntDir        = AM_HAL_GPIO_PIN_INTDIR_NONE,
    .GP.cfg_b.eGPOutCfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
    .GP.cfg_b.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_0P5X,
    .GP.cfg_b.ePullup        = AM_HAL_GPIO_PIN_PULLUP_NONE,
    .GP.cfg_b.uNCE           = 0,
    .GP.cfg_b.eCEpol         = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW,
    .GP.cfg_b.uRsvd_0        = 0,
    .GP.cfg_b.ePowerSw       = AM_HAL_GPIO_PIN_POWERSW_NONE,
    .GP.cfg_b.eForceInputEn  = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.eForceOutputEn = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.uRsvd_1        = 0,
};

static const am_hal_gpio_pincfg_t g_sI2SBclkPinCfg =
{
    .GP.cfg_b.uFuncSel       = AM_HAL_PIN_100_I2S0_CLK,
    .GP.cfg_b.eGPInput       = AM_HAL_GPIO_PIN_INPUT_NONE,
    .GP.cfg_b.eGPRdZero      = AM_HAL_GPIO_PIN_RDZERO_READPIN,
    .GP.cfg_b.eIntDir        = AM_HAL_GPIO_PIN_INTDIR_NONE,
    .GP.cfg_b.eGPOutCfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
    .GP.cfg_b.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_1P0X,
    .GP.cfg_b.ePullup        = AM_HAL_GPIO_PIN_PULLUP_NONE,
    .GP.cfg_b.uNCE           = 0,
    .GP.cfg_b.eCEpol         = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW,
    .GP.cfg_b.uRsvd_0        = 0,
    .GP.cfg_b.ePowerSw       = AM_HAL_GPIO_PIN_POWERSW_NONE,
    .GP.cfg_b.eForceInputEn  = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.eForceOutputEn = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.uRsvd_1        = 0,
};

static const am_hal_gpio_pincfg_t g_sI2SWsPinCfg =
{
    .GP.cfg_b.uFuncSel       = AM_HAL_PIN_102_I2S0_WS,
    .GP.cfg_b.eGPInput       = AM_HAL_GPIO_PIN_INPUT_NONE,
    .GP.cfg_b.eGPRdZero      = AM_HAL_GPIO_PIN_RDZERO_READPIN,
    .GP.cfg_b.eIntDir        = AM_HAL_GPIO_PIN_INTDIR_NONE,
    .GP.cfg_b.eGPOutCfg      = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
    .GP.cfg_b.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_0P5X,
    .GP.cfg_b.ePullup        = AM_HAL_GPIO_PIN_PULLUP_NONE,
    .GP.cfg_b.uNCE           = 0,
    .GP.cfg_b.eCEpol         = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW,
    .GP.cfg_b.uRsvd_0        = 0,
    .GP.cfg_b.ePowerSw       = AM_HAL_GPIO_PIN_POWERSW_NONE,
    .GP.cfg_b.eForceInputEn  = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.eForceOutputEn = AM_HAL_GPIO_PIN_FORCEEN_NONE,
    .GP.cfg_b.uRsvd_1        = 0,
};

//-----------------------------------------------------------------------------
// I2S configuration
//-----------------------------------------------------------------------------

/* Apollo510B operates as I2S master and provides BCLK and WS. */
static am_hal_i2s_io_signal_t g_sI2SIOConfig =
{
    .sFsyncPulseCfg =
    {
        .eFsyncPulseType   = AM_HAL_I2S_FSYNC_PULSE_HALF_FRAME_PERIOD,
        .ui32FsyncPulseWidth = 0,
    },
    .eFyncCpol = AM_HAL_I2S_IO_FSYNC_CPOL_HIGH,
    .eTxCpol   = AM_HAL_I2S_IO_TX_CPOL_FALLING,
    .eRxCpol   = AM_HAL_I2S_IO_RX_CPOL_RISING,
};

static am_hal_i2s_data_format_t g_sI2SDataConfig =
{
    .ePhase                   = AM_HAL_I2S_DATA_PHASE_SINGLE,
    .eChannelLenPhase1        = AM_HAL_I2S_FRAME_WDLEN_32BITS,
    .ui32ChannelNumbersPhase1 = 2,
    .ui32ChannelNumbersPhase2 = 0,
    .eDataDelay               = 1,
    .eSampleLenPhase1         = AM_HAL_I2S_SAMPLE_LENGTH_24BITS,
    .eSampleLenPhase2         = AM_HAL_I2S_SAMPLE_LENGTH_24BITS,
    .eDataJust                = AM_HAL_I2S_DATA_JUSTIFIED_LEFT,
};

static am_hal_i2s_config_t g_sI2S0Config =
{
    .eMode     = AM_HAL_I2S_IO_MODE_MASTER,
    .eXfer     = AM_HAL_I2S_XFER_RXTX,
    .eASRC     = 0,
    .eData     = &g_sI2SDataConfig,
    .eIO       = &g_sI2SIOConfig,
    .eClock    = eAM_HAL_I2S_CLKSEL_NCO_HFRC_48MHz,
    .f64NcoDiv = 31.25,
    .eDiv3     = AM_HAL_I2S_CLKDIV_1,
};

static am_hal_i2s_transfer_t g_sI2S0Transfer =
{
    .ui32RxTotalCount        = I2S_BUFFER_SAMPLES,
    .ui32RxTargetAddr        = (uint32_t)&g_ui32I2S0RxPingDataBuffer[0],
    .ui32RxTargetAddrReverse = (uint32_t)&g_ui32I2S0RxPongDataBuffer[0],
    .ui32TxTotalCount        = I2S_BUFFER_SAMPLES,
    .ui32TxTargetAddr        = (uint32_t)&g_ui32I2S0TxPingDataBuffer[0],
    .ui32TxTargetAddrReverse = (uint32_t)&g_ui32I2S0TxPongDataBuffer[0],
};

//-----------------------------------------------------------------------------
// Interrupt handler
//-----------------------------------------------------------------------------

void am_dspi2s0_isr(void)
{
    uint32_t ui32Status;

    am_hal_i2s_interrupt_status_get(g_pI2S0Handle, &ui32Status, true);
    am_hal_i2s_interrupt_clear(g_pI2S0Handle, ui32Status);

    /* Service the DMA completion event and switch the ping-pong buffer. */
    am_hal_i2s_interrupt_service(g_pI2S0Handle, ui32Status, &g_sI2S0Config);

    if ((ui32Status & AM_HAL_I2S_INT_TXDMACPL) != 0U)
    {
        g_bI2STxDmaComplete = true;
    }

    if ((ui32Status & AM_HAL_I2S_INT_RXDMACPL) != 0U)
    {
        g_bI2SRxDmaComplete = true;
    }
}

//-----------------------------------------------------------------------------
// Local functions
//-----------------------------------------------------------------------------

static void system_init(void)
{
    am_bsp_low_power_init();

    am_hal_cachectrl_icache_enable();
    am_hal_cachectrl_dcache_enable(true);

    am_bsp_itm_printf_enable();

    am_util_stdio_terminal_clear();
    am_util_stdio_printf(
        "AMAUD1 full-duplex I2S DMA loopback - Apollo510B I2S master\n\n");
}

static void codec_init(void)
{
    uint32_t ui32Status = adau1777_init();

    if (ui32Status != AM_HAL_STATUS_SUCCESS)
    {
        am_util_stdio_printf("ERROR: adau1777_init() failed.\n");
    }
}

static void i2s_init(void)
{
    am_hal_gpio_pinconfig(I2S_SDOUT_PIN, g_sI2SSdoutPinCfg);
    am_hal_gpio_pinconfig(I2S_SDIN_PIN,  g_sI2SSdinPinCfg);
    am_hal_gpio_pinconfig(I2S_BCLK_PIN,  g_sI2SBclkPinCfg);
    am_hal_gpio_pinconfig(I2S_WS_PIN,    g_sI2SWsPinCfg);

    am_hal_i2s_initialize(I2S_MODULE, &g_pI2S0Handle);
    am_hal_i2s_power_control(g_pI2S0Handle, AM_HAL_I2S_POWER_ON, false);

    if (am_hal_i2s_configure(g_pI2S0Handle, &g_sI2S0Config) !=
        AM_HAL_STATUS_SUCCESS)
    {
        am_util_stdio_printf("ERROR: Invalid I2S0 configuration.\n");
    }

    am_hal_i2s_enable(g_pI2S0Handle);
}

static void audio_dma_init(void)
{
    /*
     * Seed both TX buffers with deterministic startup data. Once RX DMA starts
     * completing, each free TX buffer is replaced with the corresponding RX
     * data by audio_loopback_process().
     */
    for (uint32_t i = 0; i < I2S_BUFFER_SAMPLES; ++i)
    {
        g_ui32I2S0TxPingDataBuffer[i] = (i & 0xFFU) | 0xF50000U;
        g_ui32I2S0TxPongDataBuffer[i] = (i & 0xFFU) | 0x5F0000U;
    }

    /* Make the initial TX buffer contents visible to the DMA engine. */
    am_hal_cachectrl_dcache_clean(
        &(am_hal_cachectrl_range_t)
        {
            (uint32_t)g_ui32I2S0TxPingDataBuffer,
            sizeof(g_ui32I2S0TxPingDataBuffer)
        });

    am_hal_cachectrl_dcache_clean(
        &(am_hal_cachectrl_range_t)
        {
            (uint32_t)g_ui32I2S0TxPongDataBuffer,
            sizeof(g_ui32I2S0TxPongDataBuffer)
        });

    am_hal_i2s_dma_configure(
        g_pI2S0Handle,
        &g_sI2S0Config,
        &g_sI2S0Transfer);
}

static void audio_dma_start(void)
{
    NVIC_EnableIRQ(I2S0_IRQn);
    am_hal_interrupt_master_enable();

    am_hal_i2s_dma_transfer_start(g_pI2S0Handle, &g_sI2S0Config);
}

static void audio_loopback_process(void)
{
    uint32_t *pui32TxFreeBuffer;
    uint32_t *pui32RxReadyBuffer;

    if (!g_bI2STxDmaComplete || !g_bI2SRxDmaComplete)
    {
        return;
    }

    g_bI2STxDmaComplete = false;
    g_bI2SRxDmaComplete = false;

    pui32TxFreeBuffer = (uint32_t *)am_hal_i2s_dma_get_buffer(
        g_pI2S0Handle,
        AM_HAL_I2S_XFER_TX);

    pui32RxReadyBuffer = (uint32_t *)am_hal_i2s_dma_get_buffer(
        g_pI2S0Handle,
        AM_HAL_I2S_XFER_RX);

    /* DMA has written the RX buffer; invalidate before the CPU reads it. */
    am_hal_cachectrl_dcache_invalidate(
        &(am_hal_cachectrl_range_t)
        {
            (uint32_t)pui32RxReadyBuffer,
            I2S_BUFFER_SIZE_BYTES
        },
        false);

    memcpy(pui32TxFreeBuffer, pui32RxReadyBuffer, I2S_BUFFER_SIZE_BYTES);

    /* CPU has written the TX buffer; clean it before DMA reads it. */
    am_hal_cachectrl_dcache_clean(
        &(am_hal_cachectrl_range_t)
        {
            (uint32_t)pui32TxFreeBuffer,
            I2S_BUFFER_SIZE_BYTES
        });
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    system_init();
    codec_init();
    i2s_init();
    audio_dma_init();
    audio_dma_start();

    while (1)
    {
        audio_loopback_process();
    }
}
