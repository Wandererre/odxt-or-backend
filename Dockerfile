FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies for C++ binaries and Python
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    libssl-dev \
    libcrypto++-dev \
    libgmp-dev \
    libntl-dev \
    libhiredis-dev \
    python3 \
    python3-pip \
    python3-venv \
    redis-tools \
    git \
    cmake \
    && rm -rf /var/lib/apt/lists/*

# Install redis-plus-plus from source
RUN git clone https://github.com/sewenew/redis-plus-plus.git /tmp/redis-plus-plus \
    && cd /tmp/redis-plus-plus \
    && mkdir build \
    && cd build \
    && cmake -DREDIS_PLUS_PLUS_CXX_STANDARD=17 .. \
    && make \
    && make install \
    && ldconfig \
    && rm -rf /tmp/redis-plus-plus

WORKDIR /app

COPY . /app

# Build odxt-cli using the updated build.sh
RUN chmod +x build.sh && ./build.sh

# Set up virtual environment and install dependencies
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
RUN pip install --no-cache-dir -r requirements.txt

EXPOSE 8001

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8001"]
