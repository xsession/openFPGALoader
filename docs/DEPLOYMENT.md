# Multi-platform binary build and release

## Local Linux/macOS

```bash
sudo apt-get install cmake pkg-config g++ gzip libftdi1-dev libhidapi-dev libudev-dev zlib1g-dev
./scripts/build.sh --ninja --package
```

Pass extra CMake switches after `--`:

```bash
./scripts/build.sh --package -- -DENABLE_CMSISDAP=OFF -DENABLE_LIBGPIOD=OFF
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

## Local Windows

Use MSYS2 UCRT64 or MINGW64 and install the matching packages:

```bash
pacman -S --needed git base-devel mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-cc mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-libusb mingw-w64-ucrt-x86_64-libftdi mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-zlib
./scripts/build.sh --ninja --package -- -DENABLE_UDEV=OFF -DENABLE_LIBGPIOD=OFF
```

Or from PowerShell when CMake and a compiler are already installed:

```powershell
.\scripts\build.ps1 -Package -Ninja
```

## Local Windows cross-compile from Linux

```bash
sudo apt-get install mingw-w64 libz-mingw-w64-dev cmake ninja-build pkg-config p7zip-full
./scripts/build.sh --windows-cross --ninja --package
```

## GitHub Actions release

The workflow builds:

- Ubuntu 22.04 and 24.04 tarballs with udev rules.
- macOS Intel and Apple Silicon runner tarballs.
- Windows native MSYS2 UCRT64/MINGW64 portable zip files.
- Windows x86_64 cross-compiled zip from Ubuntu.

A release is published when:

- a tag like `v1.1.1-custom` is pushed, or
- the workflow is run manually with `release_tag` set.
