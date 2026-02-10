# Firestore Data Configuration Guide

## Overview

This guide shows **exactly where** to configure your Firestore database for the UWB Bridge application and what data goes where.

## Your Firebase Project

- **Project ID**: `nova-40708`
- **Project URL**: https://console.firebase.google.com/project/nova-40708
- **Firestore URL**: https://console.firebase.google.com/project/nova-40708/firestore

## Database Structure

The application reads from TWO documents in Firestore:

```
nova-40708 (Your Project)
  └── Firestore Database
      └── Collection: "configs"
          ├── Document: "app_config"       ← MQTT, logging settings
          └── Document: "transform_config" ← Transform matrix parameters
```

## Step-by-Step Setup

### Step 1: Access Firestore Console

1. Go to https://console.firebase.google.com/
2. Select your project: **nova-40708**
3. Click "Firestore Database" in the left menu
4. If not created, click "Create database" → Production mode → Select region

### Step 2: Create Collection "configs"

1. Click "Start collection" (or "+ Add collection" if you have data)
2. Collection ID: **`configs`**
3. Click "Next"

### Step 3: Create Document "app_config"

This document contains MQTT broker settings and application configuration.

#### In Firestore Console:

1. Document ID: **`app_config`**
2. Add these fields (click "+ Add field" for each):

| Field Name | Type | Value | Description |
|------------|------|-------|-------------|
| `mqtt_dual_mode` | boolean | `false` | Whether to use separate source/dest brokers |
| `mqtt_broker` | string | `tcp://localhost:1883` | Your MQTT broker address |
| `mqtt_client_id` | string | `uwb_bridge_cpp` | MQTT client identifier |
| `mqtt_username` | string | *(leave empty or your username)* | MQTT username |
| `mqtt_password` | string | *(leave empty or your password)* | MQTT password |
| `mqtt_source_topic` | string | `tags/#` | MQTT topic to subscribe to |
| `mqtt_dest_topic_prefix` | string | `processed/` | Prefix for published topics |
| `mqtt_qos` | number | `1` | MQTT QoS level (0, 1, or 2) |
| `mqtt_keepalive` | number | `60` | Keepalive interval in seconds |
| `mqtt_clean_session` | boolean | `true` | Clean session flag |
| `mqtt_use_ssl` | boolean | `false` | Enable SSL/TLS |
| `log_level` | string | `info` | Logging level (debug, info, warn, error) |
| `log_file` | string | `logs/uwb_bridge.log` | Log file path |
| `log_rotation_size_mb` | number | `10` | Log rotation size in MB |
| `log_rotation_count` | number | `5` | Number of rotated logs to keep |

3. Click "Save"

#### Example Values for Your Setup:

```
Collection: configs
Document ID: app_config

Fields:
  mqtt_dual_mode: false
  mqtt_broker: "tcp://192.168.1.100:1883"  ← YOUR MQTT BROKER IP
  mqtt_client_id: "uwb_bridge_nova"
  mqtt_username: ""
  mqtt_password: ""
  mqtt_source_topic: "uwb/tags/#"          ← YOUR INPUT TOPIC
  mqtt_dest_topic_prefix: "uwb/processed/" ← YOUR OUTPUT TOPIC PREFIX
  mqtt_qos: 1
  mqtt_keepalive: 60
  mqtt_clean_session: true
  mqtt_use_ssl: false
  log_level: "info"
  log_file: "logs/uwb_bridge.log"
  log_rotation_size_mb: 10
  log_rotation_count: 5
```

### Step 4: Create Document "transform_config"

This document contains the coordinate transformation parameters (updated in real-time!).

#### In Firestore Console:

1. Click "+ Add document" in the `configs` collection
2. Document ID: **`transform_config`**
3. Add these fields:

| Field Name | Type | Value | Description |
|------------|------|-------|-------------|
| `origin_x` | number | `0.0` | X coordinate of image origin in UWB frame (mm) |
| `origin_y` | number | `0.0` | Y coordinate of image origin in UWB frame (mm) |
| `scale` | number | `1.0` | Pixels per UWB unit (pixels/mm) |
| `rotation_rad` | number | `0.0` | Rotation angle in radians |
| `x_flipped` | boolean | `false` | Whether X axis is flipped |
| `y_flipped` | boolean | `false` | Whether Y axis is flipped |
| `frame_id` | string | `map` | Frame identifier for output |
| `output_units` | string | `meters` | Output units: "meters", "millimeters", or "pixels" |

