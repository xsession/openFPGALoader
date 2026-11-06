# Local Windows release CI with `act`

The `windows-cross` job is self-contained: it builds the Xilinx Platform Cable USB
driver package, cross-compiles openFPGALoader for Windows, embeds the driver ZIP in
the portable package, and verifies the final ZIP.

## Prerequisites

- Docker Desktop using the Linux/WSL2 engine.
- `act`.
- Both submodules populated: `git submodule update --init --recursive`.
- Shell scripts checked out with LF line endings.

## Run

```powershell
act -j windows-cross `
  -P ubuntu-24.04=ghcr.io/catthehacker/ubuntu:act-latest `
  --container-architecture linux/amd64 `
  -vv
```

The job performs these steps:

1. Build libwdi for x86 and x64 and create `xilinx-platform-cable-windows.zip`.
2. Verify the driver archive checksum.
3. Resolve Docker Desktop daemon-visible bind paths when the runner workspace is
   under `/mnt/<drive>/...`.
4. Build the Alpine/MinGW cross image.
5. Build openFPGALoader and embed the driver archive at
   `share/openFPGALoader/drivers/`.
6. Copy the package back into the act workspace when host-path remapping is used.
7. Verify that the portable ZIP contains both the driver ZIP and its checksum.

The `windows-installer` job consumes only the portable Windows artifact. After
extracting it, the job verifies the embedded XPCU archive again and builds the Inno
Setup installer from the same driver bytes.

## Docker Desktop path mapping

`act`'s Linux job container can report a workspace such as
`/mnt/d/openFPGALoader`, while the outer Docker Desktop daemon sees the actual
checkout as `/host_mnt/d/openFPGALoader` (or the
`/run/desktop/mnt/host/...` equivalent). Runtime bind mounts are probed and remapped
automatically by the XPCU and Windows cross scripts.

If the XPCU services fail with exit 127 immediately after container creation, check
that the log prints a daemon-visible repository path and that both submodules exist
in the Windows host checkout.
