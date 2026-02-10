# Quick Start Guide - Docker Deployment

## Your Setup

**Firebase Project**: `nova-40708`  
**Service Account**: `firebase-adminsdk-fbsvc@nova-40708.iam.gserviceaccount.com`

## 1. Save Your Credentials

Create `firebase-credentials.json` in the `Transform_frames/` directory:

```bash
cd Transform_frames
nano firebase-credentials.json
```

Paste your service account credentials and save (Ctrl+X, Y, Enter).

**⚠️ Important**: Keep this file secure!

```bash
chmod 600 firebase-credentials.json
echo "firebase-credentials.json" >> .gitignore
```

## 2. Configure Firestore Database

### Option A: Using Python Script (Recommended)

Install Python Firebase Admin SDK:
```bash
pip install firebase-admin
```

Create `setup_firestore_nova.py`:
```python
import firebase_admin
from firebase_admin import credentials, firestore

# Initialize with your service account
cred = credentials.Certificate('firebase-credentials.json')
firebase_admin.initialize_app(cred)
db = firestore.client()

# MQTT Configuration - UPDATE THESE VALUES FOR YOUR SETUP
app_config = {
    "mqtt_dual_mode": False,
    "mqtt_broker": "tcp://localhost:1883",      # ← Your MQTT broker address
    "mqtt_client_id": "uwb_bridge_nova",
    "mqtt_username": "",                        # ← Your MQTT username (if required)
    "mqtt_password": "",                        # ← Your MQTT password (if required)
    "mqtt_source_topic": "tags/#",              # ← Topic to subscribe to
    "mqtt_dest_topic_prefix": "processed/",     # ← Prefix for output topics
    "mqtt_qos": 1,
    "mqtt_keepalive": 60,
    "mqtt_clean_session": True,
    "mqtt_use_ssl": False,
    "log_level": "info",
    "log_file": "logs/uwb_bridge.log",
    "log_rotation_size_mb": 10,
    "log_rotation_count": 5
}

# Transform Configuration - UPDATE THESE VALUES FOR YOUR FLOORPLAN
transform_config = {
    "origin_x": 0.0,        # ← X coordinate of floorplan origin (mm)
    "origin_y": 0.0,        # ← Y coordinate of floorplan origin (mm)
    "scale": 1.0,           # ← Pixels per mm
    "rotation_rad": 0.0,    # ← Rotation in radians (0 = no rotation)
    "x_flipped": False,     # ← Set true if X axis is inverted
    "y_flipped": False,     # ← Set true if Y axis is inverted
    "frame_id": "map",
    "output_units": "meters"
}

# Create documents
db.collection('configs').document('app_config').set(app_config)
print("✓ Created app_config in Firestore")

db.collection('configs').document('transform_config').set(transform_config)
print("✓ Created transform_config in Firestore")

print("\n✅ Firestore configured successfully!")
print("Project: nova-40708")
print("Collection: configs")
print("Documents: app_config, transform_config")
```

Run the script:
```bash
python3 setup_firestore_nova.py
```

### Option B: Manual Setup in Firebase Console

1. Go to https://console.firebase.google.com/project/nova-40708/firestore
2. Click "Start collection"
3. Collection ID: `configs`
4. Create document `app_config` with fields from above
5. Create document `transform_config` with fields from above

See [FIRESTORE_DATA_CONFIGURATION.md](FIRESTORE_DATA_CONFIGURATION.md) for detailed instructions.

## 3. Build and Run with Docker

```bash
# Make sure you're in Transform_frames directory
cd Transform_frames

# Verify files are present
ls -la
# Should see: Dockerfile, docker-compose.yml, firebase-credentials.json

# Build the Docker image (takes 5-10 minutes)
docker-compose build

# Run the container
docker-compose up -d

# View logs
docker-compose logs -f uwb-bridge-nova
```

## 4. Verify It's Working

### Check Logs

You should see:
```
[INFO] Initializing Firebase with service account: /app/credentials/firebase-credentials.json
[INFO] Project ID from credentials: nova-40708
[INFO] Firebase App created successfully with service account
[INFO] Firestore initialized successfully
[INFO] Fetching configuration from Firestore...
[INFO] Successfully fetched AppConfig from Firestore
[INFO] MQTT Broker: tcp://localhost:1883
[INFO] Source Topic: tags/#
[INFO] Starting real-time listener for TransformConfig...
[INFO] TransformConfig listener started successfully
[INFO] UWB Bridge running. Press Ctrl+C to stop.
```

### Check Container Status

```bash
# Container status
docker-compose ps

# Should show: uwb-bridge-nova   Up   

# Resource usage
docker stats uwb-bridge-nova
```

## 5. Test Real-Time Configuration Updates

### Update Transform Configuration

1. Go to https://console.firebase.google.com/project/nova-40708/firestore
2. Navigate to `configs` → `transform_config`
3. Click on any field (e.g., `rotation_rad`)
4. Change the value (e.g., from `0.0` to `1.5708`)
5. Click "Update"

### Watch Logs

```bash
docker-compose logs -f uwb-bridge-nova
```

You should immediately see:
```
[INFO] Updated transform matrix from Firestore
[DEBUG] New config - Origin: (0, 0), Scale: 1, Rotation: 1.5708 rad
```

