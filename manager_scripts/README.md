# UWB Management Scripts

This directory contains Python management and administration scripts for the UWB tracking system.

## Directory Contents

### 1. bridge_manager.py
**Purpose:** Sidecar controller that manages the C++ UWB bridge executable

**Key Features:**
- Listens to Firebase Firestore for configuration changes
- Generates `runtime_config.json` from Firestore data
- Manages C++ process lifecycle (start, stop, restart)
- Watchdog monitoring with auto-restart on crash
- Atomic file writes to prevent corruption
- Graceful shutdown handling

**Usage:**
```bash
cd /path/to/Transform_frames/manager_scripts
./bridge_manager.py
```

**Configuration:**
- Firestore path: `setups/&GSP&Office&29607/environment/pozyx`
- Service account: `../uwb_bridge_cpp/nova_database_cred.json`
- C++ executable: `../uwb_bridge_cpp/build/bin/uwb_bridge`
- Config output: `../uwb_bridge_cpp/config/runtime_config.json`

**See:** [BRIDGE_MANAGER_README.md](../uwb_bridge_cpp/BRIDGE_MANAGER_README.md) for detailed documentation

---

### 2. validate_setup.py
**Purpose:** Pre-flight validation for dependencies and configuration

**Key Features:**
- Checks Python version (requires 3.7+)
- Validates Firebase Admin SDK installation
- Verifies service account credentials exist
- Tests C++ executable existence and permissions
- Validates Firestore connectivity
- Comprehensive error reporting

**Usage:**
```bash
cd /path/to/Transform_frames/manager_scripts
./validate_setup.py
```

**Output:**
```
[2024-01-15 10:30:00] [INFO] Starting setup validation...
[2024-01-15 10:30:00] [INFO] ✓ Python version OK: 3.12.11
[2024-01-15 10:30:00] [INFO] ✓ Firebase Admin SDK installed
[2024-01-15 10:30:00] [INFO] ✓ Service account file exists
[2024-01-15 10:30:00] [INFO] ✓ C++ executable exists and is executable
[2024-01-15 10:30:00] [INFO] ✓ Firestore connection successful
[2024-01-15 10:30:00] [INFO] All checks passed!
```

**Recommended:** Run this before starting bridge_manager.py for the first time

---

### 3. requirements.txt
**Purpose:** Python dependencies for all management scripts

**Contents:**
- `firebase-admin>=6.0.0` - Firebase SDK (Firestore + RTDB)
- `paho-mqtt>=1.6.1` - MQTT client library

**Installation:**
```bash
pip install -r requirements.txt
```

---

## Related Scripts (Parent Directory)

### tag_tracker.py
**Location:** `../tag_tracker.py`

**Purpose:** MQTT subscriber that updates Firebase RTDB with tag positions

**Key Features:**
- Subscribes to `uwb/processed/tags/#` MQTT topic
- Movement-based jitter filter (0.3m threshold)
- Inactivity monitoring (10-minute timeout)
- Firebase Realtime Database updates
- Background cleanup thread

**Usage:**
```bash
cd /path/to/Transform_frames
./tag_tracker.py
```

**See:** [TAG_TRACKER_README.md](../TAG_TRACKER_README.md) for detailed documentation

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   Firebase Cloud                             │
│  ┌──────────────────┐        ┌──────────────────┐          │
│  │   Firestore      │        │  Realtime DB     │          │
│  │  (Config Data)   │        │  (Tag Positions) │          │
│  └────────┬─────────┘        └────────▲─────────┘          │
│           │                            │                     │
└───────────┼────────────────────────────┼─────────────────────┘
            │                            │
            │ Real-time                  │ Position
            │ Listener                   │ Updates
            │                            │
