FROM ubuntu:16.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install --yes --quiet \
      build-essential \
      cmake \
      wget \
      git \
      sudo \
      ruby \
      ruby-dev \
      python \
      python-pip \
      castxml \
      libeigen3-dev \
      pkg-config \
      autoconf \
      automake \
      libboost-dev \
      libboost-filesystem-dev \
      libboost-graph-dev \
      libboost-iostreams-dev \
      libboost-program-options-dev \
      libboost-regex-dev \
      libboost-system-dev \
      libboost-thread-dev \
      libopenscenegraph-dev \
      libqt4-dev \
      libqt4-opengl-dev \
      libxml2-dev \
      qt4-qmake
RUN pip install msgpack nose pexpect

SHELL ["/bin/bash", "-c"]

# autoproj needs these to be defined (will be removed later):
RUN git config --global user.email infuse@example.com \
    && git config --global user.name InFuse

WORKDIR /opt/rock
RUN wget http://www.rock-robotics.org/autoproj_bootstrap --directory-prefix=/opt/rock/
COPY config.yml /opt/rock/
RUN AUTOPROJ_BOOTSTRAP_IGNORE_NONEMPTY_DIR=1 ruby autoproj_bootstrap git https://github.com/rock-core/buildconf.git --seed-config=/opt/rock/config.yml
COPY manifest /opt/rock/autoproj
COPY init.rb /opt/rock/autoproj
COPY overrides.rb /opt/rock/autoproj
COPY overrides.yml /opt/rock/autoproj
RUN source env.sh && autoproj update
RUN source env.sh && autoproj build
RUN git clone https://github.com/msgpack/msgpack-c.git tools/msgpack-c --depth 1
RUN source env.sh && autoproj build tools/msgpack-c

# Reset git configuration
RUN rm /root/.gitconfig

LABEL maintainer="Alexander.Fabisch@dfki.de"
LABEL version="1.0.0"
LABEL description="Image providing the dependencies of pocolog2msgpack"

# build docker image:
# docker build -t af01/pocolog2msgpack-ci .
# create docker container:
# docker run -it af01/pocolog2msgpack-ci
