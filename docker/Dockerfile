# syntax=docker/dockerfile:1.6
#
# Atari Jaguar SDK image for the 240p Test Suite (and other rmvlib projects).
#
# Bakes in the entire Jaguar toolchain so macOS, Linux, and CI all use the
# exact same binaries:
#   - m68k-atari-mint cross compiler (Vincent Rivière's packages)
#   - RMAC assembler (installed as both `rmac` and `mac`)
#   - RLN  linker    (installed as both `rln`  and `aln`)
#   - The Removers Library (rmvlib + jlibc)
#
# The image exposes JAGPATH=/opt/jagsdk, matching the variable referenced in
# Makefile.config / Makefile.example.

FROM ubuntu:22.04 AS sdk-build

ENV DEBIAN_FRONTEND=noninteractive \
    JAGPATH=/opt/jagsdk \
    LC_ALL=C.UTF-8 \
    LANG=C.UTF-8

# ---------------------------------------------------------------------------
# Base build deps + Vincent Rivière's m68k-atari-mint cross toolchain.
#
# The PPA is added via a manual keyring (instead of add-apt-repository) so
# the build doesn't depend on launchpadlib / GPG-in-container quirks.
# ---------------------------------------------------------------------------
ARG VRIVIERE_KEY=4AAD3A5DB5690522
ARG UBUNTU_RELEASE=jammy

RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        curl \
        dirmngr \
        doxygen \
        git \
        gnupg \
        make \
        wget \
        xz-utils \
        unzip \
        zip \
    ; \
    install -d /etc/apt/keyrings; \
    curl -fsSL "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x${VRIVIERE_KEY}" \
        | gpg --dearmor -o /etc/apt/keyrings/vriviere.gpg; \
    echo "deb [signed-by=/etc/apt/keyrings/vriviere.gpg] http://ppa.launchpad.net/vriviere/ppa/ubuntu ${UBUNTU_RELEASE} main" \
        > /etc/apt/sources.list.d/vriviere.list; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        gcc-m68k-atari-mint \
        cross-mint-essential \
    ; \
    rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# Build RMAC + RLN from upstream sources.
#
# Pinned to specific upstream commit SHAs so the image is reproducible and
# protected against the upstream `master` moving under our feet (supply
# chain hygiene). To bump:
#   1. git ls-remote http://tiddly.mooo.com:5000/{rmac,rln}/{rmac,rln}.git HEAD
#   2. update RMAC_REF / RLN_REF below, rebuild, smoke-test.
#
# NOTE on transport: tiddly.mooo.com only serves git over plain HTTP on
# port 5000 (no HTTPS endpoint as of 2026-04). The pinned commit SHAs are
# what gives us integrity here -- `git checkout <sha>` will refuse to
# proceed if the fetched objects don't hash to the requested SHA, so a
# tampered transport cannot silently substitute different code. If/when
# the upstream offers HTTPS, flip these defaults.
# ---------------------------------------------------------------------------
ARG RMAC_REPO=http://tiddly.mooo.com:5000/rmac/rmac.git
ARG RMAC_REF=a81f2e5a4a4a4c62f1c411a12edd048c9f6c72c5
ARG RLN_REPO=http://tiddly.mooo.com:5000/rln/rln.git
ARG RLN_REF=4cde5893338e264c22e4fe26cd23f4b4db9107e4

# Helper: clone-then-checkout works for any ref (branch, tag, or raw SHA),
# unlike `git clone --branch <ref>` which only accepts branch/tag names.
# These repos are tiny (<2 MiB), so the lack of --depth 1 is irrelevant.
RUN set -eux; \
    mkdir -p "${JAGPATH}/bin" "${JAGPATH}/lib/lib" "${JAGPATH}/lib/include" /tmp/src; \
    cd /tmp/src; \
    git clone "${RMAC_REPO}" rmac; \
    git -C rmac checkout --detach "${RMAC_REF}"; \
    make -C rmac && \
        install -m 0755 rmac/rmac "${JAGPATH}/bin/rmac" && \
        ln -sf rmac "${JAGPATH}/bin/mac"; \
    git clone "${RLN_REPO}" rln; \
    git -C rln checkout --detach "${RLN_REF}"; \
    make -C rln && \
        install -m 0755 rln/rln "${JAGPATH}/bin/rln" && \
        ln -sf rln "${JAGPATH}/bin/aln"

