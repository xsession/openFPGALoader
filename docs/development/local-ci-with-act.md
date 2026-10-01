# Building release artifacts locally with nektos/act

This document explains how to reproduce the `Build multi-platform binaries`  
workflow (`.github/workflows/build-binaries.yml`) and the  
`Deploy MkDocs documentation` workflow (`.github/workflows/deploy-docs.yml`)  
locally with [nektos/act](https://github.com/nektos/act), and which artifacts  
each invocation produces.

`act` executes the workflow YAML inside Linux Docker containers, mapping each  
`runs-on:` label to a community runner image (`catthehacker/ubuntu:22.04`,  
`catthehacker/ubuntu:24.04`, ...). It is not a CI replacement for hardware  
tests; it reproduces the packaging pipeline.

## Job matrix: what act can and cannot run

| Job | `runs-on` | Runnable with act | Why |
| --- | --- | --- | --- |
| `linux` (22.04 / 24.04 matrix) | `ubuntu-22.04`, `ubuntu-24.04` | Yes | Plain apt + CMake build, no nested Docker. |
| `macos` | `macos-14` | No | act has no usable macOS runner on a Windows/WSL host. |
| `windows-msys2` | `windows-latest` | No | act does not support Windows container runners. |
| `windows-cross` | `ubuntu-24.04` | Yes, with DinD | Runs `docker compose` inside the container. |
| `windows-xpcu-drivers` | `ubuntu-24.04` | Yes, with DinD | Runs `externals/xilinx-usb-driver/docker-build.sh` (compose `--build`) inside the container. |
| `windows-installer` | `ubuntu-24.04` | Yes | Consumes local artifacts produced by the two jobs above. |
| `release` | `ubuntu-latest` | Skip | `softprops/action-gh-release` needs a real `GITHUB_TOKEN` and only makes sense on tag/manual dispatch. |

## Prerequisites

`act` and Docker Desktop (with the WSL2 backend).

**Docker Desktop must use the Linux container engine.** act only runs  
Linux containers, and the `windows-cross` job builds an Alpine image, so it  
additionally cannot run in Windows-container mode (see  
[DOCKER\_CROSS\_WINDOWS.md](../DOCKER_CROSS_WINDOWS.md)).

Submodules populated locally. act's checkout reuses the local repository  
but does **not** initialize submodules, so the `windows-xpcu-drivers` job  
depends on them already being present:

Optional: a `.actrc` file in the repository root for defaults.

`dind: true` gives each job container a Docker daemon (sidecar) and the  
Docker CLI, which `windows-cross` and `windows-xpcu-drivers` require.  
It is harmless for the `linux` job.

## Version stamping

CMake resolves the version from the nearest `v*` Git tag  
(`git describe --match "v*"`), falling back to a static version when `.git`  
is unavailable; `-DOPENFPGALOADER_VERSION=...` always wins. Package file names  
are derived from the built executable, so:

*   make sure the expected tag exists locally (`git tag -l "v*"`), or
*   create a temporary tag before running, or
*   for the `.deb`, override with `OPENFPGALOADER_PACKAGE_VERSION`.

## Linux tarball + Debian package

```
act -j linux
```

This runs both matrix legs (`ubuntu-22.04`, `ubuntu-24.04`) and produces,  
per leg:

*   `openFPGALoader-ubuntu-<ver>-x86_64.tar.gz` (+ `.sha256`)
*   `openFPGALoader-ubuntu-<ver>-x86_64_<version>_<arch>.deb` (+ `.sha256`)

Artifacts are written by act to `_temp/Build multi-platform binaries/linux/`  
in the repository root (the same files `actions/upload-artifact` would have  
uploaded). The staging tree is also visible under the container's `pkg/`  
during the run; the final deliverables are the uploaded artifact files.

Useful variants:

```
# single matrix leg only (faster iteration)
act -j linux -m name=ubuntu-24.04-x86_64

# keep the job container logs on failure (default), or attach a shell
act -j linux --reuse

# force a version stamp
act -j linux -e .act-event-push.json   # see notes on events below
```

If you want a different version in the package names without tagging:

```
git tag v1.1.7-local   # temporary local tag, remove afterwards
act -j linux
```

## Windows cross-compile ZIP (needs DinD)

```
act -j windows-cross -P ubuntu-24.04=ghcr.io/catthehacker/ubuntu:act-latest --container-architecture linux/amd64
```

Inside the `ubuntu-24.04` job container, act (with `dind: true`) provides a  
Docker daemon; the job then runs:

1.  `docker compose -f docker-compose.cross-windows.yml build windows-cross`  
    (Alpine + MinGW-w64 image, `openfpgaloader-windows-cross:alpine`)
2.  `docker compose ... run --rm windows-cross` (builds and packages)

Output:

*   `openFPGALoader-windows-cross-x86_64.zip` (+ `.sha256`) under  
    `_temp/Build multi-platform binaries/windows-cross/`
*   the package contents also appear in `dist/docker-windows/` in the working  
    tree, because the compose file bind-mounts `./dist/docker-windows:/out`

The first run is slow: the DinD daemon pulls and builds the Alpine MinGW  
image from scratch. Re-running `act` does not reuse that image (it lives in  
the sidecar daemon), so budget a few minutes per run.

## Xilinx Platform Cable USB driver package (needs DinD)

```
act -j windows-xpcu-drivers
```

Runs `externals/xilinx-usb-driver/docker-build.sh` inside the job container,  
which compose-builds the driver packaging image and produces:

*   `externals/xilinx-usb-driver/dist/xilinx-platform-cable-windows.zip`  
    (+ `.sha256`), also copied to  
    `_temp/Build multi-platform binaries/windows-xpcu-drivers/`

Requires the `externals/libwdi` and `externals/xilinx-usb-driver` submodules  
to be checked out locally (see prerequisites).

## Windows Inno Setup installer (needs the two jobs above)

`windows-installer` has `needs: [windows-cross, windows-xpcu-drivers]` and  
pulls both packages through `actions/download-artifact`. act resolves those  
from the local `_temp` artifact store, so run the producers first:

```
act -j windows-cross
act -j windows-xpcu-drivers
act -j windows-installer
```

Output:

*   `openFPGALoader-<version>-win64-setup.exe` (+ `.sha256`) under  
    `_temp/Build multi-platform binaries/windows-installer/`
*   staged inputs land in `dist/docker-windows/install/` in the working tree

Verify the ZIP contents as CI does:

```
unzip -l _temp/Build multi-platform binaries/windows-cross/openFPGALoader-windows-cross-x86_64.zip
# expect: bin/openFPGALoader.exe, runtime DLLs, share/openFPGALoader/ (SOJ/BPI data)
```

## MkDocs site (deploy-docs workflow)

```
act -W .github/workflows/deploy-docs.yml -j deploy
```

The job builds the site with `mkdocs build --strict`; the final  
`peaceiris/actions-gh-pages` step fails locally without a `GITHUB_TOKEN`  
(unless run with `--dryrun`), which is expected — the site build steps are  
the useful part.

For a pure local site build (no act needed):

```
pip install -r docs/requirements-mkdocs.txt
python docs/generate_compatibility.py
mkdocs build --strict --site-dir site
mkdocs serve   # live preview at http://127.0.0.1:8000
```

## Passing secrets and events

Secrets: `act -j linux -P GITHUB_TOKEN=ghp_...` or a `--secret-file`  
listing `NAME=value` lines. The `linux` job does not require any secret.

Event file: act simulates a `push` event by default, which is the right  
trigger for this workflow. To simulate a tag push (release naming):

## Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `act` errors about unknown runner / image pull failures | Docker Desktop is in Windows-container mode; switch to Linux containers (WSL2 engine). |
| `docker: command not found` or `Cannot connect to the Docker daemon` inside `windows-cross` / `windows-xpcu-drivers` | Missing `dind: true` in `.actrc` (or pass it per run). |
| `xilinx-usb-driver` build fails with empty source | Submodules not initialized locally; run `git submodule update --init --recursive`. |
| Package names show an unexpected version | CMake picks the nearest `v*` tag; tag locally or set `OPENFPGALOADER_PACKAGE_VERSION` (deb) / `OPENFPGALOADER_VERSION` (CMake). |
| `windows-installer` cannot download artifacts | Run `windows-cross` and `windows-xpcu-drivers` first; act's `download-artifact` reads the local `_temp` store. |
| `release` job 401/forbidden | Expected locally; publishing needs a real token and a tag or `workflow_dispatch` with `release_tag`. Skip the job. |
| macOS / Windows-MSYS2 artifacts missing | Those runners are not reproducible with act; rely on the GitHub Actions matrix or a native MSYS2 build (see DEPLOYMENT.md). |

## Expected artifact set for a full local tag build

| Artifact | Produced by |
| --- | --- |
| `openFPGALoader-ubuntu-22.04-x86_64.tar.gz` (+ sha256) | `linux` |
| `openFPGALoader-ubuntu-24.04-x86_64.tar.gz` (+ sha256) | `linux` |
| `openFPGALoader-ubuntu-*-...deb` (+ sha256) | `linux` |
| `openFPGALoader-windows-cross-x86_64.zip` (+ sha256) | `windows-cross` |
| `xilinx-platform-cable-windows.zip` (+ sha256) | `windows-xpcu-drivers` |
| `openFPGALoader-<ver>-win64-setup.exe` (+ sha256) | `windows-installer` |
| `site/` (MkDocs) | `deploy-docs` / local mkdocs |

Not reproducible with act on this host: macOS tarballs and the native  
Windows MSYS2 (UCRT64/MINGW64) ZIPs — those require the corresponding  
GitHub-hosted runners.

```
act -j linux --eventname tag   # uses the current HEAD as the tag
```

```
# .actrc
dind: true
```

```
git submodule update --init --recursive
```

```
# verify
docker info --format '{{.OSType}}'        # must print: linux
# if it prints "windows", switch the engine in Docker Desktop
# (Settings -> General -> "Use WSL 2 based engine" / pick Linux containers),
# or:
docker context use desktop-linux
```