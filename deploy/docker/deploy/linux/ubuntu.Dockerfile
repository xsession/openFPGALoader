# Ubuntu 24.04 build image for the Docker Linux deployment.
#
# Produces a glibc binary that runs on Ubuntu 24.04 and newer, plus the
# Ubuntu/Debian `.deb` installer (dpkg-dev provides dpkg-deb, which
# docker-linux-deploy.sh detects to trigger the Debian package build).
# The apt set mirrors the CI Linux job (build-binaries.yml) so the
# produced binary and its runtime dependencies match what CI verifies.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      bash \
      build-essential \
      ca-certificates \
      cmake \
      dpkg-dev \
      file \
      git \
      gzip \
      libftdi1-2 \
      libftdi1-dev \
      libgpiod-dev \
      libhidapi-dev \
      libhidapi-hidraw0 \
      libhidapi-libusb0 \
      libudev-dev \
      libusb-1.0-0-dev \
      ninja-build \
      pkg-config \
      xz-utils \
      zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

CMD ["bash", "/src/deploy/scripts/docker-linux-deploy.sh"]
