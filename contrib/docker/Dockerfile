ARG BASE_IMAGE

FROM $BASE_IMAGE AS build

RUN apt-get update && apt-get install -y \
    knxd-dev knxd libmosquitto-dev libssl-dev libstdc++6 libc6 libgcc1 \
    curl \
    autoconf automake g++ make git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

ARG TARGETARCH
ARG TARGETVARIANT
ARG EBUSD_VERSION
ARG GIT_REVISION

ENV EBUSD_ARCH $TARGETARCH$TARGETVARIANT
ENV EBUSD_VERSION $EBUSD_VERSION
ENV GIT_REVISION $GIT_REVISION

ADD . /build
RUN RUNTEST=full GIT_REVISION=$GIT_REVISION ./make_debian.sh --with-knxd




FROM $BASE_IMAGE-slim AS image

RUN apt-get update && apt-get install -y \
    knxd-dev knxd libmosquitto1 libssl1.1 ca-certificates libstdc++6 libc6 libgcc1 \
    && rm -rf /var/lib/apt/lists/*

LABEL maintainer="ebusd@ebusd.eu"

ARG TARGETARCH
ARG TARGETVARIANT
ARG EBUSD_VERSION
ARG EBUSD_IMAGE

ENV EBUSD_ARCH $TARGETARCH$TARGETVARIANT
ENV EBUSD_VERSION $EBUSD_VERSION

LABEL version="${EBUSD_VERSION}-${EBUSD_ARCH}-devel"

COPY --from=build /build/ebusd-*_mqtt1.deb ebusd.deb

RUN dpkg -i "--path-exclude=/etc/default/*" "--path-exclude=/etc/init.d/*" "--path-exclude=/lib/systemd/*" ebusd.deb && rm -f ebusd.deb \
    && update-ca-certificates \
    && ebusd -V

EXPOSE 8888

COPY --from=build /build/contrib/docker/docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]

ENV EBUSD_FOREGROUND ""
CMD ["--scanconfig"]