4. Click "Save"

#### Example Transformation Values:

For a typical floorplan setup:

```
Collection: configs
Document ID: transform_config

Fields:
  origin_x: 1000.0        ← Top-left corner X in UWB coordinates (mm)
  origin_y: 2000.0        ← Top-left corner Y in UWB coordinates (mm)
  scale: 0.5              ← 0.5 pixels per millimeter
  rotation_rad: 1.5708    ← 90 degrees (π/2 radians)
  x_flipped: false
  y_flipped: true         ← Flip Y axis if needed
  frame_id: "map"
  output_units: "meters"
```

## Visual Guide - Firestore Console

### Creating app_config:

```
┌─────────────────────────────────────────────────────┐
│ Firestore Database                                  │
├─────────────────────────────────────────────────────┤
│ ┌─ Collection: configs ───────────────────────┐    │
│ │                                              │    │
│ │ ┌─ Document: app_config ─────────────────┐ │    │
│ │ │                                         │ │    │
│ │ │ Field             Type      Value       │ │    │
│ │ │ ─────────────────────────────────────── │ │    │
│ │ │ mqtt_broker       string    tcp://...   │ │    │
│ │ │ mqtt_source_topic string    tags/#      │ │    │
│ │ │ mqtt_qos          number    1           │ │    │
│ │ │ log_level         string    info        │ │    │
│ │ │ ...                                     │ │    │
│ │ └─────────────────────────────────────────┘ │    │
│ │                                              │    │
│ │ ┌─ Document: transform_config ───────────┐ │    │
│ │ │                                         │ │    │
│ │ │ Field          Type      Value          │ │    │
│ │ │ ─────────────────────────────────────── │ │    │
│ │ │ origin_x       number    1000.0         │ │    │
│ │ │ origin_y       number    2000.0         │ │    │
│ │ │ scale          number    0.5            │ │    │
│ │ │ rotation_rad   number    1.5708         │ │    │
│ │ │ ...                                     │ │    │
│ │ └─────────────────────────────────────────┘ │    │
│ └──────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘
```

## How the Application Reads This Data

### Code Mapping

```cpp
// When you start: ./bin/uwb_bridge --firestore

// 1. FirestoreManager::initialize()
//    ↓ Uses your service account credentials
//    ↓ Connects to project "nova-40708"

// 2. FirestoreManager::fetchAppConfig()
//    ↓ Reads document: configs/app_config
//    ↓ Parses fields into AppConfig structure

AppConfig config;
config.mqtt.source_broker.broker_address = "tcp://..."  // from mqtt_broker field
config.mqtt.source_broker.source_topic = "tags/#"       // from mqtt_source_topic field
config.log_level = "info"                               // from log_level field
// ... all other fields from app_config document

// 3. BridgeCore initializes with config

// 4. FirestoreManager::startTransformListener()
//    ↓ Sets up real-time listener on: configs/transform_config
//    ↓ Automatically updates when you change values in Firestore

transformer->updateConfig({
    .origin_x = 1000.0,     // from origin_x field
    .origin_y = 2000.0,     // from origin_y field
    .scale = 0.5,           // from scale field
    .rotation_rad = 1.5708, // from rotation_rad field
    // ...
});
```

### Where Each Field is Used

| Firestore Field | Used By | Purpose |
|----------------|---------|---------|
| `mqtt_broker` | MqttHandler | Connect to MQTT broker |
| `mqtt_source_topic` | MqttHandler | Subscribe to this topic pattern |
| `mqtt_dest_topic_prefix` | MqttHandler | Prefix for publishing transformed data |
| `origin_x`, `origin_y` | FloorplanTransformer | Define coordinate system origin |
| `scale` | FloorplanTransformer | Convert between UWB units and pixels |
| `rotation_rad` | FloorplanTransformer | Rotate coordinate system |
| `log_level` | spdlog | Set logging verbosity |

## Testing Your Configuration

### 1. Test MQTT Settings

Make sure your MQTT broker is accessible:

```bash
# Install mosquitto clients
sudo apt install mosquitto-clients

# Test subscribing to your topic
mosquitto_sub -h YOUR_MQTT_BROKER -t "tags/#" -v

# Test publishing
mosquitto_pub -h YOUR_MQTT_BROKER -t "tags/test" -m "test message"
```

### 2. Start Application

