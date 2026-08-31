# Apollo510B + ADAU1777 Full-Duplex I2S DMA Audio

This repository provides a tested reference implementation of a continuous full-duplex stereo audio path between an **Ambiq Apollo510B** and an **Analog Devices ADAU1777** codec.

The **Apollo510B operates as the I2S master**, generating BCLK and LRCLK/WS, while the ADAU1777 operates as the I2S slave. Audio data is transferred in both directions using DMA with ping-pong buffering and explicit cache-coherency handling.

The project is built with **SEGGER Embedded Studio**, **Arm GNU Toolchain** and **AmbiqSuite SDK 5.2.0**, and is intended as a clean audio-transport baseline for further DSP and embedded AI development.

> **Independent project:** This repository is an independent technical example. It is not an official Ambiq Micro or Analog Devices software release and is not endorsed or maintained by either vendor.

The project implements the following continuous stereo audio path:

```text
Mic Input
    ↓
ADAU1777 (AMAUD1)
    ↓
I2S RX / DMA
    ↓
Apollo510B (Apollo510B EVB)
    ↓
RX ping-pong buffer
    ↓
RX → TX copy
    ↓
TX ping-pong buffer
    ↓
I2S TX / DMA
    ↓
ADAU1777 (AMAUD1)
    ↓
Stereo Output
```

The current implementation is intended primarily as a clean **audio transport baseline** for Apollo510B and as a starting point for future DSP and AI audio applications using Ambiq **neuralSPOT / neuralSPOT-X**. 

------

## Features

- **Ambiq Apollo510B** on Apollo510B EVB
- **Analog Devices ADAU1777** on AMAUD1 EVB
- Apollo510B operating as **I2S master**
- ADAU1777 operating as **I2S slave**
- Full-duplex stereo I2S operation
- DMA-based RX and TX transfers
- Ping-pong audio buffering
- Explicit D-cache coherency management
- Continuous audio streaming
- No CPU-driven per-sample audio transfers
- Project-specific **ADAU1777 register driver** based on the public device register map
- Ready-to-build **SEGGER Embedded Studio** project
- **Arm GNU Toolchain (GCC)**
- Portable **AmbiqSuite SDK** configuration using SES Global Macros
- **J-Link** programming and debugging support

------

## Current Audio Configuration

The tested configuration is:

```text
Sample rate:     24 kHz
Channels:        Stereo
I2S slot width:  32 bits
BCLK:            1.536 MHz
Transfer mode:   Full duplex
RX:              DMA
TX:              DMA
Buffering:       Ping-pong
```

Different DMA buffer sizes have been tested, including:

```text
256 samples
128 samples
```

The audio loopback operates continuously without CPU involvement in individual sample transfers.

------

## Hardware

### Required hardware

