FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV DEBCONF_NONINTERACTIVE_SEEN true

RUN apt-get update && apt-get upgrade --yes

RUN apt-get install --yes build-essential git locales gdb

RUN mkdir /src && cd /src && git clone https://github.com/HeRCLab/libherc.git && cd libherc && make CC=gcc && make CC=gcc install

RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8
