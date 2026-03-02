# ---- Build Stage ----
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    libboost-system-dev \
    libboost-thread-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source
COPY CMakeLists.txt .
COPY include/ include/
COPY src/ src/

# Build
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ---- Runtime Stage ----
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    libboost-system1.74.0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/blockchain_node .

EXPOSE 5000 8000

ENTRYPOINT ["./blockchain_node"]