- **Ambiq Apollo510B Evaluation Board (AP510BEVB)**  
  [TOP-electronics - Apollo510B SoC Eval Board](https://www.top-electronics.com/en/apollo510b-soc-eval-board)
- **Ambiq AMAUD1 Audio Add-on Board (1st Generation)** with Analog Devices ADAU1777  
  [TOP-electronics - Audio Add-on Board, 1st Generation](https://www.top-electronics.com/en/audio-add-on-board-1st-generation)
- J-Link debugger (the Apollo510B EVB also includes an on-board SEGGER J-Link debugger)
- Audio source
- Headphones, amplifier or other suitable audio output device

------

## Software Requirements

The project has been tested with:

```text
SEGGER Embedded Studio:  8.28
Arm GNU Toolchain:       14.2.Rel1
AmbiqSuite SDK:          5.2.0
```

### Official Downloads

- **AmbiqSuite SDK 5.2.0:** [Ambiq Apollo510B Content Portal](https://contentportal.ambiq.com/en/apollo510b)
- **Arm GNU Toolchain:** [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)

The project has been validated with **Arm GNU Toolchain 14.2.Rel1**. Arm may publish newer toolchain releases; using a different version may require revalidation.

### Target MCU

```text
Apollo510B
Cortex-M55
```

### J-Link Target Device

Use:

```text
AP510BFA-CBR
```

Using only a generic `Cortex-M55` target is sufficient for compilation, but does not provide the correct Apollo510B flash programming support.

------

# Project Portability

The SES project is designed so that it does not depend on absolute paths to:

- AmbiqSuite SDK
- Arm GNU Toolchain
- the location of the project itself

Two SEGGER Embedded Studio **Global Macros** are used:

```text
AMBIQSUITE_ROOT
ARM_GCC_ROOT
```

After cloning the repository, a developer should normally need to configure only these two paths.

------

## SES Global Macros

Open:

```text
Tools
→ Options
→ Building
→ Global Macros
```

Add:

```text
AMBIQSUITE_ROOT=<path-to-AmbiqSuite>
ARM_GCC_ROOT=<path-to-Arm-GNU-Toolchain>
```

Example:

```text
AMBIQSUITE_ROOT=D:/Ambiq/SDK/AmbiqSuite_5.2.0
ARM_GCC_ROOT=C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1
```

### Important

Do not add spaces around `=`.

Use:

```text
AMBIQSUITE_ROOT=D:/Ambiq/SDK/AmbiqSuite_5.2.0
```

not:

```text
AMBIQSUITE_ROOT = D:/Ambiq/SDK/AmbiqSuite_5.2.0
```

The latter may not be expanded correctly by SES.

------

## AMBIQSUITE_ROOT

`AMBIQSUITE_ROOT` must point to the AmbiqSuite root directory containing directories such as:

```text
CMSIS/
boards/
devices/
mcu/
third_party/
utils/
```

Project include paths are expressed using the macro, for example:

```text
$(AMBIQSUITE_ROOT)/mcu/apollo510
$(AMBIQSUITE_ROOT)/mcu/apollo510/regs
$(AMBIQSUITE_ROOT)/mcu/apollo510/hal
$(AMBIQSUITE_ROOT)/mcu/apollo510/hal/mcu

$(AMBIQSUITE_ROOT)/CMSIS/ARM/Include
$(AMBIQSUITE_ROOT)/CMSIS/AmbiqMicro/Include
$(AMBIQSUITE_ROOT)/CMSIS/AmbiqMicro/Source

$(AMBIQSUITE_ROOT)/devices
$(AMBIQSUITE_ROOT)/utils

$(AMBIQSUITE_ROOT)/boards/apollo510b_evb/bsp
```

Additional third-party paths required by the application are handled in the same way.

------

## ARM_GCC_ROOT

The project uses the external Arm GNU Toolchain.

SES is configured to use:

```text
$(ARM_GCC_ROOT)/bin
```

For the tested toolchain this resolves to:

```text
C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/14.2 rel1/bin
```

The compiler executable is therefore:

```text
arm-none-eabi-gcc
```

------

# AmbiqSuite Dependencies

The project uses the GCC versions of the AmbiqSuite HAL and BSP libraries:

```text
$(AMBIQSUITE_ROOT)/mcu/apollo510/hal/mcu/gcc/bin/libam_hal.a
```

and:

```text
$(AMBIQSUITE_ROOT)/boards/apollo510b_evb/bsp/gcc/bin/libam_bsp.a
```

These libraries are referenced using SES Dynamic Folders so they remain visible in the project tree without embedding machine-specific SDK paths in the project.

------

## AmbiqSuite Utility Sources

Some AmbiqSuite utility modules are provided only as source files and are compiled directly as part of the application.

The current project uses:

```text
am_util_delay.c
am_util_debug.c
am_util_stdio.c
am_util_string.c
am_util_faultisr.c
```

from:

```text
$(AMBIQSUITE_ROOT)/utils
```

They are included using an SES **Dynamic Folder** with a file filter so that the complete `utils` directory is not compiled unnecessarily.

------

## CMSIS Source

The Apollo510 system initialization source is taken from:

```text
$(AMBIQSUITE_ROOT)/CMSIS/AmbiqMicro/Source
```

The relevant source is:

```text
system_apollo510.c
```

CMSIS header directories are configured using `$(AMBIQSUITE_ROOT)` include paths.

------

# Project-Local Ambiq Components

A small number of Ambiq-originated files are stored directly in the repository so that the tested startup and memory configuration remain associated with the project:

```text
third_party/Ambiq/startup_gcc.c
third_party/Ambiq/am_resources.c
third_party/Ambiq/linker_script.ld
```

`startup_gcc.c` is based on the Apollo510B GCC startup source from **AmbiqSuite SDK 5.2.0**. The project keeps the 12 KB stack allocation used by this application.

`linker_script.ld` is based on the standard Apollo510B GCC linker script from **AmbiqSuite SDK 5.2.0**. The project-specific SEGGER Embedded Studio changes export `__StackLimit` and `__StackTop` and explicitly retain the `.stack` input sections.

`am_resources.c` is an Ambiq source component used by the Apollo510B runtime configuration.

These Ambiq-originated files retain their original copyright and license terms. They are not covered by the copyright statement for project-authored source code.

The project does not depend on absolute paths to these local files. Project-local source, driver and Ambiq component paths are referenced relatively, for example:

```text
../src
../drivers
../third_party/Ambiq
```

External AmbiqSuite headers, libraries and utility sources continue to be referenced through `$(AMBIQSUITE_ROOT)`.

------

# Compiler Configuration

The project is compiled for:

```text
CPU:        Cortex-M55
ABI:        AAPCS
Float ABI:  Hard
FPU:        FPv5-D16
ISA:        Thumb
```

Relevant generated options include:

```text
-mcpu=cortex-m55
-mthumb
-mfloat-abi=hard
```

The project uses:

```text
-fshort-enums
```

through the SES setting:

```text
Enumeration Size = Minimal Container Size
```

This setting must be retained.

Changing the project to fixed 32-bit enums caused ABI compatibility warnings when linking against the Apollo510 GCC libraries.

------

## Compiler Optimization

The baseline project currently uses:

```text
-O0
```

for straightforward debugging and migration validation.

Optimization can be changed later when performance measurements and AI workloads are introduced.

------

## Section Handling

The project uses function and data section separation:

```text
-ffunction-sections
-fdata-sections
```

and linker garbage collection:

```text
-Wl,--gc-sections
```

to remove unused sections.

------

# Linker Configuration

The project uses the project-local GNU linker script:

```text
../third_party/Ambiq/linker_script.ld
```

The script is based on the Apollo510B GCC linker script from AmbiqSuite SDK 5.2.0, with the small SES-specific stack-symbol changes described above.

The entry point is:

```text
Reset_Handler
```

Relevant linker options include:

```text
-Wl,-eReset_Handler
-Wl,--gc-sections
-nostartfiles
-static
```

The project uses its own Apollo510 startup implementation and therefore does not use the default GCC CRT startup files.

------

# Apollo510B Memory Layout

The application is linked using the following internal memory regions.

```text
ITCM
0x00000000 - 0x0003FFFF
256 KB
Application MRAM
0x00410000 - 0x007FFFFF
4032 KB
DTCM
0x20000000 - 0x2007FFFF
512 KB
System SRAM
0x20080000 - 0x2037FFFF
3 MB
```

The first 64 KB of MRAM are reserved for the Apollo510B boot environment:

```text
0x00400000 - 0x0040FFFF
```

The application therefore starts at:

```text
0x00410000
```

------

## DTCM Allocation

The 512 KB DTCM region is divided as follows:

```text
0x20000000
    |
    | MCU_TCM
    | 496 KB
    |
0x2007C000
    |
    | HEAP
    | 4 KB
    |
0x2007D000
    |
    | STACK
    | 12 KB
    |
0x20080000
```

Total:

```text
496 KB + 4 KB + 12 KB = 512 KB
```

------

# Stack Configuration

The application reserves the complete 12 KB stack region.

In `startup_gcc.c`:

```c
__attribute__ ((section(".stack")))
static uint32_t g_pui32Stack[3072];
```

Since:

```text
3072 × 4 bytes = 12288 bytes = 12 KB
```

the stack occupies:

```text
0x2007D000 - 0x2007FFFF
```

The linker script defines:

```ld
.stack (NOLOAD):
{
    . = ALIGN(8);
    __StackLimit = .;

    KEEP(*(.stack))
    KEEP(*(.stack*))

    . = ALIGN(8);
    __StackTop = .;
} > STACK
```

The resulting symbols are:

```text
__StackLimit = 0x2007D000
__StackTop   = 0x20080000
```

The initial stack pointer stored in the vector table is therefore:

```text
0x20080000
```

------

# Debug Configuration

The project has been tested with J-Link using:

```text
Target Device = AP510BFA-CBR
```

A generic Cortex-M55 target should not be used for flash programming.

------

## Initial Stack Pointer

A special consideration is required when SES starts directly from the application entry point.

Starting at `Reset_Handler` without initializing the Cortex-M55 MSP can result in a HardFault before `main()`.

The project therefore uses:

```text
Starting Stack Pointer Value = __StackTop
```

in:

```text
Project
→ Edit Options
→ Debug
→ Debugger
```

Do not replace this with a fixed numeric address.

Using the linker symbol ensures that the debugger remains correct if the stack allocation changes later.

The expected startup sequence is:

```text
J-Link
    ↓
load application
    ↓
MSP = __StackTop
    ↓
Reset_Handler
    ↓
startup_gcc.c
    ↓
system initialization
    ↓
main()
```

With the current configuration SES reaches `main()` correctly.

------

# Building

After configuring the two Global Macros:

```text
AMBIQSUITE_ROOT
ARM_GCC_ROOT
```

open the `.emProject` file in SEGGER Embedded Studio.

Run:

```text
Build → Rebuild
```

No changes to include paths, library paths, startup files or linker paths should be required.

------

# Programming and Debugging

Connect the Apollo510B EVB using J-Link.

Verify:

```text
Target Device = AP510BFA-CBR
```

Then start a debug session.

SES should:

```text
connect
→ program MRAM
→ initialize MSP from __StackTop
→ enter Reset_Handler
→ stop at main()
```

------

# Portability Test

Project portability has been explicitly tested.

The complete project was copied to a different filesystem directory and then opened from its new location.

The following operations completed successfully without modifying project paths:

```text
Clean
Rebuild
Flash
Debug
```

The application correctly reached `main()`.

This confirms that project-local files use relative paths and external dependencies are resolved through:

```text
$(AMBIQSUITE_ROOT)
$(ARM_GCC_ROOT)
```

------

# Quick Start

For a new developer:

```text
1. Clone the repository.

2. Install AmbiqSuite SDK 5.2.0.

3. Install Arm GNU Toolchain 14.2.Rel1.

4. Install SEGGER Embedded Studio.

5. Configure SES Global Macros:

   AMBIQSUITE_ROOT=<AmbiqSuite root>
   ARM_GCC_ROOT=<Arm GNU Toolchain root>

6. Open the .emProject file.

7. Verify:

   J-Link Target Device = AP510BFA-CBR

8. Rebuild the project.

9. Connect the Apollo510B EVB and AMAUD1 extension board.

10. Start Debug.
```

No source or project path modifications should normally be required.

------

# Audio DMA Architecture

The audio transport is DMA-driven.

Conceptually:

```text
                    Apollo510B

      ADAU1777
          │
          │ I2S RX
          ▼
┌────────────────────┐
│ RX DMA             │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ RX Ping/Pong       │
│ Buffers            │
└─────────┬──────────┘
          │
          │ cache invalidate
          │
          ▼
┌────────────────────┐
│ RX → TX copy       │
└─────────┬──────────┘
          │
          │ cache clean
          │
          ▼
┌────────────────────┐
│ TX Ping/Pong       │
│ Buffers            │
└─────────┬──────────┘
          │
          ▼
┌────────────────────┐
│ TX DMA             │
└─────────┬──────────┘
          │
          │ I2S TX
          ▼
       ADAU1777
```

The CPU is involved in buffer management but does not transfer individual samples.

------

# Cache Coherency

Apollo510B cache coherency must be explicitly handled when DMA accesses cacheable buffers.

The current implementation performs the required cache operations around RX and TX buffers.

Conceptually:

```text
RX DMA complete
      ↓
invalidate RX cache
      ↓
CPU accesses received samples
      ↓
copy/process RX → TX
      ↓
clean TX cache
      ↓
TX DMA
```

This becomes especially important when DSP or AI processing is inserted between RX and TX.

------

# ADAU1777 Configuration

The ADAU1777 codec is initialized before continuous I2S streaming starts by `drivers/adau1777.c`. The driver uses the public ADAU1777 control-register interface directly and does not load DSP program or parameter memory.

For this project the codec is configured as a 24 kHz stereo I2S slave. AIN2/AIN3 are converted by ADC2/ADC3 and routed through the output ASRCs to the serial output. The serial input is routed through the input ASRCs directly to DAC0/DAC1, allowing the DSP core to remain disabled.

The tested hardware configuration uses the 24.576 MHz external MCLK provided to the ADAU1777. Apollo510B generates the I2S BCLK and LRCLK/WS signals; the ADAU1777 operates as the serial-port clock slave.

When changing:

- sample rate
- serial port format
- slot width
- ADC/DAC routing
- decimation filters
- interpolation filters
- external MCLK

the Apollo510 I2S configuration and ADAU1777 configuration must remain consistent.

------

# Future AI Audio Integration

The current project intentionally contains no AI processing.

The working audio loopback serves as the baseline for future integration with:

- Ambiq NeuralSPOT
- NeuralSPOT-X
- Helia
- audio preprocessing
- embedded inference
- audio postprocessing

The intended future signal path is:

```text
ADAU1777
    ↓
I2S RX DMA
    ↓
Ping/Pong Audio Buffer
    ↓
Preprocessing
    ↓
AI Inference
    ↓
Postprocessing
    ↓
I2S TX DMA
    ↓
ADAU1777
```

The existing loopback should remain available as a reference configuration while AI functionality is introduced.

------

# Planned Performance Measurements

During AI integration the following parameters should be measured:

- inference latency
- CPU utilization
- RAM usage
- MRAM / Flash usage
- tensor arena size
- model working memory
- available processing time between DMA buffers
- DMA buffer overrun / underrun
- end-to-end audio latency
- impact of inference on continuous audio streaming

------

# Memory Strategy for Future AI Workloads

The current linker exposes the complete Apollo510B internal memory map.

Normal `.data` and `.bss` data are currently placed primarily in DTCM.

Large future AI allocations should make deliberate use of the 3 MB System SRAM region:

```text
0x20080000 - 0x2037FFFF
```

Potential future dedicated sections may include:

```text
.ai_sram
.tensor_arena
.audio_sram
.ai_scratch
```

This can keep latency-sensitive data in DTCM while using System SRAM for larger AI working buffers.

ITCM may similarly be used selectively for performance-critical code.

------

# Development Strategy

The project follows a layered validation approach:

```text
SES / build system
        ↓
startup / linker
        ↓
AmbiqSuite HAL
        ↓
ADAU1777
        ↓
I2S
        ↓
DMA
        ↓
ping/pong buffering
        ↓
cache coherency
        ↓
continuous audio
        ↓
NeuralSPOT / NSX
        ↓
AI model
```

The basic rule is:

> Keep the working audio loopback unchanged until the SES/GCC baseline has been validated.

AI functionality should then be introduced incrementally so that any new issue can be attributed to the correct software layer.

------

# Current Status

The following functionality has been validated:

```text
[PASS] SES project build
[PASS] Arm GNU Toolchain build
[PASS] Apollo510B startup based on AmbiqSuite SDK 5.2.0
[PASS] AmbiqSuite 5.2.0-based GNU linker script with SES stack symbols
[PASS] J-Link connection
[PASS] Apollo510B MRAM programming
[PASS] debugger startup
[PASS] breakpoint / run to main()
[PASS] direct register-level ADAU1777 initialization
[PASS] I2S RX
[PASS] I2S TX
[PASS] RX DMA
[PASS] TX DMA
[PASS] ping-pong buffering
[PASS] cache management
[PASS] continuous full-duplex audio loopback
[PASS] project relocation / portability test
```

The project therefore provides a stable **Apollo510B + ADAU1777 SES/GCC audio baseline** suitable for further DSP and AI development.

## License and Third-Party Components

Project-authored source code is released under the **BSD 3-Clause License**. See [LICENSE](LICENSE) for the applicable terms.

A small number of files derived from **AmbiqSuite SDK 5.2.0** are included in the repository because they form part of the tested Apollo510B startup and memory configuration. Those files retain their original Ambiq copyright and license terms.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the included third-party components, provenance information and applicable license notice.

**AmbiqSuite SDK itself is not distributed with this repository.** Users must obtain AmbiqSuite separately and configure `AMBIQSUITE_ROOT` to point to their local SDK installation.

Third-party product names and trademarks are the property of their respective owners.
