# Windows release packaging fixes - 2026-10-01

This source package was reconciled against `xsession/openFPGALoader` master
`978ba76b6bad2774aa990530123586365a1568e4` and the submodule revisions pinned
by that commit:

- `xsession/libwdi`: `41992871403995fc53db3fa9dd9f068b676eb7fe`
- `xsession/xilinx-usb-driver`: `f7ab7583fe02e04c8d8977a096ca38bb002f2b93`

The release pipeline is modified so the compiled Xilinx Platform Cable USB
payload is built once and becomes part of both Windows deliverables.

## Fixed flow

1. Build libwdi for x86 and x64.
2. Create and SHA-256 verify `xilinx-platform-cable-windows.zip`.
3. Mount that canonical package into the Windows cross-build container.
4. Verify it again and embed the ZIP plus checksum at
   `share/openFPGALoader/drivers/` before creating the portable ZIP.
5. Upload the portable openFPGALoader ZIP as the Windows cross artifact.
6. The installer job downloads only that portable ZIP, verifies the embedded
   driver package and builds the Inno Setup installer from the same bytes.

## Local act / Docker Desktop fix

The XPCU Compose runtime mounts are parameterized and
`externals/xilinx-usb-driver/docker-build.sh` maps `/mnt/<drive>/...` runner
paths to Docker Desktop daemon-visible `/host_mnt/<drive>/...` or
`/run/desktop/mnt/host/<drive>/...` paths. This fixes the observed pair of
libwdi `exit 127` failures caused by empty runtime `/src` mounts.

## Workflow repair

The stale `windows-xpcu-drivers` dependency is removed. `windows-installer`
depends only on `windows-cross`, because the driver package is already inside
the portable Windows artifact.

## Validation

- All shell scripts: `bash -n` passed.
- All shell scripts are LF-only.
- Main workflow, docs workflow, Windows cross Compose and XPCU Compose YAML
  parse successfully.
- Workflow contains no `windows-xpcu-drivers` reference.
- Static checks confirm the portable packaging and installer scripts use the
  same embedded XPCU driver path.
