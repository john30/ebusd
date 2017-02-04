FROM debian

RUN apt-get update
RUN apt-get install -y git autoconf automake g++ make
RUN apt-get install -y libmosquitto-dev

WORKDIR /build
CMD ["/bin/bash"]
