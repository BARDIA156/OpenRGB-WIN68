# OpenRGB AULA WIN68 Plugin  **[برای زبان پارسی روی من بزن](README-Persian.md)**

![Project Logo](assets/Logo.png)

Connect your AULA WIN68 HE keyboard to **OpenRGB** and sync its lighting with your other RGB devices.

> [!IMPORTANT]
> This project is still new and may contain bugs. If you find a problem, please report it in the [Issues](https://github.com/BARDIA156/OpenRGB-WIN68/issues) section.

## ✨ Features

- Supports the keyboard's built-in lighting effects

- Lets you select a color for each key
- Works with **OpenRGBEffectsPlugin**
- Includes brightness and speed controls
- Works alongside other devices supported by **OpenRGB**
- Shows an accurate key layout in **Toggle LED View**
- Easy drag-and-drop installation
- Includes the source code and a compiled version

## 🖥️ Demo Video

[![Watch the demo video](https://img.youtube.com/vi/DybYsERCELA/maxresdefault.jpg)](https://youtu.be/DybYsERCELA)

## 🪛 Installation

1. Download the latest version from [Releases](https://github.com/BARDIA156/OpenRGB-WIN68/releases).
2. Extract the downloaded file.
3. Move the extracted plugin files into the OpenRGB `plugins` folder. It is usually located here:

   `C:\Program Files\OpenRGB\plugins`

If you cannot find the OpenRGB folder, right-click the OpenRGB shortcut and select **Open file location**.

Restart OpenRGB after installing the plugin.

## 🔧 Building from Source with GitHub Actions

Precompiled versions are available on the [Releases](https://github.com/BARDIA156/OpenRGB-WIN68/releases) page. If you prefer to build the plugin yourself, you can use GitHub Actions on your own fork:

1. Fork this repository to your GitHub account.
2. Open the **Actions** tab in your fork and enable workflows if prompted.
3. Select the build workflow from the left sidebar.
4. Click **Run workflow**, choose the branch, and confirm.

5. Once the build finishes successfully, open the workflow run and download the plugin from **Artifacts**.

The workflow file must be included in your fork under `.github/workflows/build-plugin.yml` and support manual runs (`workflow_dispatch`).

## 🔧 Building Locally

The local build guide is not available yet. The project uses the OpenRGB `release_1.0` source and Qt 6.8.3 in its GitHub Actions build. More local build instructions may be added later.

## 🔎 About the Keyboard

For more detailed information about the keyboard and its reverse engineering, check out [Aether-HE](https://github.com/MrWhosNexus/Aether-HE), created by **MrWhosNexus**.

His work made it much easier to understand how this keyboard works. Thank you for sharing it!

---

Made with ❤️ and 🍵 by **BARDIA156**
