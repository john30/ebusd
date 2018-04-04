FROM debian:stretch-slim

RUN apt-get update && apt-get install -y \
    logrotate libmosquitto1 libstdc++6 libc6 libgcc1 \
    curl \
    && rm -rf /var/lib/apt/lists/*

LABEL maintainer "ebusd@ebusd.eu"

ENV EBUSD_VERSION 3.1
ENV EBUSD_ARCH amd64

LABEL version "${EBUSD_VERSION}-${EBUSD_ARCH}-mqtt1"

COPY ebusd-${EBUSD_VERSION}_${EBUSD_ARCH}_mqtt1.deb ebusd.deb

RUN dpkg -i ebusd.deb

RUN rm ebusd.deb

EXPOSE 8888

COPY docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["-f", "--scanconfig"]
