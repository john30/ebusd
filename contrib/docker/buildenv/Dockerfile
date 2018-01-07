FROM debian:jessie

RUN apt-get update && apt-get install -y \
    autoconf automake g++ make git \
    libmosquitto-dev libstdc++6 libc6 libgcc1 \
    && rm -rf /var/lib/apt/lists/*

LABEL maintainer "ebusd@ebusd.eu"

WORKDIR /build
CMD ["/bin/bash"]
