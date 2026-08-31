# Third-Party Notices

This repository contains a small number of source files derived from the
AmbiqSuite SDK. These files are included only where they are needed for the
tested Apollo510B startup and memory configuration.

Project-authored source code is licensed separately under the repository
[LICENSE](LICENSE).

## Ambiq Micro, Inc. - AmbiqSuite SDK 5.2.0

Source package:

- AmbiqSuite SDK 5.2.0
- Official source/download page: https://contentportal.ambiq.com/en/apollo510b

Included Ambiq-originated files:

```text
third_party/Ambiq/startup_gcc.c
third_party/Ambiq/am_resources.c
third_party/Ambiq/linker_script.ld
```

### Project-specific modifications

`startup_gcc.c` is based on the Apollo510B GCC startup source from AmbiqSuite
SDK 5.2.0. The project uses a 12 KiB stack allocation matching the linker
memory layout.

`linker_script.ld` is based on the standard Apollo510B GCC linker script from
AmbiqSuite SDK 5.2.0. The project-specific SEGGER Embedded Studio changes:

- export `__StackLimit` and `__StackTop` for debugger stack initialization;
- explicitly retain the `.stack` input sections.

`am_resources.c` remains an Ambiq-originated runtime component.

The Ambiq-originated files remain subject to the applicable Ambiq copyright
and license terms. Original notices present in those files must be retained.

### Ambiq License Notice

Copyright (c) 2026, Ambiq Micro, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

Third party software included in this distribution is subject to the
additional license terms as defined in the /docs/licenses directory.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

## External Development Tools

The following tools are required or recommended for building and debugging the
project but are not distributed with this repository:

- AmbiqSuite SDK 5.2.0
- Arm GNU Toolchain
- SEGGER Embedded Studio
- SEGGER J-Link software

Each external tool is distributed under its own license terms.
