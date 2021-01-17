
FROM debian:stretch as build

RUN apt-get update && apt-get install -y \
    libmosquitto-dev libstdc++6 libc6 libgcc1 \
    curl \
    autoconf automake g++ make git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

ENV EBUSD_VERSION 21.1
ENV EBUSD_ARCH amd64

RUN git clone https://github.com/john30/ebusd.git /build \
    && ./make_debian.sh

ARG UPLOAD_URL
ARG UPLOAD_CREDENTIALS
ARG UPLOAD_ONLY



FROM debian:stretch-slim

RUN apt-get update && apt-get install -y \
    logrotate libmosquitto1 libstdc++6 libc6 libgcc1 \
    && rm -rf /var/lib/apt/lists/*

LABEL maintainer "ebusd@ebusd.eu"

ENV EBUSD_VERSION 21.1
ENV EBUSD_ARCH amd64

LABEL version "${EBUSD_VERSION}-${EBUSD_ARCH}-devel"

COPY --from=build /build/ebusd-*_mqtt1.deb ebusd.deb

RUN dpkg -i ebusd.deb \
    && rm -f ebusd.deb

EXPOSE 8888

COPY docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["-f", "--scanconfig"]
