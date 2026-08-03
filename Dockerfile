FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ── System packages ───────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    cmake \
    python3 \
    python3-pip \
    python3-venv \
    redis-server \
    libgmp-dev \
    libgmpxx4ldbl \
    libhiredis-dev \
    libssl-dev \
    pkg-config \
    git \
    wget \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ── Build redis-plus-plus from source ────────────────────────────────────────
RUN git clone --depth 1 --branch 1.3.12 https://github.com/sewenew/redis-plus-plus.git /tmp/redis-plus-plus \
    && cd /tmp/redis-plus-plus \
    && mkdir build && cd build \
    && cmake -DREDIS_PLUS_PLUS_CXX_STANDARD=17 .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig \
    && rm -rf /tmp/redis-plus-plus

# ── App directory ─────────────────────────────────────────────────────────────
WORKDIR /app

# Copy all source files
COPY . .

# Fix line endings for shell scripts (CRLF -> LF)
RUN sed -i 's/\r//' build.sh && chmod +x build.sh

# ── Compile the C++ binary (cloud-safe: SSE2 + AES-NI + portable BLAKE3) ────
# Requires -maes for wmmintrin.h AES-NI instructions in aes.h
# Disables SSE4.1, AVX2, AVX512 in BLAKE3 for maximum cloud container compatibility
RUN g++ -std=c++17 -O2 -msse2 -maes -fpermissive \
        -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 \
        odxt_cli.cpp aes.cpp rawdatautil.cpp ecc_x25519.cpp \
        ./c/blake_hash.cpp ./c/blake3.c ./c/blake3_dispatch.c \
        ./c/blake3_portable.c ./c/blake3_sse2.c \
        odxt_main_single_thread.cpp utils.cpp main_single_thread.cpp \
        -lgmpxx -lgmp -lredis++ -lhiredis -lpthread -o odxt-cli \
    && echo "Build succeeded!"

# ── Python dependencies ───────────────────────────────────────────────────────
RUN pip3 install --no-cache-dir \
    fastapi \
    "uvicorn[standard]" \
    python-multipart \
    PyPDF2 \
    redis

# ── Entrypoint ────────────────────────────────────────────────────────────────
COPY entrypoint.sh /entrypoint.sh
RUN sed -i 's/\r//' /entrypoint.sh && chmod +x /entrypoint.sh

EXPOSE 8001

ENTRYPOINT ["/entrypoint.sh"]
