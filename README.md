<div align="center">
  <img src="assets/logo.png" alt="OpenRGB-WIN68 logo" width="180" style="width: min(180px, 42vw); height: auto;">

  <h1>OpenRGB-WIN68</h1>
  <p><strong>Per-key RGB control for the AULA WIN 68 HE</strong><br>Built for OpenRGB 1.0 · Plugin API v5 · Windows x64</p>

  <p>
    <a href="#installation--setup"><img src="https://img.shields.io/badge/GitHub%20Release-v0.7.0-5b5bff?style=for-the-badge&logo=github" alt="GitHub Release v0.7.0"></a>
    <a href="https://openrgb.org/"><img src="https://img.shields.io/badge/OpenRGB-1.0-5b5bff?style=for-the-badge" alt="OpenRGB 1.0"></a>
    <a href="https://openrgb.org/"><img src="https://img.shields.io/badge/Plugin%20API-v5-3978c5?style=for-the-badge" alt="Plugin API v5"></a>
    <a href="https://www.gnu.org/licenses/old-licenses/gpl-2.0.html"><img src="https://img.shields.io/badge/License-GPL--2.0-blue?style=for-the-badge" alt="GPL-2.0 license"></a>
    <a href="#installation--setup"><img src="https://img.shields.io/badge/Platform-Windows%20x64-0078D4?style=for-the-badge&logo=windows" alt="Windows x64"></a>
    <a href="INSTALL-fa.md"><img src="https://img.shields.io/badge/راهنمای%20فارسی-راهنما-239b56?style=for-the-badge" alt="راهنمای فارسی"></a>
  </p>
</div>

---

## Demo

<div align="center">
  <a href="https://www.youtube.com/watch?v=YOUR_YOUTUBE_VIDEO_ID">
    <img src="https://img.youtube.com/vi/YOUR_YOUTUBE_VIDEO_ID/maxresdefault.jpg" alt="Watch the OpenRGB-WIN68 video demo" width="720" style="max-width: 100%; height: auto;">
  </a>
  <br>
  <sub>Select the preview to watch the demo. Replace <code>YOUR_YOUTUBE_VIDEO_ID</code> with the published video ID.</sub>
</div>

## Navigation

