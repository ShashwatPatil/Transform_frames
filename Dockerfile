FROM ubuntu:22.04

# Avoid prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    unzip \
    git \
    libeigen3-dev \
    libssl-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Firebase C++ SDK
RUN mkdir -p /opt/firebase_cpp_sdk && \
    cd /tmp && \
    wget -q https://dl.google.com/firebase/sdk/cpp/firebase_cpp_sdk_11.0.0.zip && \
    unzip -q firebase_cpp_sdk_11.0.0.zip && \
    cp -r firebase_cpp_sdk/* /opt/firebase_cpp_sdk/ && \
    rm -rf firebase_cpp_sdk_11.0.0.zip firebase_cpp_sdk

# Set working directory
WORKDIR /app

# Copy source code
COPY libuwb_transform /app/libuwb_transform
COPY uwb_bridge_cpp /app/uwb_bridge_cpp

# Build libuwb_transform
WORKDIR /app/libuwb_transform
RUN mkdir -p build && cd build && \
    cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk && \
    make -j$(nproc) && \
    make install

# Build uwb_bridge_cpp
WORKDIR /app/uwb_bridge_cpp
RUN mkdir -p build && cd build && \
    cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk && \
    make -j$(nproc)

# Create directories for credentials and logs
RUN mkdir -p /app/credentials /app/logs

# Set environment variables
ENV LD_LIBRARY_PATH=/opt/firebase_cpp_sdk/libs/linux/x86_64:/usr/local/lib:$LD_LIBRARY_PATH
ENV GOOGLE_APPLICATION_CREDENTIALS=/app/credentials/firebase-credentials.json

# Set working directory for runtime
WORKDIR /app/uwb_bridge_cpp/build

# Expose for debugging (optional)
EXPOSE 1883

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=40s --retries=3 \
  CMD pgrep -x uwb_bridge || exit 1

# Run the application
CMD ["./bin/uwb_bridge", "--firestore"]
