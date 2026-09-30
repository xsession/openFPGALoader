# Alpine Docker cross-compile for Windows

This builds a Windows `openFPGALoader.exe` from Linux using an Alpine-based Docker image.
The image installs Alpine's MinGW-w64 compiler and builds the Windows target dependencies
`zlib`, `libusb`, `hidapi`, and `libftdi1` into `/opt/x86_64-w64-mingw32`.

## Windows host prerequisite

This is a **Linux-container** build. It cannot run while Docker Desktop is in
Windows-container mode because `alpine:edge` has no Windows image manifest.

Use Docker Desktop with the WSL2 backend and Linux containers. From PowerShell,
verify the engine before building:

```powershell
.\deploy\scripts\verify-docker-linux-engine.ps1
```

The expected output starts with:

```text
Docker engine: linux/amd64
```

If the check reports `windows/amd64`, enable **Use the WSL 2 based engine** in
Docker Desktop Settings > General, then choose **Switch to Linux containers**
from the Docker Desktop tray menu. The Compose service also declares
`platform: linux/amd64` so an incompatible engine fails early with a clear
platform error.

## Build

```bash
docker compose -f docker-compose.cross-windows.yml build
```

## Build and package openFPGALoader

```bash
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
```

Artifacts are written to:

```text
dist/docker-windows/
```

### Baking the XPCU driver archive into the image

The `externals/xilinx-usb-driver` and `externals/libwdi` submodules are not
required to build the portable package, but if they are initialized and their
driver archive is built first:

```bash
git submodule update --init --recursive
bash externals/xilinx-usb-driver/docker-build.sh
```

a second Compose service can bake the resulting
`externals/xilinx-usb-driver/dist/xilinx-platform-cable-windows.zip` into the
image (Dockerfile stage `cross-with-driver`, stored at `/opt/xpcu`):

```bash
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross-driver
```

The build container then copies that archive into `dist/docker-windows/` next
to the portable ZIP, so a downstream
`deploy/scripts/build-windows-installer.sh` run can consume it with
`XPCU_DRIVER_ARCHIVE` pointing at `dist/docker-windows/xilinx-platform-cable-windows.zip`
even on a machine without the submodules checked out. The plain
`windows-cross` service is unchanged and does not require the archive to
exist; it targets the base `alpine-cross` image stage.

The package contains the install tree plus any non-system DLLs imported by the EXE. The wrapper also requests static linking for the GCC/libstdc++ runtime so the Windows package is as self-contained as possible.

The `.sha256` file records the archive under its bare file name (relative
path), so it can be verified from any directory with:

```bash
sha256sum -c dist/docker-windows/openFPGALoader-windows-x86_64-*.zip.sha256
```

## Host path overrides (`CROSS_HOST_SRC`, `CROSS_HOST_OUT`)

The Compose service bind-mounts the repository into the container at `/src`
and the package output directory at `/out`. The host-side source of those
mounts is normally `.` (the repository) and `./dist/docker-windows`, but it
can be overridden with two environment variables:

```bash
CROSS_HOST_SRC="$PWD" \
CROSS_HOST_OUT="$PWD/dist/docker-windows" \
  docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
```

This override exists for one specific failure mode: running the build with
[`act`](https://github.com/nektos/act) on Docker Desktop for Windows. Inside
the act job container the workspace lives at `/mnt/<drive>/...`, but the
Docker daemon runs in the WSL2 virtual machine, where the host drive is
mounted at `/host_mnt/<drive>/...`. The daemon cannot see `/mnt/<drive>`, so
it silently creates an empty directory and the container fails with:

```text
exec: "/src/deploy/scripts/docker-cross-windows.sh": stat /src/deploy/scripts/docker-cross-windows.sh: no such file or directory
```

On GitHub Actions runners (Linux) the workspace never looks like
`/mnt/<drive>/...`, so the defaults are correct there. The
`windows-cross` job in `.github/workflows/build-binaries.yml` already handles
the act case automatically: it detects a `/mnt/<drive>` workspace, probes the
remapped `/host_mnt/<drive>` path through the Docker socket, and exports
`CROSS_HOST_SRC`/`CROSS_HOST_OUT` to it when the probe succeeds. When running
`act` manually, no extra environment variables are needed. To set the paths
by hand, for example when the repository is checked out on a different drive:

```bash
CROSS_HOST_SRC=/host_mnt/d/openFPGALoader \
CROSS_HOST_OUT=/host_mnt/d/openFPGALoader/dist/docker-windows \
  docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
```

## Clean rebuild

```bash
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross /src/deploy/scripts/docker-cross-windows.sh --clean
```

## Pass extra CMake flags

```bash
CMAKE_EXTRA_ARGS="-DENABLE_CMSISDAP=OFF -DENABLE_FTDIPP=OFF -DENABLE_UDEV=OFF -DENABLE_LIBGPIOD=OFF" \
  docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
```

## Notes

- This is for `x86_64-w64-mingw32` Windows binaries.
- The Alpine image uses the `edge` package branch because current Alpine packaging exposes the MinGW-w64 compiler there.
- Runtime tests cannot execute the Windows EXE inside this container unless Wine is added. The wrapper only checks that the EXE exists and then packages it.

### Notes

The Dockerfile downloads zlib from the official GitHub release tarball first and falls back to `zlib.net/fossils`. Do not use `https://zlib.net/zlib-<version>.tar.gz`; zlib moves older point releases out of the web root after newer releases, which causes 404 failures.

The compose file sets `pull_policy: build` so Docker Compose does not try to pull the local image name from Docker Hub before building it.

## Notes on dependency builds

The Alpine image intentionally builds `libusb` from the upstream release tarball with
`./configure --host=x86_64-w64-mingw32` instead of CMake. The GitHub source archive for
libusb does not contain a root `CMakeLists.txt`, while the official release tarball
contains the generated configure script needed for cross-compilation.

## Console output on Windows

The Docker wrapper forces the final Windows executable to use the Windows console/CUI subsystem:

```text
-static -static-libgcc -static-libstdc++ -Wl,--subsystem,console
```

This is required because `openFPGALoader` is a CLI program. If the executable is linked as a Windows GUI subsystem app, it may run from PowerShell/cmd.exe but `--help`, `--version`, and errors appear silent because stdout/stderr are detached. The wrapper verifies the PE subsystem with `x86_64-w64-mingw32-objdump -p` and fails the package step if it sees `Windows GUI`.

To rebuild after changing linker flags, use `--clean` or remove `build-docker-windows`, because CMake caches linker flags in the build directory.



## Silent executable troubleshooting

If `openFPGALoader.exe --help` opens and exits with no visible output, check these two things first:

```powershell
dumpbin /headers .\openFPGALoader.exe | findstr /i subsystem
```

Expected: `Windows CUI`. If it says `Windows GUI`, rebuild with `--clean`.

Then check imported DLLs from inside Docker:

```bash
x86_64-w64-mingw32-objdump -p /src/dist/docker-windows/install/bin/openFPGALoader.exe | grep "DLL Name"
```

The wrapper copies all non-system DLL imports into `dist/docker-windows/install/bin`. Missing runtime DLLs can make a Windows command-line program appear silent because it exits before `main()` runs.
