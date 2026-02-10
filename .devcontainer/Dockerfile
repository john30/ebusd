FROM debian:12

RUN apt-get update && apt-get install -y --no-install-recommends \
    autoconf \
    automake \
    cmake \
    g++ \
    make \
    libc6-dev \
    knxd-dev \
    libmosquitto-dev \
    libssl-dev \
    git \
    pkg-config \
    gdb \
    && rm -rf /var/lib/apt/lists/*