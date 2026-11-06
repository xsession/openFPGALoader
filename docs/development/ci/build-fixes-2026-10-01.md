# Windows release packaging fixes - 2026-10-01

This source package was reconciled against `xsession/openFPGALoader` master
`106f6bb48a80249886e112164fdcd82648f2a112` and the submodule revisions pinned
by that commit:

- `xsession/libwdi`: `41992871403995fc53db3fa9dd9f068b676eb7fe`
- `xsession/xilinx-usb-driver`: `c5c8a695ff048ac59680b5c82f62922be16e9873`

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

## Inno Setup Docker output-permission fix

The GitHub-hosted Ubuntu installer job successfully verified the portable ZIP and
its embedded XPCU driver payload, but Inno Setup failed with Windows error 5 while
copying `Setup.e32` to `Z:\dist\openFPGALoader-...-setup.e32.tmp`. The pinned
`amake/innosetup` image intentionally runs Wine as the unprivileged `xclient` user;
the GitHub workspace bind mount was readable but not writable by that user.

The installer now compiles into a temporary Docker named volume mounted at `/out`
(`Z:\out` inside Wine). The host `dist` tree is mounted read-only during compiler
execution. After a successful compile, a root Alpine helper copies only the final
installer EXE back to `dist/docker-windows`; the named volume is removed by a trap.
This also avoids Inno/Wine temporary-file locking and permission differences across
GitHub Actions, Linux hosts, WSL2 and Docker Desktop.
