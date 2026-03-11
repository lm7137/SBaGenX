# SBaGenX - Sequenced Brainwave Generator

SBaGenX is a command-line brainwave generator for creating binaural beats, monaural beats, isochronic tones, and programmable mix-based entrainment effects. It is designed for precise, scriptable session design, high-quality offline rendering, and advanced audio experimentation.

**Project website:** https://www.sbagenx.com

> **SBaGenX is a fork of SBaGen+, continuing development of the original SBaGen lineage.**
> Full credit is due, first and foremost, to the father of SBaGen, Jim Peters, and also to the creator of the SBaGen+ fork, Ruan Klein, who added isochronic beats as well as making numerous enhancements.

> Use the latest stable release from the [GitHub releases page](https://github.com/lm7137/SBaGenX/releases).  
> Current stable release: **v3.2.0**

## Table of Contents

- [About This Project](#about-this-project)
- [What SBaGenX Adds](#what-sbagenx-adds)
- [Installation](#installation)
  - [Try SBaGenX in 60 Seconds](#try-sbagenx-in-60-seconds)
  - [Using Docker for Builds](#using-docker-for-builds)
  - [Download Pre-built Binaries](#download-pre-built-binaries)
  - [Installing on Linux](#installing-on-linux)
  - [Installing on macOS](#installing-on-macos)
  - [Installing on Windows](#installing-on-windows)
- [Basic Usage](#basic-usage)
- [Documentation](#documentation)
- [Library API](#library-api)
- [Research](#research)
- [Compilation](#compilation)
  - [Build Scripts Structure](#build-scripts-structure)
  - [Building with Docker](#option-1-using-docker-compose-simplest-method)
  - [Building Natively](#option-2-building-natively)
- [License](#license)
- [Credits](#credits)

## About This Project

SBaGenX is a fork of SBaGen+, created by Ruan Klein, which is itself a fork of the original
SBaGen (Sequenced Binaural Beat Generator) created by Jim Peters. The
original project has not been maintained for many years, and SBaGenX
aims to keep the lineage functional on modern systems while preserving
its core sequencing model and command-line workflow.

The project now goes beyond maintenance work. In addition to practical
feature additions, SBaGenX has extracted the core runtime into
`sbagenxlib`, a reusable shared library with a documented API, modern
sampling/export helpers, and a cleaner foundation for future front-ends
and tooling.

The name was changed from **"Sequenced Binaural Beat Generator"** to **"Sequenced Brainwave Generator"** in the SBaGen+ fork to better reflect its expanded functionality. Since SBaGen+ added support for isochronic tones in addition to binaural beats, and SBaGenX added monaural beat support for the built-in programs, the original name no longer fully represented its capabilities.

## What SBaGenX Adds

This fork introduces substantial functional changes beyond maintenance:

1. **Built-in programs now use function-driven runtime values**  
   `drop`, `sigmoid`, and `curve` sessions evaluate beat/pulse values at
   runtime rather than relying only on coarse piecewise approximations.

2. **More expressive built-in program syntax**  
   Added:
   - `-p sigmoid` for smoother beat/pulse transitions
   - configurable sigmoid parameters `:l=<value>:h=<value>`
   - built-in monaural mode via `M`
   - signed and fractional target levels in program specs

3. **Custom function sessions from `.sbgf` files (`-p curve`)**  
   SBaGenX supports external function files with:
   - helper functions implemented in C
   - cleaner piecewise syntax such as `beat<p1 = ...`
   - `solve` equations for automatically solving unknown constants
   - runtime control of `beat`, `carrier`, `amp`, `mixamp`, and matching
     mix-effect parameters

4. **Mix-stream processing has expanded substantially**  
   Added and extended:
   - `mixpulse`
   - `mixbeat` (Hilbert-based embedded beat in the mix stream)
   - `mixam` for true multiplicative amplitude modulation of broadband mix audio
   - parameterized mix amplitude modulation via `-A`
   - curve-driven runtime control of `mixspin`, `mixpulse`, `mixbeat`, and `mixam`

5. **Isochronic and mix-envelope customization**  
   Added:
   - `-I` for one-cycle isochronic envelope control
   - `-H` for one-cycle `mixam` envelope control
   - PNG preview rendering of these cycle-level envelopes

6. **Preview and plotting tools**  
   Added PNG plotting for:
   - built-in `drop`, `sigmoid`, and `curve` beat/pulse graphs
   - one-cycle isochronic envelope and waveform views
   - one-cycle `mixam` envelope/gain views
   Windows builds bundle Python+Cairo for higher-quality anti-aliased plots.

7. **Expanded input/output format support**  
   Added:
   - FLAC input mix support with `SBAGEN_LOOPER` metadata handling
   - `SBAGEN_LOOPER` intro extension via `i `
   - native output encoding selected by filename extension for OGG/FLAC/MP3

8. **Modernized export quality on the `sbagenxlib` runtime path**  
   Added:
   - TPDF-dithered 16-bit PCM conversion
   - 24-bit FLAC export
   - float-path OGG/Vorbis export
   - float-input MP3 export where the LAME runtime supports it
   - 24-bit raw/WAV export via `-b 24`

9. **`sbagenxlib` shared library**  
   The core runtime is now available as a reusable library with:
   - public headers
   - shared/static libraries
   - pkg-config metadata
   - `.sbgf` loading and evaluation APIs
   - context/sampling helpers for future front-ends and tooling

10. **Shipped reference material**  
    The installer now includes curated `.sbgf` examples and example plots
    in the `Plots` directory so users can inspect curve styles and solved
    function forms immediately after installation.

## Installation

Download assets from the [GitHub releases page](https://github.com/lm7137/SBaGenX/releases).

Current stable release: **v3.2.0**

### Try SBaGenX in 60 Seconds

#### Windows

1. Download and install the current Windows installer:

   - [`sbagenx-windows-setup.exe`](https://github.com/lm7137/SBaGenX/releases/download/v3.2.0/sbagenx-windows-setup.exe)

2. Verify installation:

   ```bash
   sbagenx -h
   ```

3. Run a quick built-in session:

   ```bash
   sbagenx -m river1.ogg -p drop 00ds+ mix/99
   ```

#### Ubuntu (amd64)

```bash
wget -O sbagenx_3.2.0-1_amd64.deb \
  https://github.com/lm7137/SBaGenX/releases/download/v3.2.0/sbagenx_3.2.0-1_amd64.deb
sudo apt install ./sbagenx_3.2.0-1_amd64.deb
sbagenx -h
```

#### Plot Example (No Audio Output)

```bash
sbagenx -P -p sigmoid t30,30,0 00ls+:l=0.2:h=0
```

This writes a PNG curve to the current directory.

#### Custom Curve Example (`.sbgf`)

```bash
sbagenx -p curve examples/basics/curve-sigmoid-like.sbgf 00ls:l=0.2:h=0
```

#### Solved Curve Example (`.sbgf` with `solve`)

```bash
sbagenx -G -p curve examples/basics/curve-expfit-solve-demo.sbgf 00ls:l=0.2 mix/99
```

### Using Docker for Builds

This repository includes Docker support for **build workflows** (Linux/Windows artifacts), via `Dockerfile` and `compose.yml`.

Use:

```bash
# Build Linux + Windows artifacts
docker compose up build

# Build Linux ARM64 artifacts
docker compose up build-arm64
```

At present, there is no officially published SBaGenX runtime container image for end-user playback.

### Download Pre-built Binaries

Recommended download path:

- Use the binaries published on the
  [GitHub releases page](https://github.com/lm7137/SBaGenX/releases)
- The project website at [www.sbagenx.com](https://www.sbagenx.com/) provides
  release overviews and links back to the same public assets

Current stable release assets (`v3.2.0`):

- Windows installer: [`sbagenx-windows-setup.exe`](https://github.com/lm7137/SBaGenX/releases/download/v3.2.0/sbagenx-windows-setup.exe)
- Windows SHA256: `f06f463381945acef11b2f11aed51694c2901b54f65347e6b5181d6eb501349b`
- Ubuntu package: [`sbagenx_3.2.0-1_amd64.deb`](https://github.com/lm7137/SBaGenX/releases/download/v3.2.0/sbagenx_3.2.0-1_amd64.deb)
- Ubuntu SHA256: `8a3bbd120f199174d5506fd7696a99ee3794a0c751a76b3579e0b77a282e0e39`

  **Important**: Always verify the SHA256 checksum of downloaded binaries
  against those published with the release.

### Installing on Linux

Pre-built Ubuntu (`amd64`) packages are published on releases.
For other Linux distributions/architectures, build from source:

```bash
bash linux-build-libs.sh
bash linux-build-sbagenx.sh
```

You can either install the produced binary directly:

```bash
sudo cp dist/sbagenx-linux64 /usr/local/bin/sbagenx
sudo chmod +x /usr/local/bin/sbagenx
sbagenx -h
```

Or build an Ubuntu/Debian package:

```bash
bash linux-create-deb.sh
sudo apt install ./dist/sbagenx_*_amd64.deb
```

### Installing on macOS

Pre-built macOS installer assets are not currently published.
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

### Installing on Windows

1. Download the installer:

   - [`sbagenx-windows-setup.exe`](https://github.com/lm7137/SBaGenX/releases/download/v3.2.0/sbagenx-windows-setup.exe)

2. Verify the SHA256 checksum of the installer. You can use PowerShell or Command Prompt to do this:

   ```powershell
   Get-FileHash -Algorithm SHA256 .\sbagenx-windows-setup.exe
   ```

   The expected SHA256 for `v3.2.0` is:

   ```text
   f06f463381945acef11b2f11aed51694c2901b54f65347e6b5181d6eb501349b
   ```

3. Run the installer and follow the instructions.

**Warning about antivirus on Windows**

Some versions of Windows Defender or other antivirus software may falsely detect `SBaGenX` as a threat.

This happens because the executable is **not digitally signed**, and as a command-line program, it may be flagged as suspicious by default.

`SBaGenX` is an open-source project, and the source code is publicly available in this repository for inspection.

**Temporary solution:** if you trust the source of the executable, add an exception in your antivirus for the file or the folder where `SBaGenX` is installed.

## Basic Usage

See [USAGE.md](USAGE.md) for more information on how to use SBaGenX.

Common entry points:

### Built-in Program Session

```bash
sbagenx -m river1.ogg -p drop 00ds+ mix/99
```

### Broadband Mix Amplitude Modulation

```bash
sbagenx -m river1.ogg -H s=0:d=0.35:a=0.10:r=0.10:e=2 -p sigmoid Nls+:S=2 mix/100 mixam:beat
```

### Curve-Driven Session with Solved Constants

```bash
sbagenx -p curve examples/basics/curve-expfit-solve-demo.sbgf 00ls:l=0.2 mix/99
```

### Plot a Curve Before Running Audio

```bash
sbagenx -G -p curve examples/basics/curve-raised-cosine-demo.sbgf 00ls mix/99
```

### 24-bit Uncompressed Export

```bash
sbagenx -b 24 -Wo out24.wav -L 0:20 -i 200+8/30
```

## Documentation

For detailed information on all features, see the [SBAGENX.txt](docs/SBAGENX.txt) file.

For sbagenxlib migration status and roadmap, see
[docs/SBAGENXLIB.md](docs/SBAGENXLIB.md).

## Library API

For developers integrating `sbagenxlib`:

- API reference: [docs/SBAGENXLIB_API.md](docs/SBAGENXLIB_API.md)
- Quick start: [docs/SBAGENXLIB_QUICKSTART.md](docs/SBAGENXLIB_QUICKSTART.md)
- .NET interop notes: [docs/SBAGENXLIB_DOTNET_INTEROP.md](docs/SBAGENXLIB_DOTNET_INTEROP.md)
- Doxygen generation: [docs/SBAGENXLIB_DOXYGEN.md](docs/SBAGENXLIB_DOXYGEN.md)

The library is no longer just a sidecar experiment. The CLI now relies on
`sbagenxlib` for core runtime generation, modernized export paths, curve
evaluation, and host-facing sampling helpers.

Recent library-side audio output work includes:

- a modernized PCM conversion path for `sbagenxlib`
- TPDF dithering for 16-bit output
- wider host-side conversion helpers for `s16`, `s24`-in-`s32`, and `s32`
- quantitative regression tests that compare dithered and undithered output
- 24-bit FLAC export on the `sbagenxlib`-backed encoder path
- float-path OGG/Vorbis export on the `sbagenxlib`-backed encoder path
- float-input MP3 export where the LAME runtime exposes the float API
- 24-bit raw/WAV export on the `sbagenxlib`-backed uncompressed path

The current tests verify the expected tradeoff: TPDF dithering slightly raises
the noise floor while materially reducing signal-correlated quantization error
and flattening the error spectrum.

## Research

For the scientific background behind SBaGenX, check out [RESEARCH.md](RESEARCH.md).

## Compilation

SBaGenX can be compiled for macOS, Linux and Windows. The build process is divided into two steps:

1. **Building the libraries**: This step is only necessary if you want MP3 and OGG support (FLAC support is built in)
2. **Building the main program**: This step compiles SBaGenX using the libraries built in the previous step

### Build Scripts Structure

- **Library build scripts**:

  - `macos-build-libs.sh`: Builds libraries for macOS (universal binary - ARM64 + x86_64)
  - `linux-build-libs.sh`: Builds libraries for Linux (32-bit, 64-bit, ARM64 [if native])
  - `windows-build-libs.sh`: Builds libraries for Windows using MinGW (cross-compilation)

- **Main program build scripts**:
  - `macos-build-sbagenx.sh`: Builds SBaGenX for macOS (universal binary - ARM64 + x86_64)
  - `linux-build-sbagenx.sh`: Builds SBaGenX for Linux (32-bit, 64-bit, ARM64 [if native])
  - `linux-create-deb.sh`: Creates a Debian package from the Linux build output
  - `windows-build-sbagenx.sh`: Builds SBaGenX for Windows using MinGW (cross-compilation)

#### Option 1: Using Docker Compose (Simplest Method)

The easiest way to build SBaGenX for Linux and Windows is using Docker Compose:

```bash
# Build all Linux and Windows binaries with a single command
docker compose up build

# Build for Linux ARM64
docker compose up build-arm64
```

This will automatically build the Docker image and run all necessary build scripts to generate the binaries for Linux and Windows. All compiled binaries will be placed in the `dist` directory.

**For macOS**, you need compile natively. See next section for more details.

#### Option 2: Building Natively

If you prefer to build without Docker, you can use the build scripts directly on your system, provided you have all the necessary dependencies installed.

You can see the dependencies in the [Dockerfile](Dockerfile). For macOS, you need the Xcode command line tools installed and home brew installed..

The build scripts are:

```
<platform>-build-libs.sh # macOS, Linux, Windows
<platform>-build-sbagenx.sh # macOS, Linux, Windows
linux-create-deb.sh # Linux (.deb package)
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

For high-quality graph rendering (`-P`, or legacy `-G` for sigmoid)
with anti-aliased text,
Windows builds now treat Python+Cairo as a required bundled runtime.
`windows-build-sbagenx.sh` auto-prepares these runtimes if missing, then
bundles them into:
- `dist/python-win32/`
- `dist/python-win64/`

The build aborts if either runtime cannot be prepared.

You can still point the build to custom pre-prepared runtime folders:
- `SBAGENX_WIN32_PY_RUNTIME_DIR`
- `SBAGENX_WIN64_PY_RUNTIME_DIR`

Or provide archives (auto-extracted during `windows-build-sbagenx.sh`):
- `SBAGENX_WIN32_PY_RUNTIME_ARCHIVE` (default: `libs/windows-win32-python-runtime.zip`)
- `SBAGENX_WIN64_PY_RUNTIME_ARCHIVE` (default: `libs/windows-win64-python-runtime.zip`)

Optional download URLs (used if archive file is missing):
- `SBAGENX_WIN32_PY_RUNTIME_URL`
- `SBAGENX_WIN64_PY_RUNTIME_URL`

If no custom runtime/archive is provided, the script auto-downloads:
- CPython embeddable package (`SBAGENX_PY_EMBED_VERSION`, default `3.13.12`)
- matching `pycairo` wheel (`SBAGENX_PYCAIRO_VERSION`, default `1.29.0`)

You can tune behavior with:
- `SBAGENX_REQUIRE_PY_RUNTIME=1|0` (default `1`)
- `SBAGENX_AUTO_PREPARE_PY_RUNTIME=1|0` (default `1`)

Each runtime tree must include at minimum:
- `python.exe`
- standard library / site-packages needed by `scripts/sbagenx_plot.py`
- Python Cairo bindings (`pycairo`)

Provenance notes for these runtime trees are tracked in:
- `libs/windows-python-runtime-SOURCES.txt`

The installer copies the matching architecture runtime to
`{app}\python`, and SBaGenX uses it automatically for plotting.

## License

SBaGenX is distributed under the GPL license. See [COPYING.txt](COPYING.txt) for details.

Third-party dependency license texts are included in
[`licenses/third_party`](licenses/third_party), with an index in
[`licenses/third_party/README.txt`](licenses/third_party/README.txt).

## Credits

Original SBaGen was developed by Jim Peters. See [SBaGen project](https://uazu.net/sbagen/).

The SBaGen+ fork was developed by Ruan Klein. See [SBaGen+ project](https://github.com/ruanklein/sbagen-plus)

ALSA support is based from this [patch](https://github.com/jave/sbagen-alsa/blob/master/sbagen.c).