# ---------------------------------------------------------------------------
# Build jlibc + rmvlib (The Removers Library).
# Pinned to upstream SHAs for the same reproducibility reasons as above.
# Bump via: git ls-remote https://github.com/theRemovers/{jlibc,rmvlib}.git HEAD
# ---------------------------------------------------------------------------
ARG JLIBC_REPO=https://github.com/theRemovers/jlibc.git
ARG JLIBC_REF=1937a1372ff3ac452ce89ec3291c9490d2b1212b
ARG RMVLIB_REPO=https://github.com/theRemovers/rmvlib.git
ARG RMVLIB_REF=164026e1aca2e46c16329062310949b2d6383e0a

RUN set -eux; \
    cd /tmp/src; \
    git clone "${JLIBC_REPO}" jlibc; \
    git -C jlibc checkout --detach "${JLIBC_REF}"; \
    JAGPATH="${JAGPATH}" MAKEFLAGS=--no-print-directory make -C jlibc; \
    JAGPATH="${JAGPATH}" MAKEFLAGS=--no-print-directory make -C jlibc install; \
    git clone "${RMVLIB_REPO}" rmvlib; \
    git -C rmvlib checkout --detach "${RMVLIB_REF}"; \
    JAGPATH="${JAGPATH}" MAKEFLAGS=--no-print-directory make -C rmvlib; \
    JAGPATH="${JAGPATH}" MAKEFLAGS=--no-print-directory make -C rmvlib install

# ---------------------------------------------------------------------------
# Stage libgcc.a for the m68k-atari-mint target so the Jaguar link line
# (which references `libgcc.a` directly via -l-less form) can find it.
# ---------------------------------------------------------------------------
RUN set -eux; \
    LIBGCC="$(find /usr/lib/gcc/m68k-atari-mint -name libgcc.a | head -n1)"; \
    test -n "${LIBGCC}"; \
    install -m 0644 "${LIBGCC}" "${JAGPATH}/lib/lib/libgcc.a"

# ---------------------------------------------------------------------------
# Note: the original Tursilion `makefastboot` is a Windows-only Visual Studio
# project, so we don't try to compile it here. The repo ships a pure-Python
# port at scripts/make-rom.py which the `make rom` target invokes directly,
# so the runtime image only needs python3 (already installed below).
# ---------------------------------------------------------------------------
# Final lean image: keep only what is needed at build/run time.
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS sdk

ENV DEBIAN_FRONTEND=noninteractive \
    JAGPATH=/opt/jagsdk \
    LC_ALL=C.UTF-8 \
    LANG=C.UTF-8

ARG VRIVIERE_KEY=4AAD3A5DB5690522
ARG UBUNTU_RELEASE=jammy

RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        coreutils \
        curl \
        dirmngr \
        file \
        gnupg \
        make \
        python3 \
        xz-utils \
    ; \
    install -d /etc/apt/keyrings; \
    curl -fsSL "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x${VRIVIERE_KEY}" \
        | gpg --dearmor -o /etc/apt/keyrings/vriviere.gpg; \
    echo "deb [signed-by=/etc/apt/keyrings/vriviere.gpg] http://ppa.launchpad.net/vriviere/ppa/ubuntu ${UBUNTU_RELEASE} main" \
        > /etc/apt/sources.list.d/vriviere.list; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        gcc-m68k-atari-mint \
        cross-mint-essential \
    ; \
    rm -rf /var/lib/apt/lists/*

COPY --from=sdk-build /opt/jagsdk /opt/jagsdk

ENV PATH="${JAGPATH}/bin:${PATH}"

WORKDIR /work

LABEL org.opencontainers.image.title="Jaguar SDK" \
      org.opencontainers.image.description="Atari Jaguar dev environment: m68k-atari-mint + RMAC/RLN (mac/aln) + rmvlib (jlibc)" \
      org.opencontainers.image.source="https://github.com/BitJag/atari_jaguar_240p_test_suite" \
      org.opencontainers.image.licenses="GPL-2.0-or-later"

CMD ["make"]
