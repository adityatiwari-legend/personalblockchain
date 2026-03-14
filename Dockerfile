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
    libboost-thread1.74.0 \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Run as non-root user
RUN useradd -m -s /bin/bash blockchain
WORKDIR /app

COPY --from=builder /app/build/blockchain_node .

# Create persistent data directory
RUN mkdir -p /app/data && chown blockchain:blockchain /app/data

USER blockchain

EXPOSE 5000 8000

HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8000/health || exit 1

ENTRYPOINT ["./blockchain_node"]
