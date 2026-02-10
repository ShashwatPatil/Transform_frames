# Quick Reference: Firebase Firestore Integration

## Build Commands

```bash
# libuwb_transform
cd Transform_frames/libuwb_transform/build
cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk
make -j$(nproc)

# uwb_bridge_cpp
cd Transform_frames/uwb_bridge_cpp/build
cmake .. -DFIREBASE_CPP_SDK_DIR=/opt/firebase_cpp_sdk
make -j$(nproc)
```

## Run Commands

### Local JSON Mode
```bash
./bin/uwb_bridge -c config/app_config.json
```

### Firestore Mode
```bash
export FIREBASE_PROJECT_ID="your-project-id"
export FIREBASE_API_KEY="your-api-key"
./bin/uwb_bridge --firestore
```

## Firestore Structure

```
configs/
├── app_config          (fetched once on startup)
│   ├── mqtt_broker
│   ├── mqtt_source_topic
│   ├── log_level
│   └── ...
└── transform_config    (real-time listener)
    ├── origin_x
    ├── origin_y
    ├── scale
    ├── rotation_rad
    ├── x_flipped
    └── y_flipped
```

## Key Features

### Thread Safety
- **Read operations**: Multiple concurrent readers allowed
- **Write operations**: Exclusive lock, blocks all readers
- **Lock type**: `std::shared_mutex` (C++17)

### Firestore Operations
- **AppConfig**: One-time `Get()` on startup
- **TransformConfig**: Real-time `AddSnapshotListener()`

### Backwards Compatibility
- Local JSON mode fully preserved
- No breaking changes to existing deployments
- Add `--firestore` flag to enable cloud mode

## Environment Variables

```bash
# Required for Firestore mode
export FIREBASE_PROJECT_ID="your-project-id"
export FIREBASE_API_KEY="your-api-key"

# Optional: Custom SDK location
export FIREBASE_CPP_SDK_DIR="/custom/path/to/sdk"
```

## Update Transform in Real-Time

### Via Firestore Console
1. Go to Firebase Console > Firestore
2. Navigate to `configs/transform_config`
3. Edit any field (origin_x, scale, rotation_rad, etc.)
4. Save - changes apply immediately (no restart needed)

### Via Python
```python
db.collection('configs').document('transform_config').update({
    'rotation_rad': 1.5708,  # 90 degrees
    'scale': 0.5
})
```

### Via REST API
```bash
curl -X PATCH \
  "https://firestore.googleapis.com/v1/projects/YOUR_PROJECT/databases/(default)/documents/configs/transform_config?updateMask.fieldPaths=rotation_rad" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{"fields": {"rotation_rad": {"doubleValue": 1.5708}}}'
```

## Logs to Monitor

```bash
# Firestore initialization
[INFO] Initializing Firebase App...
[INFO] Firebase App created successfully
[INFO] Firestore initialized successfully

# AppConfig fetch
[INFO] Fetching AppConfig from Firestore...
[INFO] Successfully fetched AppConfig from Firestore

# Transform listener
[INFO] Starting real-time listener for TransformConfig...
[INFO] TransformConfig listener started successfully

# Real-time update received
[INFO] Updated transform matrix from Firestore
[DEBUG] New config - Origin: (1000, 2000), Scale: 0.5, Rotation: 1.5708 rad
```

## Troubleshooting

### Build Error: Firebase SDK not found
```bash
cmake .. -DFIREBASE_CPP_SDK_DIR=/correct/path/to/firebase_cpp_sdk
```

### Runtime Error: Connection failed
```bash
# Check environment variables
echo $FIREBASE_PROJECT_ID
echo $FIREBASE_API_KEY

# Verify Firestore is enabled in Firebase Console
```

### Linking Error: undefined reference
```bash
export LD_LIBRARY_PATH=/opt/firebase_cpp_sdk/libs/linux/x86_64:$LD_LIBRARY_PATH
```

## Performance Considerations

### Firestore Limits
- **Read operations**: 50,000/day (free tier)
- **Write operations**: 20,000/day (free tier)
- **Real-time listeners**: No limit on active listeners

### Network Latency
- Initial config fetch: ~100-500ms (one-time on startup)
- Real-time updates: ~50-200ms (propagation time)

### Thread Safety Overhead
- Shared lock (read): Minimal overhead (~10ns)
- Exclusive lock (write): Blocks readers momentarily (~1μs)
- Transform operation: ~500ns (unchanged)

## Integration with BridgeCore

The FloorplanTransformer in main.cpp needs to be shared with BridgeCore for the listener to work. You may need to modify BridgeCore to accept an external transformer or expose its internal transformer:

### Option 1: Pass transformer to BridgeCore
```cpp
auto transformer = std::make_shared<uwb_transform::FloorplanTransformer>(config);
g_bridge = std::make_unique<uwb_bridge::BridgeCore>(config, transformer);
g_firestore->startTransformListener(transformer);
```

### Option 2: Get transformer from BridgeCore
```cpp
g_bridge = std::make_unique<uwb_bridge::BridgeCore>(config);
auto transformer = g_bridge->getTransformer();  // Add getter method
g_firestore->startTransformListener(transformer);
```

## Production Deployment

See `deploy/uwb-bridge-firestore.service` for systemd service configuration with Firestore support.

```bash
# Install service
sudo cp deploy/uwb-bridge-firestore.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable uwb-bridge-firestore
sudo systemctl start uwb-bridge-firestore

# Check status
sudo systemctl status uwb-bridge-firestore

# View logs
sudo journalctl -u uwb-bridge-firestore -f
```
