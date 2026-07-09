# Alpine Docker cross-compile for Windows

This builds a Windows `openFPGALoader.exe` from Linux using an Alpine-based Docker image.
The image installs Alpine's MinGW-w64 compiler and builds the Windows target dependencies
`zlib`, `libusb`, `hidapi`, and `libftdi1` into `/opt/x86_64-w64-mingw32`.

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

The package contains the install tree plus any non-system DLLs imported by the EXE. The wrapper also requests static linking for the GCC/libstdc++ runtime so the Windows package is as self-contained as possible.

## Clean rebuild

```bash
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross /src/scripts/docker-cross-windows.sh --clean
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
