FROM ubuntu:20.04

RUN DPKG_FRONTEND=noninteractive apt update && apt upgrade --yes

RUN apt install --yes build-essential git locales

RUN mkdir /src && cd /src && git clone https://github.com/HeRCLab/libherc.git && cd libherc && make CC=gcc PREFIX=/usr && make CC=gcc PREFIX=/usr install && ldconfig /usr/lib

RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8