┌───────────▼────────────────────────────┼─────────────────────┐
│                   Linux Server                                │
│                                        │                      │
│  ┌─────────────────────┐              │                      │
│  │ bridge_manager.py   │              │                      │
│  │ (Sidecar)           │              │                      │
│  └──────────┬──────────┘              │                      │
│             │ Generates               │                      │
│             │ runtime_config.json     │                      │
│             │                         │                      │
│             │ Manages Process         │                      │
│             ▼                         │                      │
│  ┌─────────────────────┐              │                      │
│  │ uwb_bridge (C++)    │              │                      │
│  │                     │              │                      │
│  │ - Reads MQTT        │ Publishes    │                      │
│  │ - Transforms coords │─────────────►│                      │
│  │ - Writes MQTT       │ Processed    │                      │
│  └─────────────────────┘ Tags         │                      │
│                                        │                      │
│                          ┌─────────────┴──────────┐          │
│                          │ tag_tracker.py          │          │
│                          │                         │          │
│                          │ - Subscribes to MQTT    │          │
│                          │ - Movement filtering    │          │
│                          │ - Updates RTDB          │          │
│                          └─────────────────────────┘          │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

---

## Installation

### 1. Install Python Dependencies

```bash
cd manager_scripts
pip install -r requirements.txt
```

### 2. Setup Firebase Credentials

Ensure service account JSON file exists:
```bash
ls -la ../uwb_bridge_cpp/nova_database_cred.json
```

If not present, download from Firebase Console:
1. Go to Firebase Console → Project Settings
2. Service Accounts → Generate New Private Key
3. Save as `nova_database_cred.json`

### 3. Validate Setup

```bash
./validate_setup.py
```

### 4. Build C++ Executable (if not already built)

```bash
cd ../uwb_bridge_cpp
./build.sh
```

---

## Usage Workflows

### Workflow 1: Manual Testing

```bash
# Terminal 1 - Start bridge manager (sidecar + C++ bridge)
cd manager_scripts
./bridge_manager.py

# Terminal 2 - Start tag tracker
cd ..
./tag_tracker.py

# Monitor logs from both terminals
```

### Workflow 2: Production Deployment (systemd)

See [DEPLOYMENT_GUIDE.md](../uwb_bridge_cpp/DEPLOYMENT_GUIDE.md) for systemd service setup.

```bash
# Install services
sudo cp deploy/bridge-manager.service /etc/systemd/system/
sudo cp deploy/tag-tracker.service /etc/systemd/system/

# Start services
sudo systemctl start bridge-manager
sudo systemctl start tag-tracker

# Enable auto-start on boot
sudo systemctl enable bridge-manager
sudo systemctl enable tag-tracker
```

---

## Configuration Files

### Firebase Configuration

**Service Account:**
- Path: `../uwb_bridge_cpp/nova_database_cred.json`
- Format: Firebase service account JSON
- Permissions: Read/Write Firestore, Read/Write RTDB

**Firestore Collection:**
- Path: `setups/&GSP&Office&29607/environment/pozyx`
- Fields: `source_broker`, `dest_broker`, `rotation`, `transform`

**RTDB URL:**
- Default: `https://nova-40708-default-rtdb.firebaseio.com`
- Structure: `tags/{tagId}/` nodes

### MQTT Configuration

**Pozyx Setup File:**
- Path: `../uwb_bridge_cpp/config/pozyx_setup.json`
- Used by: tag_tracker.py
- Fields: `dest_broker` (connection details)

**Runtime Config File:**
- Path: `../uwb_bridge_cpp/config/runtime_config.json`
- Generated by: bridge_manager.py
- Used by: uwb_bridge (C++ executable)
- Auto-updated on Firestore changes

---

## Troubleshooting

### Issue: "Firebase module not found"

**Solution:**
```bash
pip install firebase-admin
# Or
pip install -r requirements.txt
```

### Issue: "Service account file not found"

**Solution:**
```bash
# Check path
ls -la ../uwb_bridge_cpp/nova_database_cred.json

# Update path in script if needed
SERVICE_ACCOUNT_PATH = "../uwb_bridge_cpp/nova_database_cred.json"
```

