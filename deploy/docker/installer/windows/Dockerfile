# Dockerfile for building openFPGALoader Windows installer via Wine + Inno Setup
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    wget \
    cabextract \
    unzip \
    cabextract \
    wine \
    cabextract \
    cabextract \
    && rm -rf /var/lib/apt/lists/*

# Install Inno Setup 6
RUN set -eux; \
    wget -q "https://files.jrsoftware.org/is/6/innosetup-6.2.2.exe" -O /tmp/is.exe; \
    WINEARCH=win64 WINEPREFIX=/root/.wine winecapp /tmp/is.exe /verysilent /dir="C:\\Program Files (x86)\\Inno Setup 6" || true; \
    rm -f /tmp/is.exe

WORKDIR /src
CMD ["/src/deploy/scripts/build-windows-installer.sh"]