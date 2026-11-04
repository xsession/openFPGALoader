# Arch Linux (rolling) build image for the Docker Linux deployment.
#
# Unlike the Alpine image (musl), this produces a glibc-linked binary that
# runs on Arch Linux and other glibc distros with an equal or newer glibc.
# Package names are the Arch equivalents of the Alpine build set:
#   base-devel = build-base
#   eudev      = eudev-dev   (libudev, pkg-config: libudev)
#   libftdi    = libftdi1-dev (pkg-config: libftdi1)
#   hidapi     = hidapi-dev
#   libusb     = libusb-dev
#   libgpiod   = libgpiod-dev (v2 API, pkg-config: libgpiod)
FROM archlinux/base:latest

# Rolling distro: refresh the package database before installing.
RUN pacman -Syu --noconfirm --needed \
      bash \
      base-devel \
      ca-certificates \
      cmake \
      eudev \
      file \
      git \
      gzip \
      hidapi \
      libftdi \
      libgpiod \
      libusb \
      ninja \
      pkgconf \
      tar \
      xz \
      zlib

WORKDIR /src

CMD ["bash", "/src/deploy/scripts/docker-linux-deploy.sh"]
