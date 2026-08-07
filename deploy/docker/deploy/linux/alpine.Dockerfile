ARG ALPINE_VERSION=3.20
FROM alpine:${ALPINE_VERSION}

RUN apk add --no-cache \
      bash \
      build-base \
      ca-certificates \
      cmake \
      eudev-dev \
      file \
      git \
      gzip \
      hidapi-dev \
      libftdi1-dev \
      libgpiod-dev \
      libusb-dev \
      linux-headers \
      ninja \
      pkgconf \
      tar \
      xz \
      zlib-dev

WORKDIR /src

CMD ["bash", "/src/deploy/scripts/docker-linux-deploy.sh"]
