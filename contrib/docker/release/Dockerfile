FROM debian

RUN apt-get update && apt-get install -y \
    libmosquitto1 \
    curl

LABEL maintainer "ebusd@ebusd.eu"

ENV EBUSD_VERSION 3.0
ENV EBUSD_ARCH amd64

LABEL version "${EBUSD_VERSION}-${EBUSD_ARCH}"

RUN curl -SL https://github.com/john30/ebusd/releases/download/v${EBUSD_VERSION}/ebusd-${EBUSD_VERSION}_${EBUSD_ARCH}.deb > ebusd.deb

RUN dpkg -i ebusd.deb

RUN rm ebusd.deb

RUN curl -SL https://github.com/john30/ebusd-configuration/archive/master.tar.gz \
    | tar xz --strip-components=3 -C /etc/ebusd ebusd-configuration-master/ebusd-2.1.x/de

EXPOSE 8888

COPY docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["-f", "--scanconfig"]
