# Windows Xilinx Platform Cable driver packaging

The Windows release has a single canonical Xilinx Platform Cable USB driver package:

`xilinx-platform-cable-windows.zip`

It is built once from the pinned `externals/libwdi` and `externals/xilinx-usb-driver`
sources. The build verifies its SHA-256 checksum before openFPGALoader packaging starts.

## Portable ZIP

The canonical driver archive and its checksum are embedded unchanged in:

```text
share/openFPGALoader/drivers/
  xilinx-platform-cable-windows.zip
  xilinx-platform-cable-windows.zip.sha256
```

The Windows portable ZIP build is required to fail if either file is missing or the
checksum does not match. This prevents publishing a Windows package that silently
omits the Xilinx Platform Cable driver payload.

## Windows installer

The installer job downloads only the portable openFPGALoader ZIP. It verifies and
extracts that ZIP, then verifies the embedded XPCU driver archive again. The Inno
Setup build consumes exactly that embedded archive; there is no second driver build
and no independent driver artifact dependency.

The installer always carries the compressed driver archive under the application
`share/openFPGALoader/drivers` tree. When the user selects **Install Xilinx Platform
Cable USB drivers**, the package is additionally expanded into the installer input
area and the bundled PowerShell installation helper is executed after installation.

## `act` on Windows / Docker Desktop

`act` runs the workflow inside a Linux container, while Docker Desktop runs the
Docker daemon outside that container. Runtime bind mounts therefore cannot blindly
reuse paths such as `/mnt/d/openFPGALoader`.

`externals/xilinx-usb-driver/docker-build.sh` probes Docker Desktop's daemon-visible
paths (`/host_mnt/<drive>/...` and `/run/desktop/mnt/host/<drive>/...`) and exports
those locations to Compose. The same rule is used by the main Windows cross-build.
This avoids empty `/src` mounts and the resulting libwdi `exit 127` failures.