### Issue: "C++ executable not found"

**Solution:**
```bash
# Build the executable
cd ../uwb_bridge_cpp
./build.sh

# Verify it exists
ls -la build/bin/uwb_bridge
```

### Issue: "Permission denied when executing scripts"

**Solution:**
```bash
chmod +x bridge_manager.py validate_setup.py
chmod +x ../tag_tracker.py
```

### Issue: "Firestore connection failed"

**Solution:**
1. Check internet connectivity
2. Verify service account has correct permissions
3. Check Firebase project ID in credentials file
4. Ensure Firestore database is enabled in Firebase Console

---

## Monitoring and Logs

### View Live Logs

```bash
# Bridge Manager
tail -f /var/log/bridge-manager.log

# Tag Tracker
tail -f /var/log/tag-tracker.log

# Or use journalctl for systemd services
sudo journalctl -u bridge-manager -f
sudo journalctl -u tag-tracker -f
```

### Check Process Status

```bash
# If running as systemd services
sudo systemctl status bridge-manager
sudo systemctl status tag-tracker

# If running manually
ps aux | grep -E "(bridge_manager|tag_tracker)"
```

---

## Development

### Enable Debug Logging

Edit script and change logging level:

```python
logging.basicConfig(
    level=logging.DEBUG,  # Change from INFO
    format='[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s'
)
```

### Test Configuration Changes

1. Update Firestore document:
   ```javascript
   // Firebase Console → Firestore
   // Edit: setups/&GSP&Office&29607/environment/pozyx
   ```

2. Watch bridge_manager.py logs:
   ```bash
   # Should see:
   [INFO] Configuration changed, regenerating runtime_config.json
   [INFO] Restarting C++ process with new configuration
   ```

### Validate JSON Configuration

```bash
# Check runtime config syntax
python3 -m json.tool ../uwb_bridge_cpp/config/runtime_config.json

# Check pozyx setup syntax
python3 -m json.tool ../uwb_bridge_cpp/config/pozyx_setup.json
```

---

## Safety and Best Practices

### 1. Atomic File Writes
All configuration writes use atomic operations to prevent corruption:
```python
# GOOD: Atomic write
fd, temp_path = tempfile.mkstemp()
os.write(fd, json_data)
os.close(fd)
os.replace(temp_path, target_path)

# BAD: Non-atomic write (can corrupt file if process crashes)
with open(target_path, 'w') as f:
    f.write(json_data)
```

### 2. Process Health Checks
Bridge manager checks C++ process health every 5 seconds:
```python
watchdog_interval = 5  # seconds
```

### 3. Graceful Shutdown
Always use Ctrl+C or SIGTERM, not SIGKILL:
```bash
# GOOD
kill -TERM <pid>
# or
sudo systemctl stop bridge-manager

# BAD (use only as last resort)
kill -9 <pid>
```

### 4. Log Rotation
For production deployments, configure logrotate:
```bash
# /etc/logrotate.d/uwb-tracking
/var/log/bridge-manager.log /var/log/tag-tracker.log {
    daily
    rotate 7
    compress
    missingok
    notifempty
}
```

---

## Related Documentation

- [BRIDGE_MANAGER_README.md](../uwb_bridge_cpp/BRIDGE_MANAGER_README.md) - Bridge manager details
- [TAG_TRACKER_README.md](../TAG_TRACKER_README.md) - Tag tracker details
- [DEPLOYMENT_GUIDE.md](../uwb_bridge_cpp/DEPLOYMENT_GUIDE.md) - Production deployment
- [QUICKSTART.md](../uwb_bridge_cpp/QUICKSTART.md) - Quick start guide

---

## Version History

- **1.0.0** (2024-01-15)
  - Initial release
  - bridge_manager.py (sidecar architecture)
  - validate_setup.py (pre-flight checks)
  - requirements.txt (dependency management)

---

## License

Part of UWB Tracking System

## Contact

For issues or questions, refer to the main project documentation.
