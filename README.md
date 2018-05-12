# memloader ![License](https://img.shields.io/badge/License-GPLv2-blue.svg)
Parses ini files from microsd root and loads/decompresses/boots the appropriate binaries on the AArch64 CPU of the Nintendo Switch. 

Ini files can be generated from source images using the programs inside tools subdirectory. Currently the tools understand coreboot CBFS images or ELF payloads (like u-boot).

## Usage
 1. Build `memloader.bin` using make from the repository root directory, or download a binary release from https://switchtools.sshnuke.net (tools folder is built separately)
 2. Either put the appropriate ini+binary files onto your microsd card before inserting it into your Switch, or pass the --dataini parameter to TegraRcmSmash.exe to load them via USB.
 2. Send the memloader.bin to your Switch running in RCM mode via a fusee-launcher (sudo ./fusee-launcher.py memloader.bin or just drag and drop it onto TegraRcmSmash.exe on Windows)
 3. Follow the on-screen menu.

## Changes

This section is required by the GPLv2 license

 * initial code based on https://github.com/Atmosphere-NX/Atmosphere
 * everything except fusee-primary been removed (from Atmosphere)
 * all hwinit code has been replaced by the updated versions from https://github.com/nwert/hekate
 * Files pinmux.c/h, carveout.c/h, flow.h, sdram.c/h, decomp.h,lz4_wrapper.c,lzma.c,lzmadecode.c,lz4.c.inc,cbmem.c/h are based on https://github.com/fail0verflow/switch-coreboot.git sources
 * main.c has been modified to display an on-screen menu and either load binaries via ini files on microsd card, or directly via USB transfer from host

## Responsibility

**I am not responsible for anything, including dead switches, loss of life, or total nuclear annihilation.**
