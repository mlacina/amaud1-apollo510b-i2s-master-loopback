#!/usr/bin/env python3

from pathlib import Path
import sys

if len(sys.argv) != 3:
    print("Usage:")
    print("  python patch_jlink.py <source.JLinkScript> <output.JLinkScript>")
    sys.exit(1)

src = Path(sys.argv[1])
dst = Path(sys.argv[2])

if src.resolve() == dst.resolve():
    raise RuntimeError("Source and output files must be different.")

text = src.read_text(encoding="utf-8")

PATCH_MARKER = "SES workaround: setting PC to application Reset_Handler"

if PATCH_MARKER in text:
    print("Patch already applied.")
    sys.exit(0)


# --------------------------------------------------------------------
# 1. Add AppPC variable
# --------------------------------------------------------------------

marker = "    int timeout;"

replacement = """    int timeout;
    int AppPC;
"""

if marker not in text:
    raise RuntimeError("Cannot find variable marker: int timeout;")

text = text.replace(marker, replacement, 1)


# --------------------------------------------------------------------
# 2. Insert SetCoreReg() before ResetTarget()
# --------------------------------------------------------------------

marker = "int ResetTarget(void)"

helper = r"""
//
// Local helper for SES/J-Link workaround.
//
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

"""

if marker not in text:
    raise RuntimeError("Cannot find ResetTarget()")

text = text.replace(marker, helper + marker, 1)


# --------------------------------------------------------------------
# 3. Insert workaround at beginning of successful ResetTarget exit
# --------------------------------------------------------------------

marker = """    if ( timeout == 0 )
    {
"""

patch = r"""    if ( timeout == 0 )
    {
        //
        // Local workaround for SEGGER Embedded Studio / J-Link.
        //
        // Apollo5 secure reset may leave the CPU halted with PC at an
        // address that SES cannot read. Redirect the debug PC to the
        // application Reset_Handler before returning to the debugger.
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
        }

"""

if marker not in text:
    raise RuntimeError("Cannot find ResetTarget success marker")

text = text.replace(marker, patch, 1)


# --------------------------------------------------------------------
# Write local patched copy
# --------------------------------------------------------------------

dst.write_text(text, encoding="utf-8")

print("Patch applied.")
print("Source :", src)
print("Output :", dst)