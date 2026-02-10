# Docker Deployment with Firebase Service Account

## Overview

This guide shows how to deploy the UWB Bridge application using Docker with Firebase service account authentication.

## Step 1: Get Your Service Account Credentials

1. Go to [Firebase Console](https://console.firebase.google.com/) → Your Project → Project Settings → Service Accounts
2. Click "Generate New Private Key" and download the JSON file
3. Save it as `firebase-credentials.json` in your project directory

## Step 2: Save Your Credentials

Create a file named `firebase-credentials.json` with your service account credentials downloaded from Firebase Console:

```json
{
  "type": "service_account",
  "project_id": "YOUR_PROJECT_ID",
  "private_key_id": "YOUR_PRIVATE_KEY_ID",
  "private_key": "-----BEGIN PRIVATE KEY-----\nYOUR_PRIVATE_KEY\n-----END PRIVATE KEY-----\n",
  "client_email": "YOUR_SERVICE_ACCOUNT_EMAIL",
  "client_id": "YOUR_CLIENT_ID",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://oauth2.googleapis.com/token",
  "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
  "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/YOUR_SERVICE_ACCOUNT_EMAIL",
  "universe_domain": "googleapis.com"
}
```

**⚠️ Security**: Keep this file secure! Add to `.gitignore`:

```bash
echo "firebase-credentials.json" >> .gitignore
```

## Step 2: Create Dockerfile

Create `Dockerfile` in `Transform_frames/uwb_bridge_cpp/`:

```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    wget \
    unzip \
    git \
    libeigen3-dev \
    libssl-dev \
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
COPY Transform_frames /app/Transform_frames

# Build libuwb_transform
WORKDIR /app/Transform_frames/libuwb_transform/build
RUN cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk && \
    make -j$(nproc)

# Build uwb_bridge_cpp
WORKDIR /app/Transform_frames/uwb_bridge_cpp/build
RUN cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk && \
    make -j$(nproc)

# Create directories for credentials and logs
RUN mkdir -p /app/credentials /app/logs

# Set environment variables
ENV LD_LIBRARY_PATH=/opt/firebase_cpp_sdk/libs/linux/x86_64:$LD_LIBRARY_PATH
ENV GOOGLE_APPLICATION_CREDENTIALS=/app/credentials/firebase-credentials.json

# Set working directory for runtime
WORKDIR /app/Transform_frames/uwb_bridge_cpp/build

# Run the application
CMD ["./bin/uwb_bridge", "--firestore"]
```

## Step 3: Create docker-compose.yml

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  uwb-bridge:
    build:
      context: .
      dockerfile: Transform_frames/uwb_bridge_cpp/Dockerfile
    container_name: uwb-bridge
    restart: unless-stopped
    
    # Mount the credentials file
    volumes:
      - ./firebase-credentials.json:/app/credentials/firebase-credentials.json:ro
      - ./logs:/app/logs
    
    # Environment variables
    environment:
      - GOOGLE_APPLICATION_CREDENTIALS=/app/credentials/firebase-credentials.json
      - LD_LIBRARY_PATH=/opt/firebase_cpp_sdk/libs/linux/x86_64
    
    # Network mode (if MQTT broker is on host)
    network_mode: host
    
    # Or use custom network for internal MQTT
    # networks:
    #   - uwb-network

# networks:
#   uwb-network:
#     driver: bridge
```

## Step 4: Build and Run

```bash
# Build the Docker image
docker-compose build

# Run the container
docker-compose up -d

# View logs
docker-compose logs -f uwb-bridge

# Stop the container
docker-compose down
```

## Alternative: Docker Run Command

If not using docker-compose:

```bash
# Build image
docker build -t uwb-bridge -f Transform_frames/uwb_bridge_cpp/Dockerfile .

# Run container with mounted credentials
docker run -d \
  --name uwb-bridge \
  --network host \
  -v $(pwd)/firebase-credentials.json:/app/credentials/firebase-credentials.json:ro \
  -v $(pwd)/logs:/app/logs \
  -e GOOGLE_APPLICATION_CREDENTIALS=/app/credentials/firebase-credentials.json \
  uwb-bridge
```

## How It Works

### Authentication Flow

1. **Container starts**
2. **Environment variable** `GOOGLE_APPLICATION_CREDENTIALS` points to your mounted credentials file
3. **FirestoreManager::initialize()** checks for this environment variable
4. **Reads credentials file** and extracts `project_id: "nova-40708"`
5. **Firebase C++ SDK** uses the service account for authentication
6. **Connects to Firestore** in your `nova-40708` project

### Code Flow

```cpp
// In FirestoreManager.cpp
bool FirestoreManager::initialize() {
    // Check for GOOGLE_APPLICATION_CREDENTIALS
    const char* creds_path = std::getenv("GOOGLE_APPLICATION_CREDENTIALS");
    
    if (creds_path) {
        // Use service account file
        return initializeWithServiceAccount(creds_path);
    }
    
    // Fallback to API key authentication
    // ...
}

bool FirestoreManager::initializeWithServiceAccount(const std::string& credentials_path) {
    // 1. Set environment variable for Firebase SDK
    setenv("GOOGLE_APPLICATION_CREDENTIALS", credentials_path.c_str(), 1);
    
    // 2. Read project_id from credentials file
    std::ifstream creds_file(credentials_path);
    nlohmann::json creds_json;
    creds_file >> creds_json;
    project_id_ = creds_json["project_id"].get<std::string>();
    // project_id_ = "nova-40708"
    
    // 3. Initialize Firebase with service account
    firebase::AppOptions options;
    options.set_project_id(project_id_.c_str());
    app_ = firebase::App::Create(options);
    
    // 4. Get Firestore instance
    db_ = firebase::firestore::Firestore::GetInstance(app_);
}
```

## Security Best Practices

### 1. File Permissions

```bash
# Restrict credentials file permissions
chmod 600 firebase-credentials.json
chown root:root firebase-credentials.json  # If running as root
```

### 2. Docker Secrets (Production)

For production, use Docker secrets instead of mounting files:

```yaml
version: '3.8'

services:
  uwb-bridge:
    # ... other config ...
    secrets:
      - firebase_credentials
    environment:
      - GOOGLE_APPLICATION_CREDENTIALS=/run/secrets/firebase_credentials

secrets:
  firebase_credentials:
    file: ./firebase-credentials.json
```

### 3. Kubernetes Secrets (K8s)

```bash
# Create secret
kubectl create secret generic firebase-credentials \
  --from-file=firebase-credentials.json

# Use in deployment
apiVersion: v1
kind: Pod
spec:
  containers:
  - name: uwb-bridge
    volumeMounts:
    - name: firebase-creds
      mountPath: /app/credentials
      readOnly: true
  volumes:
  - name: firebase-creds
    secret:
      secretName: firebase-credentials
```

## Troubleshooting

### Credentials Not Found

```
[ERROR] Failed to open credentials file: /app/credentials/firebase-credentials.json
```

**Solution**: Verify mount path matches:
```bash
docker exec uwb-bridge ls -la /app/credentials/
```

### Permission Denied

```
[ERROR] Failed to open credentials file: Permission denied
```

**Solution**: Check file permissions in container:
```bash
docker exec uwb-bridge ls -la /app/credentials/firebase-credentials.json
```

### Invalid Credentials

```
[ERROR] Failed to create Firebase App
```

**Solution**: Verify JSON is valid:
```bash
docker exec uwb-bridge cat /app/credentials/firebase-credentials.json | python3 -m json.tool
```

## Monitoring

### View Application Logs

```bash
# Follow logs
docker-compose logs -f uwb-bridge

# Look for these messages
[INFO] Initializing Firebase with service account: /app/credentials/firebase-credentials.json
[INFO] Project ID from credentials: nova-40708
[INFO] Firebase App created successfully with service account
[INFO] Firestore initialized successfully
```

### Check Container Status

```bash
# Container status
docker-compose ps

# Resource usage
docker stats uwb-bridge

# Container details
docker inspect uwb-bridge
```

## Next Steps

After Docker is running successfully:

1. **Configure Firestore** - See FIRESTORE_DATA_CONFIGURATION.md
2. **Set up database structure** - Create collections and documents
3. **Test real-time updates** - Modify transform config in Firestore Console
4. **Monitor performance** - Check logs and Firebase Console

## Summary

- ✅ Service account file mounted to container
- ✅ `GOOGLE_APPLICATION_CREDENTIALS` environment variable set
- ✅ FirestoreManager automatically detects and uses service account
- ✅ Secure credential handling with read-only mount
- ✅ Project ID extracted from credentials: `nova-40708`
