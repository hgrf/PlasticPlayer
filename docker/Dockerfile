FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    bc \
    build-essential \
    cpio \
    file \
    git \
    libncurses-dev \
    rsync \
    swig \
    unzip \
    wget \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
