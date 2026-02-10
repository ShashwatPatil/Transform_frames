# Firebase Firestore Migration Guide

## Overview

This migration moves configuration from local JSON files to Firebase Firestore for both `libuwb_transform` and `uwb_bridge_cpp`. The implementation supports:

- **AppConfig**: One-time fetch on startup (MQTT settings, logging config)
- **TransformConfig**: Real-time updates via Firestore listeners

## Prerequisites

### 1. Firebase C++ SDK Installation

Download the Firebase C++ SDK for Linux:

```bash
# Download from https://firebase.google.com/download/cpp
cd /tmp
wget https://dl.google.com/firebase/sdk/cpp/firebase_cpp_sdk_11.0.0.zip
unzip firebase_cpp_sdk_11.0.0.zip

# Install to /opt (or your preferred location)
sudo mkdir -p /opt/firebase_cpp_sdk
sudo cp -r firebase_cpp_sdk/* /opt/firebase_cpp_sdk/

# Verify installation
ls /opt/firebase_cpp_sdk/libs/linux/x86_64/
# Should see: libfirebase_app.a, libfirebase_firestore.a, etc.
```

### 2. Firebase Project Setup

1. Go to [Firebase Console](https://console.firebase.google.com/)
2. Create a new project or use existing one
3. Enable Firestore Database:
   - Go to "Firestore Database" in left menu
   - Click "Create database"
   - Choose production mode
   - Select your preferred region

### 3. Firestore Security Rules

Set up basic security rules in Firestore:

```javascript
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /configs/{document=**} {
      // Allow read/write for authenticated users
      // Adjust based on your security requirements
      allow read, write: if request.auth != null;
    }
  }
}
```

### 4. Environment Variables

Set the following environment variables:

```bash
export FIREBASE_PROJECT_ID="your-project-id"
export FIREBASE_API_KEY="your-api-key"
```

You can find these in Firebase Console > Project Settings > General.

## Firestore Data Structure

### Collection: `configs`

#### Document: `app_config`

Contains MQTT and logging configuration:

```json
{
  "mqtt_dual_mode": false,
  "mqtt_broker": "tcp://localhost:1883",
  "mqtt_client_id": "uwb_bridge_cpp",
  "mqtt_username": "",
  "mqtt_password": "",
  "mqtt_source_topic": "tags/#",
  "mqtt_dest_topic_prefix": "processed/",
  "mqtt_qos": 1,
  "mqtt_keepalive": 60,
  "mqtt_clean_session": true,
  "mqtt_use_ssl": false,
  "log_level": "info",
  "log_file": "logs/uwb_bridge.log",
  "log_rotation_size_mb": 10,
  "log_rotation_count": 5,
  "origin_x": 0.0,
  "origin_y": 0.0,
  "scale": 1.0,
  "rotation_rad": 0.0,
  "x_flipped": false,
  "y_flipped": false,
  "frame_id": "map",
  "output_units": "meters"
}
```

#### Document: `transform_config`

Contains transformation matrix parameters (updated in real-time):

```json
{
  "origin_x": 0.0,
  "origin_y": 0.0,
  "scale": 1.0,
  "rotation_rad": 0.0,
  "x_flipped": false,
  "y_flipped": false,
  "frame_id": "map",
  "output_units": "meters"
}
```

## Building the Project

### 1. Build libuwb_transform

```bash
cd Transform_frames/libuwb_transform
mkdir -p build && cd build

# Configure with Firebase SDK
cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk

# Build
make -j$(nproc)
```

### 2. Build uwb_bridge_cpp

```bash
cd Transform_frames/uwb_bridge_cpp
mkdir -p build && cd build

# Configure with Firebase SDK
cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk

# Build
make -j$(nproc)
```

## Running the Application

### Local JSON Mode (Default)

```bash
# Use local JSON configuration file
./bin/uwb_bridge -c config/app_config.json
```

### Firestore Mode (New)

```bash
# Set environment variables
export FIREBASE_PROJECT_ID="your-project-id"
export FIREBASE_API_KEY="your-api-key"

# Run with Firestore
./bin/uwb_bridge --firestore
```

## Key Changes

### 1. Thread-Safe FloorplanTransformer

The `FloorplanTransformer` class now uses `std::shared_mutex` for thread-safe configuration updates:

- **Read operations** (`transformToPixel`, `transformToUWB`): Use shared lock (multiple concurrent readers)
- **Write operations** (`updateConfig`): Use exclusive lock (single writer, blocks all readers)

### 2. FirestoreManager

New class that handles:
- Firebase initialization
- One-time AppConfig fetch using `Get()`
- Real-time TransformConfig updates using `AddSnapshotListener()`

### 3. Dual Mode Operation

The application now supports two modes:
- **Local JSON**: Traditional file-based configuration (default)
- **Firestore**: Cloud-based configuration with real-time updates

## Testing Real-Time Updates

1. Start the application in Firestore mode:
   ```bash
   ./bin/uwb_bridge --firestore
   ```

2. Update the `transform_config` document in Firestore Console

3. Watch the logs - you should see:
   ```
   [INFO] Updated transform matrix from Firestore
   [DEBUG] New config - Origin: (x, y), Scale: s, Rotation: r rad
   ```

4. The transformer will automatically use the new configuration without restarting

## Troubleshooting

### Firebase SDK Not Found

```
CMake Warning: Firebase C++ SDK not found at /opt/firebase_cpp_sdk
```

**Solution**: Set the correct path when running cmake:
```bash
cmake .. -DFIREBASE_CPP_SDK_DIR=/path/to/your/firebase_cpp_sdk
```

### Firestore Connection Failed

```
[ERROR] Failed to create Firebase App
```

**Solution**: Check environment variables:
```bash
echo $FIREBASE_PROJECT_ID
echo $FIREBASE_API_KEY
```

### Linking Errors

```
undefined reference to `firebase::...`
```

**Solution**: Make sure Firebase libraries are in the link path:
```bash
export LD_LIBRARY_PATH=/opt/firebase_cpp_sdk/libs/linux/x86_64:$LD_LIBRARY_PATH
```

## Migration Checklist

- [ ] Install Firebase C++ SDK
- [ ] Create Firebase project
- [ ] Enable Firestore
- [ ] Set up security rules
- [ ] Create `configs/app_config` document
- [ ] Create `configs/transform_config` document
- [ ] Set environment variables
- [ ] Build libuwb_transform with Firebase
- [ ] Build uwb_bridge_cpp with Firebase
- [ ] Test local JSON mode (backwards compatibility)
- [ ] Test Firestore mode
- [ ] Test real-time transform updates
- [ ] Update systemd service (if applicable)

## Backward Compatibility

The local JSON mode is fully preserved. Existing deployments can continue using:

```bash
./bin/uwb_bridge -c config/app_config.json
```

No changes required to existing configurations or workflows.
