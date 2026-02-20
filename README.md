<img src="assets/sbagenx.png" alt="SBaGenX Logo" width="32" height="32"> SBaGenX - Sequenced Brainwave Generator

SBaGenX is a command-line tool for generating binaural beats and isochronic tones, designed to assist with meditation, relaxation, and altering states of consciousness.

> ⚠️ **SBaGenX is a fork of the project SBaGen+, which is no longer under active development.**  
> Full credit is due, first and foremost, to the father of SBaGen, Jim Peters, and also to the creator of the SBaGen+ fork, Ruan Klein, who added isochronic beats as well as making numerous enhancements.

## 📑 Table of Contents

- [💡 About This Project](#-about-this-project)
- [📥 Installation](#-installation)
  - [🐳 Using Docker for Builds](#-using-docker-for-builds)
  - [⬇️ Download Pre-built Binaries](#️-download-pre-built-binaries)
  - [🐧 Installing on Linux](#-installing-on-linux)
  - [🍎 Installing on macOS](#-installing-on-macos)
  - [🪟 Installing on Windows](#-installing-on-windows)
- [🚀 Basic Usage](#-basic-usage)
- [📚 Documentation](#-documentation)
- [🔍 Research](#-research)
- [🛠️ Compilation](#️-compilation)
  - [📁 Build Scripts Structure](#-build-scripts-structure)
  - [🐳 Building with Docker](#-option-1-using-docker-compose-simplest-method)
  - [💻 Building Natively](#-option-2-building-natively)
- [⚖️ License](#️-license)
- [👏 Credits](#-credits)

## 💡 About This Project

SBaGenX is a fork of SBaGen+, which is itself a fork of the original
SBaGen (Sequenced Binaural Beat Generator) created by Jim Peters. The
original project has not been maintained for many years, and SBaGenX
aims to keep the lineage functional on modern systems while preserving
its core structure. Updates focus on compatibility fixes and practical
feature additions, without major refactoring of the original codebase.

The name has been changed from **"Sequenced Binaural Beat Generator"** to **"Sequenced Brainwave Generator"** to better reflect its expanded functionality. Since SBaGenX now supports isochronic tones in addition to binaural beats, the original name no longer fully represented its capabilities.

## 📥 Installation

Download assets from the [GitHub releases page](https://github.com/lm7137/SBaGenX/releases).

### 🐳 Using Docker for Builds

This repository includes Docker support for **build workflows** (Linux/Windows artifacts), via `Dockerfile` and `compose.yml`.

Use:

```bash
# Build Linux + Windows artifacts
docker compose up build

# Build Linux ARM64 artifacts
docker compose up build-arm64
```

At present, there is no officially published SBaGenX runtime container image for end-user playback.

### ⬇️ Download Pre-built Binaries

Current release asset:

- Windows installer: [sbagenx-windows-setup.exe](https://github.com/lm7137/SBaGenX/releases/download/v2.0.0/sbagenx-windows-setup.exe)
- SHA256: `16B9CE7F3F4F00BA674D184B0C1448D35B59E107F2DCEB41B4B760509544EA88`

  **Important**: Always verify the SHA256 checksum of downloaded binaries against those listed on the [releases page](https://github.com/lm7137/SBaGenX/releases) to ensure file integrity and security.

### 🐧 Installing on Linux

Pre-built Linux binaries are not currently published for `v2.0.0`.
Build from source instead:

```bash
bash linux-build-libs.sh
bash linux-build-sbagenx.sh
```

Then install the produced binary (for example):

```bash
sudo cp dist/sbagenx-linux64 /usr/local/bin/sbagenx
sudo chmod +x /usr/local/bin/sbagenx
sbagenx -h
```

### 🍎 Installing on macOS

Pre-built macOS installer assets are not currently published for `v2.0.0`.
Build on macOS instead:

```bash
bash macos-build-libs.sh
bash macos-build-sbagenx.sh
bash macos-create-installer.sh
```

This creates the macOS artifact in `dist/`.

**Important:** The `SBaGenX` application is not digitally signed, so you may need to add an exception on the `System Settings -> Security & Privacy -> General tab`.

If you want to use SBaGenX as a command-line tool, you can create a symlink to the `sbagenx` binary in your PATH.

```bash
sudo ln -s /Applications/SBaGenX.app/Contents/Resources/bin/sbagenx /usr/local/bin/sbagenx
```

And you can see the usage with:

```bash
sbagenx -h
```

### 🪟 Installing on Windows

1. Download the installer:

   - [sbagenx-windows-setup.exe](https://github.com/lm7137/SBaGenX/releases/download/v2.0.0/sbagenx-windows-setup.exe)

2. Verify the SHA256 checksum of the installer. You can use PowerShell or Command Prompt to do this:

   ```powershell
   Get-FileHash -Algorithm SHA256 .\sbagenx-windows-setup.exe
   # Expected SHA256:
   # 16B9CE7F3F4F00BA674D184B0C1448D35B59E107F2DCEB41B4B760509544EA88
   ```

3. Run the installer and follow the instructions.

⚠️ **Warning about antivirus on Windows**

Some versions of Windows Defender or other antivirus software may falsely detect `SBaGenX` as a threat.

This happens because the executable is **not digitally signed**, and as a command-line program, it may be flagged as suspicious by default.

`SBaGenX` is an open-source project, and the source code is publicly available in this repository for inspection.

✅ **Temporary solution:** if you trust the source of the executable, add an exception in your antivirus for the file or the folder where `SBaGenX` is installed.

## 🚀 Basic Usage

See [USAGE.md](USAGE.md) for more information on how to use SBaGenX.

## 📚 Documentation

For detailed information on all features, see the [SBAGENX.txt](docs/SBAGENX.txt) file.

## 🔍 Research

For the scientific background behind SBaGenX, check out [RESEARCH.md](RESEARCH.md).

## 🛠️ Compilation

SBaGenX can be compiled for macOS, Linux and Windows. The build process is divided into two steps:

1. **Building the libraries**: This step is only necessary if you want MP3 and OGG support (FLAC support is built in)
2. **Building the main program**: This step compiles SBaGenX using the libraries built in the previous step

### 📁 Build Scripts Structure

- **Library build scripts**:

  - `macos-build-libs.sh`: Builds libraries for macOS (universal binary - ARM64 + x86_64)
  - `linux-build-libs.sh`: Builds libraries for Linux (32-bit, 64-bit, ARM64 [if native])
  - `windows-build-libs.sh`: Builds libraries for Windows using MinGW (cross-compilation)

- **Main program build scripts**:
  - `macos-build-sbagenx.sh`: Builds SBaGenX for macOS (universal binary - ARM64 + x86_64)
  - `linux-build-sbagenx.sh`: Builds SBaGenX for Linux (32-bit, 64-bit, ARM64 [if native])
  - `windows-build-sbagenx.sh`: Builds SBaGenX for Windows using MinGW (cross-compilation)

#### 🐳 Option 1: Using Docker Compose (Simplest Method)

The easiest way to build SBaGenX for Linux and Windows is using Docker Compose:

```bash
# Build all Linux and Windows binaries with a single command
docker compose up build

# Build for Linux ARM64
docker compose up build-arm64
```

This will automatically build the Docker image and run all necessary build scripts to generate the binaries for Linux and Windows. All compiled binaries will be placed in the `dist` directory.

**For macOS**, you need compile natively. See next section for more details.

#### 💻 Option 2: Building Natively

If you prefer to build without Docker, you can use the build scripts directly on your system, provided you have all the necessary dependencies installed.

You can see the dependencies in the [Dockerfile](Dockerfile). For macOS, you need the Xcode command line tools installed and home brew installed..

The build scripts are:

```
<platform>-build-libs.sh # macOS, Linux, Windows
<platform>-build-sbagenx.sh # macOS, Linux, Windows
<platform>-create-installer.sh # macOS, Windows
```

Run the script with the `platform` you use. This will create a installers and binaries in the `dist` directory.

For Windows builds, `windows-build-sbagenx.sh` supports two encoder
integration modes:

1. Dynamic runtime loading (default fallback): searches and bundles
   `libsndfile` / `libmp3lame` runtime DLLs into `dist/` for the
   installer.
   You can keep these in-repo at:
   - `libs/windows-win32-runtime/`
   - `libs/windows-win64-runtime/`
   Required runtime DLL set:
   - `libsndfile-1.dll`
   - `libmp3lame-0.dll`
   - `libFLAC.dll`
   - `libmpg123-0.dll`
   - `libogg-0.dll`
   - `libopus-0.dll`
   - `libvorbis-0.dll`
   - `libvorbisenc-2.dll`
   - `libwinpthread-1.dll`
   - `libgcc_s_dw2-1.dll` (Win32 only)
2. Static output encoder link path: if both of these files exist for an
   architecture, the script enables `STATIC_OUTPUT_ENCODERS` and links
   them into the executable:
   - `libs/windows-win32-libsndfile.a`
   - `libs/windows-win32-libmp3lame.a`
   - `libs/windows-win64-libsndfile.a`
   - `libs/windows-win64-libmp3lame.a`

If extra static link dependencies are needed, pass them via:
- `SBAGENX_STATIC_ENCODER_DEPS_WIN32`
- `SBAGENX_STATIC_ENCODER_DEPS_WIN64`

## ⚖️ License

SBaGenX is distributed under the GPL license. See [COPYING.txt](COPYING.txt) for details.

Third-party dependency license texts are included in
[`licenses/third_party`](licenses/third_party), with an index in
[`licenses/third_party/README.txt`](licenses/third_party/README.txt).

## 👏 Credits

Original SBaGen was developed by Jim Peters. See [SBaGen project](https://uazu.net/sbagen/).

ALSA support is based from this [patch](https://github.com/jave/sbagen-alsa/blob/master/sbagen.c).
