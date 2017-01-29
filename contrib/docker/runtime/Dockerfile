FROM debian

RUN apt-get update
RUN apt-get install -y curl

LABEL maintainer "ebusd@ebusd.eu"

RUN mkdir /etc/ebusd \
    && curl -SL https://github.com/john30/ebusd-configuration/archive/master.tar.gz \
    | tar xz --strip-components=3 -C /etc/ebusd ebusd-configuration-master/ebusd-2.1.x/de

ENV EBUSD_VERSION 3.0pre
ENV EBUSD_ARCH amd64
COPY ebusd-${EBUSD_VERSION}_${EBUSD_ARCH}.deb ebusd.deb

RUN dpkg -i ebusd.deb

EXPOSE 8888

COPY docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD ["-f", "--scanconfig"]