- [Overview](#overview)
- [Key features](#key-features)
- [Installation & setup](#installation--setup)
- [Hardware & protocol specifications](#hardware--protocol-specifications)
- [Troubleshooting & tips](#troubleshooting--tips)
- [Building from source](#building-from-source)
- [License & author](#license--author)

## Overview

OpenRGB-WIN68 is a Windows x64 plugin that brings the **AULA WIN 68 HE** keyboard into OpenRGB as a controllable device. It provides a named per-key layout, firmware lighting modes, and a clickable keyboard preview in the plugin settings. The plugin targets **OpenRGB 1.0 (Plugin API v5)** and communicates with the keyboard through its vendor HID interface.

## Key features

- 🎨 **Per-key color control** for the keyboard's 68 mapped keys.
- 🌈 **Firmware lighting modes** alongside direct color control.
- ⌨️ **Readable key layout** presented as a 5 × 16 OpenRGB matrix.
- 🖱️ **Clickable keyboard preview** for selecting a key and assigning its color.
- 🔌 **OpenRGB SDK compatibility** for controller and zone color updates.
- ⚡ **Efficient HID updates** that avoid resending unchanged lighting data.

## Installation & setup

### Prerequisites

- Windows 10 or 11, 64-bit
- OpenRGB **1.0**
- AULA **WIN 68 HE** keyboard
- The plugin build artifact (`OpenRGBAulaHE.dll`)

### Steps

1. **Close competing keyboard software.** Exit Aether and any other utility that may access the keyboard's vendor HID interface.
2. **Get the plugin build.** Download the latest successful **Build AULA WIN68 HE plugin** artifact from this repository's **Actions** tab. The artifact is named `OpenRGBAulaHE-0.7.0-OpenRGB-1.0-windows-x64`.
3. **Install the plugin.** Extract the artifact, then copy `OpenRGBAulaHE.dll` into the OpenRGB `plugins` folder. If the artifact includes `hidapi-hotplug.dll`, use it only when needed; do not replace a newer copy already supplied by OpenRGB.
4. **Restart OpenRGB.** Launch OpenRGB 1.0, enable the plugin if necessary, and confirm that **AULA WIN 68 HE** appears in the device list.
5. **Check the layout.** In OpenRGB, leave **Disable Key Expansion** unchecked so wide keys display correctly. Use the plugin settings preview to inspect and select individual keys.
6. **Test gently.** Start with a static color or a firmware effect before trying frequent per-key updates.

> [!NOTE]
> **فارسی:** راهنمای نصب فارسی در [INSTALL-fa.md](INSTALL-fa.md) در دسترس است.

> [!WARNING]
> Rapid Custom/per-key updates have been observed to coincide with missed or temporarily held key presses. The cause is unconfirmed, and a safe continuous update rate has not been established. Stop the effect immediately if keyboard input behaves unexpectedly.

## Hardware & protocol specifications

| Specification | Details |
| --- | --- |
| Device | AULA WIN 68 HE RGB keyboard |
| USB VID:PID | `2E3C:C365` |
| Product strings | `WIN 68`, `SI2828HEARGB`, `SI2828KZHEARGB` |
| HID usage | Vendor usage page `0xFF1B` |
| Key mapping | 68 named keys in a 132-slot firmware index table |
| OpenRGB matrix | 5 × 16; standardized key expansion is used for wide keys |
| Lighting protocol | Command `0x07` — lighting mode and effect settings |
| Per-key palette protocol | Command `0x09` — Custom palette data |

> **Protocol note:** The USB identity, product-string checks, and command mappings are based on available device research. The key indices are provisional and may not be physically verified on every unit. VID:PID alone is not unique to this keyboard.

## Troubleshooting & tips

| Issue | What to try |
| --- | --- |
| Device or plugin is missing | Confirm OpenRGB 1.0 and Windows x64, enable the plugin, then reconnect the keyboard and restart OpenRGB. |
| Device is not detected or HID writes fail | Close Aether and other keyboard utilities first. Only one application should control the vendor HID interface at a time. Reconnect the keyboard and try again. |
| Keys look compressed or uneven in OpenRGB | Keep **Disable Key Expansion** unchecked. The device matrix uses OpenRGB's standardized wide-key expansion; the plugin preview reflects the keyboard layout more closely. |
| A key appears in the wrong position | Treat the current 68-key map as provisional. Check the key in the Settings preview and report the affected key and device details. |
| Typing becomes delayed, missed, or stuck during an effect | **Stop the effect immediately.** Frequent per-key updates can coincide with input issues; avoid sustained high-rate writes. |
| Custom mode does not behave as expected | Try a built-in firmware effect first, then test a small number of per-key changes. The persistent palette behavior may vary by device firmware. |

## Building from source

### Local Windows build

Use an **x64 Visual Studio 2022 Developer PowerShell** with MSVC tools, Qt 6.8.3 for MSVC 2022 x64, and a matching OpenRGB 1.0 source tree containing `OpenRGBPluginInterface.h`.

```powershell
./scripts/build-plugin.ps1 `
  -OpenRGBSource .\vendor\openrgb-1.0 `
  -OutputDirectory .\dist
```

The script builds `OpenRGBAulaHE.dll` and places it in the selected output directory. Make sure `qmake.exe` and `nmake.exe` are available in the developer environment.

### GitHub Actions build

The **Build AULA WIN68 HE plugin** workflow builds on Windows Server 2022 with MSVC 2022 x64 and Qt 6.8.3, using the matching OpenRGB 1.0 source. Run it from the repository's **Actions** tab; the resulting DLL and applicable HID library are published as a downloadable artifact.

## License & author

This project is licensed under **GNU GPL-2.0**. See the [GNU GPL-2.0 license text](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).

**Author:** Bardia Dehbozorgi

---

<div align="center"><sub>OpenRGB-WIN68 · AULA WIN 68 HE · OpenRGB 1.0</sub></div>
