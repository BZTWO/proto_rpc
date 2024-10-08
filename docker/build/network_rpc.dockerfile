FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get clean && \
    apt-get autoclean

RUN apt update && \
    apt install -y \
    vim \
    htop \
    apt-utils \
    curl \
    cmake \
    net-tools \
    gdb  gcc g++ \
    libboost-all-dev \
    libgoogle-glog-dev

COPY install/abseil /tmp/install/abseil
RUN chmod +x /tmp/install/abseil/install_abseil.sh && /tmp/install/abseil/install_abseil.sh

COPY install/protobuf /tmp/install/protobuf
RUN chmod +x /tmp/install/protobuf/install_protobuf.sh && /tmp/install/protobuf/install_protobuf.sh


WORKDIR /work
 

