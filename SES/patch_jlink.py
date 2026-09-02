#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026 Mariusz Łacina
#
# This script creates a locally patched copy of the Ambiq/SEGGER
# J-Link device script. The generated J-Link script is not covered
# by the BSD-3-Clause license and remains subject to the copyright
# and license terms of the original vendor script.

from pathlib import Path
import sys

if len(sys.argv) != 3:
    print("Usage:")
    print("  python patch_jlink.py <source.JLinkScript> <output.JLinkScript>")
    sys.exit(1)

src = Path(sys.argv[1])
dst = Path(sys.argv[2])

if not src.is_file():
    raise RuntimeError(f"Source file does not exist: {src}")

if src.resolve() == dst.resolve():
    raise RuntimeError("Source and output files must be different.")

if not dst.parent.exists():
    raise RuntimeError(f"Output directory does not exist: {dst.parent}")

text = src.read_text(encoding="utf-8")

NEW_PATCH_MARKER = "SES workaround: blank application; initial 3-second SBL recovery wait."
LEGACY_PATCH_MARKER = "SES workaround: setting PC to application Reset_Handler"

if NEW_PATCH_MARKER in text:
    print("Patch already applied.")
    sys.exit(0)

if LEGACY_PATCH_MARKER in text:
    raise RuntimeError(
        "Source appears to contain an older SES workaround. "
        "Use the original installed Apollo330P_510L.JLinkScript as input."
    )


# --------------------------------------------------------------------
# 1. Add AppPC variable to ResetTarget()
# --------------------------------------------------------------------

marker = "    int timeout;"
replacement = """    int timeout;
    int AppPC;
"""

if marker not in text:
    raise RuntimeError("Cannot find variable marker: int timeout;")

text = text.replace(marker, replacement, 1)


# --------------------------------------------------------------------
# 2. Insert helper and SetupTarget() before ResetTarget()
# --------------------------------------------------------------------

marker = "int ResetTarget(void)"

setup_patch = r'''//
// Local helpers for SES/J-Link workaround.
//
int TryRecoverSWD(void)
{
    int AHBAP_REG_CTRL;
    int Ctrl;
    int retval;

    AHBAP_REG_CTRL = 0;

    //
    // Configure the active debug interface in the same way as
    // cfg_swd_interface(), but return a status to the caller.
    //
    if (MAIN_ActiveTIF == JLINK_TIF_JTAG)
    {
        JLINK_CORESIGHT_Configure("IRPre=0;DRPre=0;IRPost=0;DRPost=0;IRLenDevice=4");
    }
    else
    {
        JLINK_CORESIGHT_Configure("");
    }

    //
    // Power-up complete DAP.
    //
    Ctrl = 0            |
           (1 << 30)    |   // System power-up
           (1 << 28)    |   // Debug power-up
           (1 << 5);        // Clear STICKYERR

    retval = JLINK_CORESIGHT_WriteDP(1, Ctrl);
    if (retval < 0)
    {
        return -1;
    }

    //
    // Select AHB-AP.
    //
    retval = JLINK_CORESIGHT_WriteDP(2, (0 << 24) | (0xD00 << 0));
    if (retval < 0)
    {
        return -1;
    }

    //
    // Configure AHB-AP as in cfg_swd_interface().
    //
    JLINK_CORESIGHT_WriteAP(
        AHBAP_REG_CTRL,
        (1 << 4) | (1 << 24) | (1 << 25) | (1 << 29) | (2 << 0)
    );

    return 0;
}

void SetCoreReg(int RegNo, int Value)
{
    int v;

    JLINK_MEM_WriteU32(0xE000EDF8, Value);               // DCRDR
    JLINK_MEM_WriteU32(0xE000EDF4, 0x00010000 | RegNo); // DCRSR

    do
    {
        v = JLINK_MEM_ReadU32(0xE000EDF0);               // DHCSR
    }
    while ((v & 0x00010000) == 0);                       // S_REGRDY
}

int SetupTarget(void)
{
    int AppPC;
    int v;
    int Tries;
    int Recovered;

    Report(" SES workaround: SetupTarget()");

    //
    // Detect blank or invalid application image.
    //
    AppPC = JLINK_MEM_ReadU32(0x00410004);
    AppPC = AppPC & 0xFFFFFFFE;

    if ((AppPC < 0x00410000) || (AppPC >= 0x00600000))
    {
        Report(" SES workaround: blank application; initial 3-second SBL recovery wait.");

        //
        // Give SBL its normal recovery window before touching SWD.
        //
        SYS_Sleep(3000);

        //
        // Then try to recover SWD up to 6 times, 500 ms apart.
        // This gives attempts at approximately:
        // 3.0, 3.5, 4.0, 4.5, 5.0 and 5.5 seconds.
        //
        Tries     = 0;
        Recovered = 0;

        do
        {
            Tries = Tries + 1;

            if (TryRecoverSWD() == 0)
            {
                Recovered = 1;
                Report1(" SES workaround: SWD recovered on attempt ", Tries);

                //
                // Allow the recovered SWD/debug state to stabilize before
                // performing any further debug operation.
                //
                SYS_Sleep(100);
            }
            else if (Tries < 6)
            {
                SYS_Sleep(500);
            }
        }
        while ((Recovered == 0) && (Tries < 6));

        if (Recovered != 0)
        {
            //
            // Halt the core after SBL recovery.
            //
            v = JLINK_MEM_ReadU32(0xE000EDF0);
            v &= 0x3F;
            v |= 0xA05F0000;
            v |= 0x00000002;    // C_HALT
            v |= 0x00000001;    // C_DEBUGEN
            JLINK_MEM_WriteU32(0xE000EDF0, v);

            //
            // Bounded wait for S_HALT.
            //
            Tries = 0;
            do
            {
                SYS_Sleep(50);
                v = JLINK_MEM_ReadU32(0xE000EDF0);
                Tries = Tries + 1;
            }
            while (((v & 0x00020000) == 0) && (Tries < 20));

            if ((v & 0x00020000) != 0)
            {
                Report(" SES workaround: core halted; setting safe PC.");
                SetCoreReg(15, 0x00410000);
                SetCoreReg(16, 0x01000000);   // xPSR: T-bit = 1
            }
            else
            {
                Report(" SES workaround: unable to halt core after SBL recovery.");
            }
        }
        else
        {
            Report(" SES workaround: SWD recovery failed after 6 attempts.");
        }
    }

    return 0;
}

'''

