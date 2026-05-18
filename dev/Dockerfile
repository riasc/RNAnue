############################################################
# Dev image for CLion (or any IDE) Docker toolchain.
#
# This image pre-installs every build dependency RNAnue needs
# (compilers, CMake, Boost, HTSlib, ViennaRNA, segemehl) but
# does NOT clone or build the project itself — the source tree
# is mounted in from the host (typically at /tmp/RNAnue) and
# the IDE drives cmake/make against it.
#
# Build:   docker build -f dev/Dockerfile -t rnanue-dev .
# Use:     point your CLion Docker toolchain at rnanue-dev,
#          mount the repo at /tmp/RNAnue, and SeqAn3 at
#          /tmp/RNAnue/seqan3 (already in your source tree).
############################################################

FROM ubuntu:24.04

LABEL authors="Richard A. Schaefer"
LABEL purpose="RNAnue development toolchain (build deps only, no project sources)"

# Pin DEBIAN_FRONTEND so apt doesn't prompt during image build.
ENV DEBIAN_FRONTEND=noninteractive

# Base build tooling + dev libs that RNAnue and its transitive
# deps need at configure/compile time.
RUN apt-get -y update && apt-get -y upgrade && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        curl \
        git \
        gdb \
        pkg-config \
        libbz2-dev \
        zlib1g-dev \
        libncurses5-dev \
        liblzma-dev \
        libboost-all-dev \
        libmpfr-dev \
        liblapacke-dev \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# htslib (pkg-config exposes `htslib`)
WORKDIR /opt
RUN curl -L https://github.com/samtools/htslib/releases/download/1.20/htslib-1.20.tar.bz2 -o htslib-1.20.tar.bz2 && \
    tar -xf htslib-1.20.tar.bz2 && rm htslib-1.20.tar.bz2 && \
    cd htslib-1.20 && \
    ./configure && \
    make -j"$(nproc)" && \
    make install && \
    cd .. && rm -rf htslib-1.20

# ViennaRNA (pkg-config exposes `RNAlib2`)
# --without-swig avoids the Python/Perl binding compile that fails on
# minimal images. --without-doc / --without-tutorial skip LaTeX deps.
WORKDIR /opt
RUN curl -L https://www.tbi.univie.ac.at/RNA/download/sourcecode/2_6_x/ViennaRNA-2.6.4.tar.gz -o ViennaRNA-2.6.4.tar.gz && \
    tar -xf ViennaRNA-2.6.4.tar.gz && rm ViennaRNA-2.6.4.tar.gz && \
    cd ViennaRNA-2.6.4 && \
    ./configure --without-swig --without-doc --without-tutorial && \
    make -j"$(nproc)" && \
    make install && \
    cd .. && rm -rf ViennaRNA-2.6.4

# segemehl (runtime dep for the `align` subcall; not used at compile time
# but kept here so `detect`/`complete` work end-to-end from the container).
WORKDIR /opt
RUN curl -L http://legacy.bioinf.uni-leipzig.de/Software/segemehl/downloads/segemehl-0.3.4.tar.gz -o segemehl-0.3.4.tar.gz && \
    tar -xf segemehl-0.3.4.tar.gz && rm segemehl-0.3.4.tar.gz && \
    cd segemehl-0.3.4 && \
    make all && \
    cp segemehl.x /usr/local/bin && \
    cd .. && rm -rf segemehl-0.3.4

# Make sure pkg-config can find anything installed under /usr/local
# (htslib and ViennaRNA install their .pc files there).
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig

# Where CLion's Docker toolchain typically mounts the repo.
WORKDIR /tmp/RNAnue