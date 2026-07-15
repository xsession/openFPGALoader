# syntax=docker/dockerfile:1.7
# Alpine-based Windows x86_64 cross-build image for openFPGALoader.
# It builds the Windows-side dependencies into /opt/x86_64-w64-mingw32 so the
# repository can be cross-compiled without relying on MSYS2 on the host.

ARG ALPINE_VERSION=edge
FROM alpine:${ALPINE_VERSION}

ARG TARGET_TRIPLE=x86_64-w64-mingw32
ARG CROSS_PREFIX=/opt/x86_64-w64-mingw32

ARG ZLIB_VERSION=1.3.2
ARG LIBUSB_VERSION=1.0.29
ARG HIDAPI_VERSION=0.15.0
ARG LIBFTDI_VERSION=1.5
# ARG XILINX_XUSB_REPO=https://github.com/gabrieldurante/xilinx-xusb.git

ENV TARGET_TRIPLE=${TARGET_TRIPLE} \
    CROSS_PREFIX=${CROSS_PREFIX} \
    PATH=${CROSS_PREFIX}/bin:/usr/lib/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    PKG_CONFIG_LIBDIR=${CROSS_PREFIX}/lib/pkgconfig \
    PKG_CONFIG_PATH=${CROSS_PREFIX}/lib/pkgconfig \
    CMAKE_TOOLCHAIN_FILE=/opt/toolchain-mingw64.cmake

RUN set -eux; \
    apk add --no-cache \
      bash \
      ca-certificates \
      ccache \
      cmake \
      curl \
      file \
      git \
      bzip2 \
      make \
      mingw-w64-gcc \
      ninja \
      patch \
      perl \
      pkgconf \
      python3 \
      tar \
      xz \
      zip; \
    update-ca-certificates; \
    mkdir -p "${CROSS_PREFIX}" /build-deps /src /out

COPY docker/cross/windows/toolchain-mingw64.cmake /opt/toolchain-mingw64.cmake

RUN set -eux; \
    cd /build-deps; \
    curl -fsSL "https://github.com/madler/zlib/archive/refs/tags/v${ZLIB_VERSION}.tar.gz" -o zlib.tar.gz \
      || curl -fsSL "https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz" -o zlib.tar.gz; \
    tar -xf zlib.tar.gz; \
    cmake -S "zlib-${ZLIB_VERSION}" -B zlib-build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${CROSS_PREFIX}" \
      -DZLIB_BUILD_EXAMPLES=OFF \
      -DZLIB_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=OFF; \
    cmake --build zlib-build --target zlib zlibstatic; \
    cmake --install zlib-build; \
    if [ -f "${CROSS_PREFIX}/lib/libzs.a" ] && [ ! -f "${CROSS_PREFIX}/lib/libz.a" ]; then \
      cp "${CROSS_PREFIX}/lib/libzs.a" "${CROSS_PREFIX}/lib/libz.a"; \
    fi; \
    ${TARGET_TRIPLE}-ranlib "${CROSS_PREFIX}/lib/libz.a"

RUN set -eux; \
    cd /build-deps; \
    curl -fsSL "https://github.com/libusb/libusb/releases/download/v${LIBUSB_VERSION}/libusb-${LIBUSB_VERSION}.tar.bz2" -o libusb.tar.bz2; \
    tar -xf libusb.tar.bz2; \
    cd "libusb-${LIBUSB_VERSION}"; \
    ./configure \
      --host="${TARGET_TRIPLE}" \
      --build="$(gcc -dumpmachine)" \
      --prefix="${CROSS_PREFIX}" \
      --disable-shared \
      --enable-static \
      --disable-udev; \
    make -j"$(nproc)"; \
    make install

RUN set -eux; \
    cd /build-deps; \
    curl -fsSL "https://github.com/libusb/hidapi/archive/refs/tags/hidapi-${HIDAPI_VERSION}.tar.gz" -o hidapi.tar.gz; \
    tar -xf hidapi.tar.gz; \
    cmake -S "hidapi-hidapi-${HIDAPI_VERSION}" -B hidapi-build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${CROSS_PREFIX}" \
      -DBUILD_SHARED_LIBS=OFF \
      -DHIDAPI_BUILD_HIDTEST=OFF \
      -DHIDAPI_WITH_HIDRAW=OFF \
      -DHIDAPI_WITH_LIBUSB=ON \
      -DHIDAPI_WITH_TESTS=OFF; \
    cmake --build hidapi-build; \
    cmake --install hidapi-build

RUN set -eux; \
    cd /build-deps; \
    curl -fsSL "https://www.intra2net.com/en/developer/libftdi/download/libftdi1-${LIBFTDI_VERSION}.tar.bz2" -o libftdi.tar.bz2; \
    tar -xf libftdi.tar.bz2; \
    cmake -S "libftdi1-${LIBFTDI_VERSION}" -B libftdi-build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/toolchain-mingw64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="${CROSS_PREFIX}" \
      -DCMAKE_PREFIX_PATH="${CROSS_PREFIX}" \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config \
      -DSTATICLIBS=ON \
      -DEXAMPLES=OFF \
      -DFTDI_EEPROM=OFF \
      -DDOCUMENTATION=OFF \
      -DPYTHON_BINDINGS=OFF; \
    cmake --build libftdi-build; \
    cmake --install libftdi-build

# Local Xilinx firmware is copied by scripts/docker-cross-windows.sh at
# container runtime. The Compose bind mount at /src does not exist while this
# image is being built, so /src/ise_programmer_bins cannot be read in a RUN
# instruction here.

WORKDIR /src

CMD ["/src/scripts/docker-cross-windows.sh"]
