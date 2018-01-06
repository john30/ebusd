FROM debian:jessie

RUN apt-get update && apt-get install -y \
    autoconf automake g++ make \
    libmosquitto-dev git

LABEL maintainer "ebusd@ebusd.eu"

WORKDIR /build
CMD ["/bin/bash"]
