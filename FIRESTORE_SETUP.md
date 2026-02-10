# Firestore Configuration Example

## Python Script to Initialize Firestore Documents

```python
import firebase_admin
from firebase_admin import credentials, firestore

# Initialize Firebase Admin SDK
cred = credentials.Certificate('path/to/serviceAccountKey.json')
firebase_admin.initialize_app(cred)

db = firestore.client()

# Create app_config document
app_config = {
    # MQTT Configuration
    "mqtt_dual_mode": False,
    "mqtt_broker": "tcp://localhost:1883",
    "mqtt_client_id": "uwb_bridge_cpp",
    "mqtt_username": "",
    "mqtt_password": "",
    "mqtt_source_topic": "tags/#",
    "mqtt_dest_topic_prefix": "processed/",
    "mqtt_qos": 1,
    "mqtt_keepalive": 60,
    "mqtt_clean_session": True,
    "mqtt_use_ssl": False,
    
    # Logging Configuration
    "log_level": "info",
    "log_file": "logs/uwb_bridge.log",
    "log_rotation_size_mb": 10,
    "log_rotation_count": 5,
    
    # Initial Transform Configuration (will be overridden by transform_config)
    "origin_x": 0.0,
    "origin_y": 0.0,
    "scale": 1.0,
    "rotation_rad": 0.0,
    "x_flipped": False,
    "y_flipped": False,
    "frame_id": "map",
    "output_units": "meters"
}

db.collection('configs').document('app_config').set(app_config)
print("Created app_config document")

# Create transform_config document (for real-time updates)
transform_config = {
    "origin_x": 1000.0,      # X location of image top-left corner in UWB frame (mm)
    "origin_y": 2000.0,      # Y location of image top-left corner in UWB frame (mm)
    "scale": 0.5,            # Pixels per UWB unit (pixels/mm)
    "rotation_rad": 1.5708,  # Rotation in radians (90 degrees)
    "x_flipped": False,      # X axis flipped
    "y_flipped": False,      # Y axis flipped
    "frame_id": "map",       # Frame ID for output
    "output_units": "meters" # Output units
}

db.collection('configs').document('transform_config').set(transform_config)
print("Created transform_config document")

print("\nFirestore setup complete!")
print("You can now update transform_config in real-time from the Firestore console.")
```

## JavaScript/Node.js Script to Initialize Firestore

```javascript
const admin = require('firebase-admin');
const serviceAccount = require('./path/to/serviceAccountKey.json');

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount)
});

const db = admin.firestore();

async function initializeFirestore() {
  // Create app_config document
  const appConfig = {
    // MQTT Configuration
    mqtt_dual_mode: false,
    mqtt_broker: "tcp://localhost:1883",
    mqtt_client_id: "uwb_bridge_cpp",
    mqtt_username: "",
    mqtt_password: "",
    mqtt_source_topic: "tags/#",
    mqtt_dest_topic_prefix: "processed/",
    mqtt_qos: 1,
    mqtt_keepalive: 60,
    mqtt_clean_session: true,
    mqtt_use_ssl: false,
    
    // Logging Configuration
    log_level: "info",
    log_file: "logs/uwb_bridge.log",
    log_rotation_size_mb: 10,
    log_rotation_count: 5,
    
    // Initial Transform Configuration
    origin_x: 0.0,
    origin_y: 0.0,
    scale: 1.0,
    rotation_rad: 0.0,
    x_flipped: false,
    y_flipped: false,
    frame_id: "map",
    output_units: "meters"
  };

  await db.collection('configs').doc('app_config').set(appConfig);
  console.log("Created app_config document");

  // Create transform_config document
  const transformConfig = {
    origin_x: 1000.0,
    origin_y: 2000.0,
    scale: 0.5,
    rotation_rad: 1.5708,
    x_flipped: false,
    y_flipped: false,
    frame_id: "map",
    output_units: "meters"
  };

  await db.collection('configs').doc('transform_config').set(transformConfig);
  console.log("Created transform_config document");

  console.log("\nFirestore setup complete!");
}

initializeFirestore().catch(console.error);
```

## Manual Setup via Firestore Console

