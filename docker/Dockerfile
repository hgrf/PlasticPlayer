FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    bc \
    build-essential \
    cpio \
    file \
    git \
    libncurses-dev \
    python3 \
    python3-pillow \
    rsync \
    swig \
    unzip \
    wget \
    && rm -rf /var/lib/apt/lists/*

RUN wget https://github.com/containerbase/flutter-prebuild/releases/download/3.27.1/flutter-3.27.1-aarch64.tar.xz && \
    tar xf flutter-3.27.1-aarch64.tar.xz && \
    rm flutter-3.27.1-aarch64.tar.xz && \
    mv 3.27.1 /opt/flutter

WORKDIR /workspace
