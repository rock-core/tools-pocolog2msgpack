FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install --yes \
      build-essential \
      cmake \
      wget \
      git \
      sudo \
      ruby \
      ruby-dev \
      python3 \
      python3-pexpect \
      python3-msgpack \
      python3-setuptools \
      python3-distutils \
      python3-nose \
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
      libxml2-dev \
      doxygen \
      graphviz \
      libyaml-cpp-dev \
      zlib1g-dev

SHELL ["/bin/bash", "-c"]
ENV SHELL=/bin/bash

# autoproj needs these to be defined (will be removed later):
RUN git config --global user.email infuse@example.com \
    && git config --global user.name InFuse

WORKDIR /opt/rock
COPY autoproj-seed-config.yml /opt/rock/
RUN wget http://www.rock-robotics.org/autoproj_bootstrap --directory-prefix=/opt/rock/
RUN ruby autoproj_bootstrap git https://github.com/rock-core/buildconf.git --no-interactive --seed-config=autoproj-seed-config.yml
COPY manifest /opt/rock/autoproj
COPY init.rb /opt/rock/autoproj
COPY overrides.rb /opt/rock/autoproj
COPY overrides.yml /opt/rock/autoproj
RUN source env.sh && autoproj update --no-interactive
RUN source env.sh && autoproj build --verbose --no-interactive

# Reset git configuration
RUN rm /root/.gitconfig
RUN rm -rf /var/lib/apt/lists/*

LABEL maintainer="Alexander.Fabisch@dfki.de"
LABEL version="1.1.0"
LABEL description="Image providing the dependencies of pocolog2msgpack"

# build docker image:
# docker build -t af01/pocolog2msgpack-ci .
# create docker container:
# docker run -it af01/pocolog2msgpack-ci
# or
# docker build -t "pocolog2msgpack_env" .