**🎉 No restart needed! The configuration updates in real-time.**

## Common Commands

```bash
# Start container
docker-compose up -d

# Stop container
docker-compose down

# View logs
docker-compose logs -f uwb-bridge-nova

# Restart container
docker-compose restart uwb-bridge-nova

# Rebuild after code changes
docker-compose build --no-cache

# Shell into container (for debugging)
docker exec -it uwb-bridge-nova /bin/bash

# Check if credentials are mounted correctly
docker exec uwb-bridge-nova cat /app/credentials/firebase-credentials.json
```

## Troubleshooting

### Problem: Credentials not found

```
[ERROR] Failed to open credentials file
```

**Solution**: Check the file is present
```bash
ls -la firebase-credentials.json
docker exec uwb-bridge-nova ls -la /app/credentials/
```

### Problem: MQTT connection failed

```
[ERROR] Failed to connect to MQTT broker
```

**Solution**: 
- Verify MQTT broker is running
- Check `mqtt_broker` value in Firestore `app_config`
- If broker is on host machine, ensure `network_mode: host` in docker-compose.yml

### Problem: Firestore connection failed

```
[ERROR] Failed to create Firebase App
```

**Solution**:
- Verify credentials file is valid JSON: `cat firebase-credentials.json | python3 -m json.tool`
- Check project ID matches: `nova-40708`
- Ensure Firestore is enabled in Firebase Console

### Problem: Config documents not found

```
[ERROR] AppConfig document does not exist in Firestore
```

**Solution**: Run the Python setup script again or create documents manually in Firebase Console

## Next Steps

1. **Configure Your MQTT Settings**
   - Edit `mqtt_broker` in Firestore: `configs/app_config`
   - Set your actual MQTT broker IP/hostname
   - Configure your topic patterns

2. **Configure Your Transform Parameters**
   - Edit `origin_x`, `origin_y` in Firestore: `configs/transform_config`
   - Set your floorplan coordinate origin
   - Adjust `scale` and `rotation_rad` as needed

3. **Test with Real UWB Data**
   - Publish test data to your MQTT source topic
   - Monitor the output topic for transformed data
   - Adjust transform parameters in real-time

4. **Production Deployment**
   - Set up proper MQTT broker
   - Configure SSL/TLS if needed
   - Set up monitoring and logging
   - Configure restart policies

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                     Your Computer                       │
│                                                         │
│  ┌─────────────────────────────────────────────────┐  │
│  │          Docker Container                       │  │
│  │  ┌─────────────────────────────────────────┐   │  │
│  │  │      UWB Bridge Application             │   │  │
│  │  │                                         │   │  │
│  │  │  ┌──────────────┐  ┌────────────────┐ │   │  │
│  │  │  │ MQTT Handler │  │ Transformer    │ │   │  │
│  │  │  └──────┬───────┘  └────────┬───────┘ │   │  │
│  │  │         │                   │          │   │  │
│  │  │         └───────────┬───────┘          │   │  │
│  │  │                     │                  │   │  │
│  │  │         ┌───────────▼──────────┐       │   │  │
│  │  │         │ FirestoreManager     │       │   │  │
│  │  │         │ (Service Account)    │       │   │  │
│  │  │         └───────────┬──────────┘       │   │  │
│  │  └─────────────────────┼──────────────────┘   │  │
│  │                        │                      │  │
│  │  Mounted Credentials   │                      │  │
│  │  firebase-credentials.json                    │  │
│  └────────────────────────┼───────────────────────┘  │
└─────────────────────────┼───────────────────────────┘
                          │
                    Internet
                          │
              ┌───────────▼───────────┐
              │  Firebase Cloud       │
              │  Project: nova-40708  │
              │                       │
              │  ┌─────────────────┐ │
              │  │   Firestore DB  │ │
              │  │                 │ │
              │  │  Collection:    │ │
              │  │    configs      │ │
              │  │  ├─ app_config  │ │
              │  │  └─ transform_  │ │
              │  │     config      │ │
              │  └─────────────────┘ │
              └─────────────────────┘
```

## File Checklist

In your `Transform_frames/` directory, you should have:

- ✅ `firebase-credentials.json` (your service account file)
- ✅ `Dockerfile` (builds the container image)
- ✅ `docker-compose.yml` (orchestrates the container)
- ✅ `libuwb_transform/` (transform library source code)
- ✅ `uwb_bridge_cpp/` (bridge application source code)
- ✅ `logs/` directory (created automatically, contains logs)

## Summary

You now have:
- ✅ Docker container running UWB Bridge
- ✅ Service account authentication with Firebase
- ✅ Firestore database configured with your settings
- ✅ Real-time configuration updates enabled
- ✅ MQTT bridge processing UWB data
- ✅ Coordinate transformation applied

Update configurations in Firestore Console anytime - changes apply immediately!

For more details, see:
- [DOCKER_DEPLOYMENT.md](DOCKER_DEPLOYMENT.md) - Complete Docker guide
- [FIRESTORE_DATA_CONFIGURATION.md](FIRESTORE_DATA_CONFIGURATION.md) - Database setup details
- [FIREBASE_README.md](FIREBASE_README.md) - Comprehensive Firebase guide
