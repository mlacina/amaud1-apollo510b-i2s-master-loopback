## J-Link timing and erased-device recovery

The local J-Link workaround uses different timing paths for an **erased/blank device** and for a device that already contains a valid application image. The delays below are intentional and are used only where needed to allow the Apollo5 Secure BootLoader (SBL) and the SWD debug interface to reach a stable state.

### Erased / blank device

`SetupTarget()` checks the application reset vector at `0x00410004`. If the vector is erased or outside the expected application range, the device is treated as blank.

The recovery sequence is:

1. **Initial SBL recovery wait: 3000 ms**

   After a blank image is detected, the script waits:

   ```c
   SYS_Sleep(3000);
   ```

   This gives the SBL time to complete its normal wired/UART recovery window without repeated SWD accesses.

2. **SWD recovery attempts: up to 6 attempts, 500 ms apart**

   After the initial 3-second wait, the script tries to restore the SWD/DAP connection. If the first attempt fails, another attempt is made after 500 ms.

   The nominal attempt times are therefore approximately:

   ```text
   3.0 s
   3.5 s
   4.0 s
   4.5 s
   5.0 s
   5.5 s
   ```

   The loop exits immediately after the first successful recovery, so a device whose SBL recovery finishes after about 3 seconds does not have to wait for the full retry window.

3. **SWD stabilization delay after successful recovery: 100 ms**

   After a successful SWD recovery attempt, the script waits an additional:

   ```c
   SYS_Sleep(100);
   ```

   before performing further debug operations. This short delay allows the recovered debug interface to stabilize before the core is halted and its registers are modified.

4. **Core halt polling: 50 ms intervals, maximum 20 attempts**

   After requesting `C_HALT`, the script checks `S_HALT` every 50 ms:

   ```text
   20 attempts x 50 ms = approximately 1 second maximum
   ```

   This is a timeout only. The loop exits immediately when the core is observed in the halted state.

5. **Blank-device `ResetTarget()` fast path**

   SEGGER Embedded Studio normally calls `ResetTarget()` again before downloading the image. Repeating a full `SYSRESETREQ` at this point would cause the blank device to enter the SBL recovery sequence a second time.

   The workaround therefore checks whether:

   - the application is still blank, and
   - the core has already been halted by `SetupTarget()`.

   If both conditions are true, the redundant reset is skipped and `ResetTarget()` returns immediately after restoring the safe debug context.

   This fast path adds no intentional delay.

   In a representative test, the blank-device sequence completed approximately as follows:

   ```text
   SetupTarget()                     ~3.15 s
   ResetTarget() blank fast path     ~3.6 ms
   JLINK_BeginDownload()             ~3.35 s from session start
   ```

### Programmed device / normal reset path

A device with a valid application reset vector does **not** enter the blank-device delay sequence in `SetupTarget()`.

The original Apollo5 reset timing is retained for the normal `ResetTarget()` path.

1. **Post-`SYSRESETREQ` delay: 350 ms**

   The original device script contains:

   ```c
   SYS_Sleep(350);
   ```

   This delay is intentionally preserved.

2. **CPU halt polling: 100 ms intervals**

   After the 350 ms delay, the script polls `DHCSR` approximately every 100 ms while waiting for the CPU to reach a stable halted state.

3. **Extended reset timeout: 80 attempts**

   The original short timeout is extended to:

   ```text
   80 attempts x 100 ms = approximately 8 seconds maximum
   ```

   This is a maximum timeout, not a fixed delay. The loop exits as soon as the normal halt condition is satisfied.

### Timing summary

| Condition | Delay / retry | Purpose |
|---|---:|---|
| Blank image detected | 3000 ms fixed | Allow the normal SBL wired/UART recovery window to complete |
| SWD recovery after blank wait | Up to 6 attempts, 500 ms apart | Recover SWD as soon as it becomes available |
| Successful SWD recovery | 100 ms fixed | Allow SWD/debug state to stabilize |
| Halt after SWD recovery | 20 x 50 ms maximum | Wait for `S_HALT`; exits early on success |
| Blank `ResetTarget()` fast path | No intentional delay | Avoid a second SBL recovery cycle before download |
| Normal `ResetTarget()` | 350 ms fixed | Preserve the original Apollo5 reset timing |
| Normal reset halt polling | Up to 80 x 100 ms | Extended safety timeout; exits early on success |

> **Note**
>
> The times above are nominal script delays. Actual elapsed time can be slightly longer because J-Link/SWD transactions and target-side operations also consume time.
>
> The blank-device timing is intentionally asymmetric: the first 3 seconds are a quiet SBL recovery period, while the following retry window is adaptive. Once SWD is successfully recovered, the script continues immediately after the additional 100 ms stabilization delay.