1. Go to Firebase Console > Firestore Database
2. Click "Start collection"
3. Collection ID: `configs`
4. Add first document:
   - Document ID: `app_config`
   - Add fields as shown in the app_config structure above
5. Add second document:
   - Document ID: `transform_config`
   - Add fields as shown in the transform_config structure above

## Example: Dual MQTT Broker Configuration

For scenarios where you read from one MQTT broker and publish to another:

```json
{
  "mqtt_dual_mode": true,
  
  // Source Broker (subscribe)
  "mqtt_source_broker": "tcp://192.168.1.100:1883",
  "mqtt_source_client_id": "uwb_bridge_source",
  "mqtt_source_username": "source_user",
  "mqtt_source_password": "source_pass",
  "mqtt_source_topic": "uwb/raw/#",
  "mqtt_source_qos": 1,
  "mqtt_source_keepalive": 60,
  "mqtt_source_clean_session": true,
  "mqtt_source_use_ssl": false,
  
  // Destination Broker (publish)
  "mqtt_dest_broker": "ssl://192.168.1.200:8883",
  "mqtt_dest_client_id": "uwb_bridge_dest",
  "mqtt_dest_username": "dest_user",
  "mqtt_dest_password": "dest_pass",
  "mqtt_dest_topic_prefix": "processed/uwb/",
  "mqtt_dest_qos": 2,
  "mqtt_dest_keepalive": 60,
  "mqtt_dest_clean_session": true,
  "mqtt_dest_use_ssl": true,
  
  // Logging and Transform configs remain the same
  "log_level": "info",
  "log_file": "logs/uwb_bridge.log",
  "log_rotation_size_mb": 10,
  "log_rotation_count": 5
}
```

## Testing Real-Time Updates

After setup, test real-time updates with this Python script:

```python
import firebase_admin
from firebase_admin import credentials, firestore
import time
import math

cred = credentials.Certificate('path/to/serviceAccountKey.json')
firebase_admin.initialize_app(cred)

db = firestore.client()

# Update transform config every 5 seconds with different rotations
angles = [0, 45, 90, 135, 180]

for angle in angles:
    angle_rad = math.radians(angle)
    
    update_data = {
        "rotation_rad": angle_rad,
        "scale": 0.5 + (angle / 360.0)  # Also vary scale
    }
    
    db.collection('configs').document('transform_config').update(update_data)
    print(f"Updated rotation to {angle}° ({angle_rad:.4f} rad)")
    
    time.sleep(5)

print("Test complete!")
```

## Field Descriptions

### Transform Config Fields

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `origin_x` | double | X coordinate of image top-left corner in UWB frame (mm) | 1000.0 |
| `origin_y` | double | Y coordinate of image top-left corner in UWB frame (mm) | 2000.0 |
| `scale` | double | Pixels per UWB unit (pixels/mm) | 0.5 |
| `rotation_rad` | double | Rotation of UWB frame in radians (counter-clockwise) | 1.5708 |
| `x_flipped` | boolean | True if UWB X axis opposes Image X axis | false |
| `y_flipped` | boolean | True if UWB Y axis opposes Image Y axis | false |
| `frame_id` | string | Frame ID to add to output coordinates | "map" |
| `output_units` | string | Output units: "meters", "millimeters", or "pixels" | "meters" |

### MQTT Config Fields

| Field | Type | Description | Example |
|-------|------|-------------|---------|
| `mqtt_dual_mode` | boolean | Use separate brokers for source/dest | false |
| `mqtt_broker` | string | MQTT broker address | "tcp://localhost:1883" |
| `mqtt_client_id` | string | MQTT client identifier | "uwb_bridge_cpp" |
| `mqtt_username` | string | MQTT username (optional) | "" |
| `mqtt_password` | string | MQTT password (optional) | "" |
| `mqtt_source_topic` | string | Topic pattern to subscribe to | "tags/#" |
| `mqtt_dest_topic_prefix` | string | Prefix for published topics | "processed/" |
| `mqtt_qos` | integer | Quality of Service (0, 1, or 2) | 1 |
| `mqtt_keepalive` | integer | Keep-alive interval in seconds | 60 |
| `mqtt_clean_session` | boolean | Clean session flag | true |
| `mqtt_use_ssl` | boolean | Enable SSL/TLS | false |