if marker not in text:
    raise RuntimeError("Cannot find ResetTarget()")

text = text.replace(marker, setup_patch + marker, 1)


# --------------------------------------------------------------------
# 3. Add erased-device fast path to ResetTarget()
# --------------------------------------------------------------------
#
# SetupTarget() already waits for SBL recovery, restores SWD, halts the
# core and sets PC to 0x00410000 for a blank image.  When ResetTarget()
# is called immediately afterwards, avoid issuing a redundant SYSRESETREQ
# if the image is still blank and the core is already halted.
#
# If the core is not halted, fall through to the original ResetTarget()
# sequence unchanged.
#

marker = """    //
    // Configure the interface
    //
    cfg_swd_interface();

    //
    // Enable Debug and Halt the MCU Core.
"""

replacement = r"""    //
    // Configure the interface
    //
    cfg_swd_interface();

    //
    // Fast path for an erased device already prepared by SetupTarget().
    //
    AppPC = JLINK_MEM_ReadU32(0x00410004);
    AppPC = AppPC & 0xFFFFFFFE;

    if ((AppPC < 0x00410000) || (AppPC >= 0x00600000))
    {
        v = JLINK_MEM_ReadU32(DHCSR_ADDR);

        if ((v != 0xFFFFFFFF) && ((v & 0x00020000) != 0))
        {
            Report(" SES workaround: blank image already prepared; skipping redundant reset.");
            SetCoreReg(15, 0x00410000);
                SetCoreReg(16, 0x01000000);   // xPSR: T-bit = 1
            return 0;
        }
    }

    //
    // Enable Debug and Halt the MCU Core.
"""

if marker not in text:
    raise RuntimeError("Cannot find ResetTarget interface marker")

text = text.replace(marker, replacement, 1)


# --------------------------------------------------------------------
# 4. Extend ResetTarget() wait so blank-device SBL recovery can finish
# --------------------------------------------------------------------

marker = "if ((Tries >= 10) && (Done != 1))"
replacement = "if ((Tries >= 80) && (Done != 1))"

if marker not in text:
    raise RuntimeError("Cannot find ResetTarget timeout marker")

text = text.replace(marker, replacement, 1)
text = text.replace(
    "// wait for up to 0.5 seconds.",
    "// allow time for SBL recovery on blank devices.",
    1,
)


# --------------------------------------------------------------------
# 5. Insert workaround at beginning of successful ResetTarget exit
# --------------------------------------------------------------------

marker = """    if ( timeout == 0 )
    {
"""

reset_patch = r'''    if ( timeout == 0 )
    {
        //
        // Local workaround for SEGGER Embedded Studio / J-Link.
        //
        // Apollo5 reset may leave the CPU halted with PC at an address
        // that SES cannot read. Redirect the debug PC to the application
        // Reset_Handler, or to a safe MRAM address for a blank image.
        //
        AppPC = JLINK_MEM_ReadU32(0x00410004);
        AppPC = AppPC & 0xFFFFFFFE;

        if ((AppPC >= 0x00410000) && (AppPC < 0x00600000))
        {
            Report1(" SES workaround: setting PC to application Reset_Handler: ", AppPC);
            SetCoreReg(15, AppPC);
        }
        else
        {
            Report1(" SES workaround: invalid application Reset_Handler: ", AppPC);
            SetCoreReg(15, 0x00410000);
                SetCoreReg(16, 0x01000000);   // xPSR: T-bit = 1
        }

'''

if marker not in text:
    raise RuntimeError("Cannot find ResetTarget success marker")

text = text.replace(marker, reset_patch, 1)


# --------------------------------------------------------------------
# Write local patched copy
# --------------------------------------------------------------------

dst.write_text(text, encoding="utf-8")

print("Patch applied.")
print("Source :", src)
print("Output :", dst)
