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

RUN if [ `uname -m` = aarch64 ]; then \
    wget https://github.com/containerbase/flutter-prebuild/releases/download/3.27.1/flutter-3.27.1-aarch64.tar.xz && \
    tar xf flutter-3.27.1-aarch64.tar.xz && \
    rm flutter-3.27.1-aarch64.tar.xz && \
    mv 3.27.1 /opt/flutter ; \
  else \
    wget https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.27.1-stable.tar.xz && \
    tar xf flutter_linux_3.27.1-stable.tar.xz && \
    rm flutter_linux_3.27.1-stable.tar.xz && \
    mv flutter /opt/ && \
    git config --global --add safe.directory /opt/flutter ; \
  fi

WORKDIR /workspace
