FROM balenalib/raspberrypi3

RUN install_packages \
    logrotate libmosquitto1 libstdc++6 libc6 libgcc1 \
    curl

LABEL maintainer "ebusd@ebusd.eu"

ENV EBUSD_VERSION 21.1
ENV EBUSD_ARCH armhf

LABEL version "${EBUSD_VERSION}-${EBUSD_ARCH}"

RUN curl -SL https://github.com/john30/ebusd/releases/download/v${EBUSD_VERSION}/ebusd-${EBUSD_VERSION}_${EBUSD_ARCH}-stretch_mqtt1.deb > ebusd.deb

RUN dpkg -i ebusd.deb

RUN rm ebusd.deb

EXPOSE 8888

COPY docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["-f", "--scanconfig"]
