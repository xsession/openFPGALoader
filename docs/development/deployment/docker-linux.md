# Docker Linux deployment

This builds a native Linux `openFPGALoader` binary inside a container,
packages the install tree as a tarball, and builds distro installer
packages from the same staged install tree. Three services are available:

- `linux-deploy` — Alpine 3.24.2 (musl). Portable tarball; the binary runs
  on musl systems.
- `linux-deploy-ubuntu` — Ubuntu 24.04 (glibc). Builds the Ubuntu/Debian
  `.deb` installer (the binary links the system glibc and libraries, so it
  targets Ubuntu 24.04 and newer).
- `linux-deploy-arch` — Arch Linux (glibc, rolling). Builds the Arch
  `.pkg.tar.zst` installer; the binary runs on Arch and other glibc distros
  with an equal or newer glibc.

## Build the images

```bash
docker compose -f docker-compose.deploy-linux.yml build
```

## Build and package openFPGALoader

```bash
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy-ubuntu
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy-arch
```

Artifacts are written to:

```text
dist/docker-linux/        (Alpine/musl,  openFPGALoader-linux-*)
dist/docker-linux-ubuntu/ (Ubuntu 24.04, openFPGALoader-ubuntu-24.04-*)
dist/docker-linux-arch/   (Arch/glibc,   openFPGALoader-arch-linux-*)
```

| Service | Artifacts |
|---|---|
| `linux-deploy` | `openFPGALoader-linux-<arch>-<ver>.tar.gz` |
| `linux-deploy-ubuntu` | tar.gz + `openfpgaloader_<ver>_amd64.deb` (Ubuntu/Debian installer) |
| `linux-deploy-arch` | tar.gz + `openfpgaloader-<ver>-1-x86_64.pkg.tar.zst` (Arch installer) |

The `.deb` and `.pkg.tar.zst` are built from the same staged install tree
as the tarball (executable, `share/openFPGALoader` assets, udev rules,
license). The Debian `Depends` (libusb, libftdi1, libhidapi, zlib) and the
Arch `depends` (libusb, libftdi1, hidapi, zlib) declare the runtime
libraries the glibc binaries link against.

Install on Ubuntu/Debian: `sudo dpkg -i openfpgaloader_*.deb`
Install on Arch: `sudo pacman -U openfpgaloader-*.pkg.tar.zst`
Both packages place the udev rules under `/usr/lib/udev/rules.d/`.

The Arch service also runs an in-container smoke test (`SMOKE_TEST=1`):
`--version`, `--help`, `--list-boards`, `--list-cables`.

The package contains:

- `bin/openFPGALoader`
- installed `spiOverJtag` / `bpiOverJtag` assets
- Spartan-6 ISE `.cor` bridge files
- Xilinx USB firmware `.hex` files when present in `deploy/ise_programmer_bins/`
- Linux udev rules under `lib/udev/rules.d/`
- a build manifest with `file` and `ldd` output

## Clean rebuild

```bash
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy /src/deploy/scripts/docker-linux-deploy.sh --clean
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy-arch /src/deploy/scripts/docker-linux-deploy.sh --clean
```

## Pass extra CMake flags

```bash
CMAKE_EXTRA_ARGS="-DENABLE_CMSISDAP=OFF -DENABLE_LIBGPIOD=OFF" \
  docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy
```

## Notes

- The default image is Alpine 3.24.2 (musl); the Arch service uses the
  official `archlinux:latest` image (glibc, weekly build; `pacman -Syu` in the
  Dockerfile refreshes it to the current rolling state).
- The resulting binary is dynamically linked against the packages installed
  in the Docker image. Check
  `share/doc/openFPGALoader/docker-linux-build-manifest.txt` in the package for
  the exact runtime libraries.
- The Arch package is a deploy/package helper, not a distro package. Because
  Arch is rolling, a build made today requires a glibc at least as new as the
  image at build time; rebuild the image to refresh the toolchain.
- Install udev rules on the target machine if USB probes are not accessible
  to normal users.

The Compose services use a single repository bind mount. Build and package output
remains under `/src/dist/docker-linux` (Alpine) or `/src/dist/docker-linux-arch`
(Arch), avoiding nested host bind mounts.