```bash
# With Docker
docker-compose up

# Or directly
export GOOGLE_APPLICATION_CREDENTIALS=/path/to/firebase-credentials.json
./bin/uwb_bridge --firestore
```

### 3. Verify in Logs

```
[INFO] Initializing Firebase with service account: /path/to/credentials
[INFO] Project ID from credentials: nova-40708
[INFO] Firestore initialized successfully
[INFO] Fetching configuration from Firestore...
[INFO] Successfully fetched AppConfig from Firestore
[INFO] MQTT Broker: tcp://192.168.1.100:1883
[INFO] Source Topic: uwb/tags/#
[INFO] Starting real-time listener for TransformConfig...
[INFO] TransformConfig listener started successfully
```

### 4. Test Real-Time Updates

1. Keep application running
2. Go to Firestore Console
3. Edit `configs/transform_config`
4. Change `rotation_rad` from `0.0` to `1.5708`
5. Click "Update"
6. Watch application logs:

```
[INFO] Updated transform matrix from Firestore
[DEBUG] New config - Origin: (1000, 2000), Scale: 0.5, Rotation: 1.5708 rad
```

## Quick Setup Script

Use Python to quickly populate your Firestore:

```python
import firebase_admin
from firebase_admin import credentials, firestore

# Initialize with your service account
cred = credentials.Certificate('firebase-credentials.json')
firebase_admin.initialize_app(cred)
db = firestore.client()

# Create app_config
app_config = {
    "mqtt_dual_mode": False,
    "mqtt_broker": "tcp://192.168.1.100:1883",  # ← YOUR BROKER
    "mqtt_client_id": "uwb_bridge_nova",
    "mqtt_username": "",
    "mqtt_password": "",
    "mqtt_source_topic": "uwb/tags/#",           # ← YOUR INPUT TOPIC
    "mqtt_dest_topic_prefix": "uwb/processed/",  # ← YOUR OUTPUT PREFIX
    "mqtt_qos": 1,
    "mqtt_keepalive": 60,
    "mqtt_clean_session": True,
    "mqtt_use_ssl": False,
    "log_level": "info",
    "log_file": "logs/uwb_bridge.log",
    "log_rotation_size_mb": 10,
    "log_rotation_count": 5
}

db.collection('configs').document('app_config').set(app_config)
print("✓ Created app_config")

# Create transform_config
transform_config = {
    "origin_x": 1000.0,     # ← YOUR FLOORPLAN ORIGIN X
    "origin_y": 2000.0,     # ← YOUR FLOORPLAN ORIGIN Y
    "scale": 0.5,           # ← YOUR SCALE FACTOR
    "rotation_rad": 1.5708, # ← YOUR ROTATION (90° = π/2)
    "x_flipped": False,
    "y_flipped": True,
    "frame_id": "map",
    "output_units": "meters"
}

db.collection('configs').document('transform_config').set(transform_config)
print("✓ Created transform_config")

print("\n✓ Firestore configured for project: nova-40708")
print("You can now run: ./bin/uwb_bridge --firestore")
```

Save as `setup_firestore.py` and run:

```bash
pip install firebase-admin
python3 setup_firestore.py
```

## Summary

### What You Need to Configure:

1. **In Firebase Console** (https://console.firebase.google.com/project/nova-40708/firestore):
   - Create collection `configs`
   - Create document `app_config` with MQTT and logging settings
   - Create document `transform_config` with transformation parameters

2. **Update These Values for Your Setup**:
   - `mqtt_broker`: Your MQTT broker IP/hostname
   - `mqtt_source_topic`: Your UWB data topic
   - `mqtt_dest_topic_prefix`: Where to publish transformed data
   - `origin_x`, `origin_y`: Your floorplan coordinate origin
   - `scale`: Your pixels-per-mm scale factor
   - `rotation_rad`: Your coordinate system rotation

3. **Mount Your Credentials**:
   - Save service account JSON as `firebase-credentials.json`
   - Set `GOOGLE_APPLICATION_CREDENTIALS` environment variable
   - Or mount to Docker container

4. **Start Application**:
   - Run with `--firestore` flag
   - Application reads from Firestore automatically
   - Real-time updates work immediately

### Where Data Comes From:

- **MQTT Data**: Comes from your UWB system → `mqtt_source_topic`
- **Transform Params**: Stored in Firestore → `transform_config`
- **Output**: Published to MQTT → `mqtt_dest_topic_prefix`

The application acts as a **bridge** between your UWB MQTT stream and Firestore configuration!
