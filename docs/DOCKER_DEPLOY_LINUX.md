# Docker Linux deployment

This builds a native Linux `openFPGALoader` binary inside an Alpine container
and packages the install tree as a tarball.

## Build the image

```bash
docker compose -f docker-compose.deploy-linux.yml build
```

## Build and package openFPGALoader

```bash
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy
```

Artifacts are written to:

```text
dist/docker-linux/
```

The package contains:

- `bin/openFPGALoader`
- installed `spiOverJtag` / `bpiOverJtag` assets
- Spartan-6 ISE `.cor` bridge files
- Xilinx USB firmware `.hex` files when present in `ise_programmer_bins/`
- Linux udev rules under `lib/udev/rules.d/`
- a build manifest with `file` and `ldd` output

## Clean rebuild

```bash
docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy /src/scripts/docker-linux-deploy.sh --clean
```

## Pass extra CMake flags

```bash
CMAKE_EXTRA_ARGS="-DENABLE_CMSISDAP=OFF -DENABLE_LIBGPIOD=OFF" \
  docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy
```

## Notes

- The default image is Alpine 3.20.
- The resulting binary is dynamically linked against the Alpine packages
  installed in the Docker image. Check
  `share/doc/openFPGALoader/docker-linux-build-manifest.txt` in the package for
  the exact runtime libraries.
- This is a deploy/package helper, not a distro package. Install udev rules on
  the target machine if USB probes are not accessible to normal users.
