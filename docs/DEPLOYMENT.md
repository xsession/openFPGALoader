# Multi-platform binary build and release

## Local Linux/macOS

```bash
sudo apt-get install cmake pkg-config g++ gzip libftdi1-dev libhidapi-dev libudev-dev zlib1g-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix "$PWD/dist/local"
```

Pass extra CMake switches after `--`:

```bash
cmake -S . -B build -G Ninja \
  -DENABLE_CMSISDAP=OFF -DENABLE_LIBGPIOD=OFF
cmake --build build --parallel
```

## Docker Linux deployment

```bash
docker compose -f docker-compose.deploy-linux.yml build
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy
```

Artifacts are written to:

```text
dist/docker-linux/
```

See `DOCKER_DEPLOY_LINUX.md` for clean rebuilds, extra CMake flags, and package
contents.

The GitHub Actions Linux jobs also create Debian installers from the same
staged install tree. The local equivalent is:

```bash
cmake --install build --prefix "$PWD/pkg/usr/local"
INSTALL_DIR="$PWD/pkg/usr/local" \
  DIST_DIR="$PWD/dist" \
  PACKAGE_NAME="openFPGALoader-local" \
  bash deploy/scripts/build-linux-deb.sh
```

The resulting `.deb` and `.sha256` files are placed in `dist/`. The package
script derives its version from `openFPGALoader --version`, unless
`OPENFPGALOADER_PACKAGE_VERSION` or `VERSION` is supplied.

## Local Windows

Use MSYS2 UCRT64 or MINGW64 and install the matching packages:

```bash
pacman -S --needed git base-devel mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-cc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-libusb mingw-w64-ucrt-x86_64-libftdi mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-zlib
cmake -S . -B build -G Ninja -DENABLE_UDEV=OFF -DENABLE_LIBGPIOD=OFF
cmake --build build --parallel
```

Or from PowerShell when CMake and a compiler are already installed:

```powershell
cmake -S . -B build -G Ninja -DENABLE_UDEV=OFF -DENABLE_LIBGPIOD=OFF
cmake --build build --parallel
```

## Local Windows cross-compile from Linux

```bash
sudo apt-get install mingw-w64 libz-mingw-w64-dev cmake ninja-build pkg-config p7zip-full
cmake -S . -B build-win64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-x86_64-w64-mingw32.cmake \
  -DWINDOWS_CROSSCOMPILE=ON -DCROSS_COMPILE_DEPS=ON \
  -DENABLE_CMSISDAP=OFF -DENABLE_UDEV=OFF -DENABLE_LIBGPIOD=OFF
cmake --build build-win64 --parallel
```

For the reproducible Docker path used by the binary workflow, use the Compose
service instead. Docker Desktop must use Linux containers/WSL2 on Windows:

```bash
docker compose -f docker-compose.cross-windows.yml build windows-cross
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
```

## Windows installer and external driver dependencies

The portable Windows package does not contain the generated XPCU driver
installer. The external submodules build that input explicitly:

```bash
git submodule update --init --recursive
bash externals/xilinx-usb-driver/docker-build.sh
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
bash deploy/scripts/build-windows-installer.sh
```

The Xilinx driver build first compiles `externals/libwdi` for x86 and x64, then
packages the `xilinx-usb-driver` PowerShell installer and `wdi-simple.exe`
helpers. The Inno Setup package embeds that driver kit and invokes its elevated
installer only when the user selects the driver task. This keeps the portable
ZIP useful on its own while making the full Windows installer complete.

The script accepts these overrides when the artifacts are staged elsewhere:

```bash
DIST_DIR="$PWD/dist/docker-windows" \
INSTALL_DIR="$PWD/dist/docker-windows/install" \
XPCU_DRIVER_ARCHIVE="$PWD/externals/xilinx-usb-driver/dist/xilinx-platform-cable-windows.zip" \
  bash deploy/scripts/build-windows-installer.sh
```

## GitHub Actions release

The workflow builds:

- Ubuntu 22.04 and 24.04 tarballs with udev rules and Debian `.deb` installers.
- macOS Intel and Apple Silicon runner tarballs.
- Windows native MSYS2 UCRT64/MINGW64 portable zip files.
- Windows x86_64 cross-compiled zip from Ubuntu using the Linux-container
  Compose build.
- Windows XPCU driver package from the `libwdi` and `xilinx-usb-driver`
  submodules.
- Windows Inno Setup installer containing the cross-built executable,
  transport data, and optional XPCU driver kit.

A release is published when:

- a tag like `v1.1.1-custom` is pushed, or
- the workflow is run manually with `release_tag` set.

The separate `Deploy MkDocs documentation` workflow builds the same strict
MkDocs site and publishes the generated `site/` directory to the `gh-pages`
branch on pushes to `master` or `main`.
