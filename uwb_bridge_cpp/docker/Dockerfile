FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libeigen3-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Create build directory
WORKDIR /build

# Copy source code
COPY . /build/

# Build the application
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd --system --no-create-home --shell /bin/false uwb

# Create directories
RUN mkdir -p /opt/uwb_bridge/bin \
    /opt/uwb_bridge/config \
    /var/log/uwb_bridge && \
    chown -R uwb:uwb /opt/uwb_bridge /var/log/uwb_bridge

# Copy binary from builder
COPY --from=builder /build/build/bin/uwb_bridge /opt/uwb_bridge/bin/

# Copy default configuration
COPY --from=builder /build/config/app_config.json /opt/uwb_bridge/config/

# Set working directory
WORKDIR /opt/uwb_bridge

# Switch to non-root user
USER uwb

# Expose health check (if you add HTTP endpoint later)
# EXPOSE 8080

# Set entrypoint
ENTRYPOINT ["/opt/uwb_bridge/bin/uwb_bridge"]
CMD ["-c", "/opt/uwb_bridge/config/app_config.json"]
