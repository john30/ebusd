ebusd Docker image
==================

An [ebusd](https://github.com/john30/ebusd/) Docker image is available on the
[Docker Hub](https://hub.docker.com/r/john30/ebusd/) and is able to download the latest released German
[configuration files](https://github.com/john30/ebusd-configuration/) from a [dedicated webservice](https://upd.ebusd.eu/).

It allows you to run ebusd without actually installing (or even building) it on your system.
You might even be able to run it on a non-Linux operating system, which is at least known to
work well on a Synology Diskstation as well as Windows with Docker Desktop.


Getting the image
-----------------
To download the latest release image from the hub, use the following command:  
> docker pull john30/ebusd

The image is able to run on any of the following architectures and the right image will be picked automatically:
* amd64
* i386
* arm32v7
* arm64v8

Due to changes of docker hub policies, the development set of images with "devel" tag are currently not automatically
built with every commit to the git repository. Run the following command to use it: 

> docker pull john30/ebusd:devel


Running interactively
---------------------
To run an ebusd container interactively, e.g. on serial device /dev/ttyUSB1, use the following command:
> docker run --rm -it --device=/dev/ttyUSB1:/dev/ttyUSB0 -p 8888 john30/ebusd

This will show the ebusd output directly in the terminal.


Running in background
---------------------
To start an ebusd container and have it run in the background, e.g. on serial device /dev/ttyUSB1, use the following command:
> docker run -d --name=ebusd --device=/dev/ttyUSB1:/dev/ttyUSB0 -p 8888 john30/ebusd

The container has the name "ebusd", so you can use that when querying docker about the container.

In order to get the log output from ebusd, use the following command:
> docker logs ebusd


Using a network device
----------------------
When using a network device, the "--device" argument to docker can be omitted, but the device information has to be
passed on to ebusd:
> docker run --rm -it -p 8888 john30/ebusd --scanconfig -d 192.168.178.123:10000 --latency=20

Note: the required "-f" (foreground) argument is passed as environment variable and does not need to be specified anymore.

Note: the default "--scanconfig" argument is only passed to ebusd if it is called without any additional arguments.
So when passing further arguments, this usually needs to be added as well or set as environment variable EBUSD_SCANCONFIG.


Running with MQTT broker
------------------------
To start an ebusd container in the background and have it connect to your MQTT broker, use the following command while
replacing "BROKERHOST" with your MQTT broker host name or IP address:
> docker run -d --name=ebusd --device=/dev/ttyUSB0 -p 8888 john30/ebusd --scanconfig -d /dev/ttyUSB0 --mqttport=1883 --mqtthost=BROKERHOST


Use of environment variables
----------------------------
Instead of passing arguments (at the end of docker run) to ebusd, almost all (long) arguments can also be passed as
environment variables with the prefix `EBUSD_`, e.g. the following line can be used instead of the last example above:
> docker run -d --name=ebusd --device=/dev/ttyUSB0 -p 8888 -e EBUSD_SCANCONFIG= -e EBUSD_DEVICE=/dev/ttyUSB0 -e EBUSD_MQTTPORT=1883 -e EBUSD_MQTTHOST=BROKERHOST john30/ebusd

This eases use of e.g. docker-compose files.

See [docker-compose.example.yaml](https://github.com/john30/ebusd/blob/master/contrib/docker/docker-compose.example.yaml) for further environment variables.


Running newer images on older operating systems
-----------------------------------------------
When running a newer image on an older operating system, the container might face several issues like invalid system
date or access restrictions, which e.g. prevents using the HTTPS based config webservice.

In order to test if this is the case for the current setup, starting the image like this will reveal a date of 1970:
> docker run -it --rm john30/ebusd date

If that's the case, the easiest way to circumvent it is to run the container without the default security constraints
set by docker, i.e. add the following argument to the docker run command:
> --security-opt seccomp=unconfined

Another option is to update docker and/or the [libseccomp library](https://github.com/moby/moby/issues/40734) on the
host. See also [here](https://serverfault.com/questions/1037146/docker-container-with-random-date/1048351#1048351).
