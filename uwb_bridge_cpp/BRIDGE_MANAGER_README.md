# UWB Bridge Manager - Sidecar Architecture

## Overview

The **Bridge Manager** is a Python controller that implements the "Sidecar" architecture pattern for managing the C++ UWB Bridge application. It handles Firebase Firestore configuration synchronization and lifecycle management of the C++ process.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     Firebase Firestore                  │
│               (Configuration Database)                  │
└────────────────────┬────────────────────────────────────┘
                     │ Real-time Listener
                     │ (on_snapshot)
                     ↓
┌─────────────────────────────────────────────────────────┐
│           bridge_manager.py (Python Controller)         │
│  • Fetches configuration from Firestore                 │
│  • Writes config.json atomically                        │
│  • Manages C++ process lifecycle                        │
│  • Restarts on config changes                           │
│  • Watchdog: Auto-restart on crashes                    │
└────────────────────┬────────────────────────────────────┘
                     │ Spawns/Controls via subprocess
                     ↓
┌─────────────────────────────────────────────────────────┐
│           uwb_bridge (C++ Worker)                       │
│  • Loads config from JSON file                          │
│  • High-speed MQTT bridging                            │
│  • Real-time coordinate transformation                  │
│  • No Firebase dependencies                             │
└─────────────────────────────────────────────────────────┘
```

## Components

### 1. **Python Controller** (`bridge_manager.py`)

**Role:** The "Boss"

**Responsibilities:**
- Connect to Firebase using Admin SDK (Service Account)
- Listen for real-time configuration changes via `on_snapshot`
- Transform Firestore data to JSON configuration format
- Write configuration atomically (prevents C++ from reading partial files)
- Manage C++ process:
  - Start the executable
  - Monitor for crashes (watchdog)
  - Restart on config changes or crashes

### 2. **C++ Worker** (`uwb_bridge`)

**Role:** The "Muscle"

**Responsibilities:**
- Load configuration from JSON file
- Pure C++ with no Firebase dependencies
- High-performance MQTT bridging and transformation
- Graceful shutdown on SIGTERM

## Configuration Flow

### Firestore Document Structure

**Path:** `setups/&GSP&Office&29607/environment/pozyx`

**Fields:**
```javascript
{
  source_broker: {
    broker_address: "mqtt.cloud.pozyxlabs.com",
    port: 443,
    use_ssl: true,
    username: "...",
    password: "...",
    source_topic: "tags/#",
    client_id: "uwb_bridge_subscriber",
    qos: 1,
    keepalive_interval: 60,
    clean_session: true
  },
  dest_broker: {
    broker_address: "...",
    port: 8883,
    use_ssl: true,
    username: "...",
    password: "...",
    dest_topic_prefix: "uwb/processed/tags/",
    client_id: "uwb_bridge_publisher",
    qos: 1
  },
  transform: {
    origin_x: -5994.48,
    origin_y: 31132.18,
    rotation: 0.0,      // degrees
    scale: 18.929,
    x_flip: 1,          // 1 or -1
    y_flip: -1,         // 1 or -1
    frame_id: "floorplan_pixel_frame",
    output_units: "meters"
  },
  log_level: "info",
  log_file: "uwb_bridge.log"
}
```

### Generated JSON Configuration

The Python controller transforms Firestore data into this format:

```json
{
  "source_broker": {
    "broker_address": "mqtt.cloud.pozyxlabs.com",
    "port": 443,
    "use_ssl": true,
    "username": "...",
    "password": "...",
    "source_topic": "tags/#",
    "client_id": "uwb_bridge_subscriber",
    "qos": 1,
    "keepalive_interval": 60,
    "clean_session": true
  },
  "dest_broker": {
    "broker_address": "...",
    "port": 8883,
    "use_ssl": true,
    "username": "...",
    "password": "...",
    "dest_topic_prefix": "uwb/processed/tags/",
    "client_id": "uwb_bridge_publisher",
    "qos": 1
  },
  "transform": {
    "origin_x": -5994.48,
    "origin_y": 31132.18,
    "rotation": 0.0,
    "scale": 18.929,
    "x_flip": 1,
    "y_flip": -1,
    "frame_id": "floorplan_pixel_frame",
    "output_units": "meters"
  },
  "logging": {
    "log_level": "info",
    "log_file": "uwb_bridge.log"
  }
}
```

## Setup Instructions

### Prerequisites

1. **Python 3.8+** with pip
2. **C++ Compiler** (GCC 9+ or Clang 10+)
3. **CMake 3.15+**
4. **Firebase Service Account** credentials

### Step 1: Install Python Dependencies

```bash
cd uwb_bridge_cpp
pip install -r requirements.txt
```

### Step 2: Setup Firebase Credentials

Place your Firebase service account JSON file in the project root:

```bash
cp /path/to/your/service-account.json nova_database_cred.json
```

**⚠️ SECURITY:** Never commit `nova_database_cred.json` to version control!

Add to `.gitignore`:
```
nova_database_cred.json
```

### Step 3: Build C++ Executable

```bash
cd uwb_bridge_cpp
./build.sh
```

This creates: `./bin/uwb_bridge`

### Step 4: Configure Paths

Edit `bridge_manager.py` if needed:

```python
# Firebase Configuration
SERVICE_ACCOUNT_PATH = "nova_database_cred.json"
FIRESTORE_COLLECTION = "setups"
FIRESTORE_DOCUMENT = "&GSP&Office&29607"
FIRESTORE_ENV_PATH = "environment"
FIRESTORE_POZYX_DOC = "pozyx"

# C++ Process Configuration
CPP_EXECUTABLE = "./bin/uwb_bridge"
CONFIG_FILE_PATH = "config/runtime_config.json"
```

## Running the Bridge Manager

### Manual Start

```bash
python3 bridge_manager.py
```

**Output:**
```
[2026-02-12 10:30:00] [INFO] [BridgeManager] ==============================================
[2026-02-12 10:30:00] [INFO] [BridgeManager]   UWB Bridge Manager - Python Controller
[2026-02-12 10:30:00] [INFO] [BridgeManager]   Version: 1.0.0
[2026-02-12 10:30:00] [INFO] [BridgeManager] ==============================================
[2026-02-12 10:30:00] [INFO] [BridgeManager] Initializing Firebase with service account: nova_database_cred.json
[2026-02-12 10:30:01] [INFO] [BridgeManager] Firebase initialized successfully
[2026-02-12 10:30:01] [INFO] [BridgeManager] Loading initial configuration from Firestore...
[2026-02-12 10:30:02] [INFO] [BridgeManager] Successfully parsed Firestore configuration
[2026-02-12 10:30:02] [INFO] [BridgeManager] Configuration written atomically to: config/runtime_config.json
[2026-02-12 10:30:02] [INFO] [BridgeManager] Initial configuration loaded successfully
[2026-02-12 10:30:02] [INFO] [BridgeManager] Setting up Firestore listener on: setups/&GSP&Office&29607/environment/pozyx
[2026-02-12 10:30:02] [INFO] [BridgeManager] Firestore listener active
[2026-02-12 10:30:02] [INFO] [BridgeManager] Starting C++ process: ./bin/uwb_bridge --config config/runtime_config.json
[2026-02-12 10:30:03] [INFO] [BridgeManager] C++ process started successfully (PID: 12345)
[2026-02-12 10:30:03] [INFO] [BridgeManager] Bridge Manager running. Press Ctrl+C to stop.
```

### Stop

Press `Ctrl+C` or send `SIGTERM`:

```bash
kill -TERM <pid>
```

The manager will:
1. Stop listening to Firestore
2. Send SIGTERM to C++ process
3. Wait for graceful shutdown (10 seconds)
4. Force kill if necessary

## Systemd Service (Production)

### Create Service File

Create `/etc/systemd/system/uwb-bridge-manager.service`:

```ini
[Unit]
Description=UWB Bridge Manager - Python Controller
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=uwb
Group=uwb
WorkingDirectory=/opt/uwb_bridge
ExecStart=/usr/bin/python3 /opt/uwb_bridge/bridge_manager.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/uwb_bridge/config /opt/uwb_bridge/logs

[Install]
WantedBy=multi-user.target
```

### Install and Enable

```bash
# Copy files to /opt
sudo cp -r uwb_bridge_cpp /opt/uwb_bridge
sudo chown -R uwb:uwb /opt/uwb_bridge

# Reload systemd
sudo systemctl daemon-reload

# Enable at boot
sudo systemctl enable uwb-bridge-manager

# Start service
sudo systemctl start uwb-bridge-manager

# Check status
sudo systemctl status uwb-bridge-manager

# View logs
sudo journalctl -u uwb-bridge-manager -f
```

## Operational Features

### ✅ Atomic Configuration Writes

**Problem:** C++ process might read a partially written config file.

**Solution:** Write to a temporary file, then atomically rename:

```python
def write_config_atomic(config, target_path):
    fd, temp_path = tempfile.mkstemp(dir=same_directory)
    write_to(temp_path)
    os.replace(temp_path, target_path)  # Atomic!
```

### ✅ Graceful Restart on Config Change

When Firestore configuration changes:
1. Detect change via `on_snapshot` callback
2. Write new config atomically
3. Send SIGTERM to C++ process
4. Wait for process to exit (10s timeout)
5. Start new C++ process with updated config

**Result:** Zero data loss, clean MQTT disconnects/reconnects.

### ✅ Watchdog: Auto-Restart on Crash

Every 5 seconds, the manager checks if C++ process died:

```python
while running:
    sleep(5)
    if not check_process_health():
        logger.warning("Process crashed, restarting...")
        restart_cpp_process()
```

**Result:** Unattended operation, maximum uptime.

### ✅ Clean Shutdown

On `SIGINT` (Ctrl+C) or `SIGTERM`:
1. Unsubscribe from Firestore listener
2. Send SIGTERM to C++ process
3. Wait for graceful exit
4. Clean up resources

## Monitoring and Debugging

### Check if Python Controller is Running

```bash
ps aux | grep bridge_manager.py
```

### Check if C++ Process is Running

```bash
ps aux | grep uwb_bridge
```

### View Python Logs (systemd)

```bash
journalctl -u uwb-bridge-manager -f
```

### View C++ Logs

```bash
tail -f uwb_bridge.log
```

### Test Configuration Change

Update Firestore document → Watch logs for:

```
[INFO] Configuration change detected in Firestore
[INFO] Configuration changed, updating...
[INFO] Configuration written atomically to: config/runtime_config.json
[INFO] Restarting C++ process...
[INFO] Stopping C++ process (PID: 12345)...
[INFO] C++ process stopped gracefully
[INFO] Starting C++ process: ./bin/uwb_bridge --config config/runtime_config.json
[INFO] C++ process started successfully (PID: 12389)
[INFO] Successfully applied new configuration
```

## Error Handling

### Python Controller Errors

**Firebase Connection Lost:**
- The `firebase-admin` SDK handles automatic reconnection
- Listener will resume when connection is restored

**C++ Process Crashes:**
- Watchdog detects crash within 5 seconds
- Automatically restarts the process
- Logged with error details

**Invalid Configuration:**
- Python validates before writing
- C++ validates on startup
- If invalid, C++ exits and Python restarts it

### C++ Worker Errors

**Missing Configuration File:**
```
[CRITICAL] Failed to open config file: config/runtime_config.json
```
→ Python will create it on first run

**Invalid JSON:**
```
[CRITICAL] Failed to parse JSON config: [json.exception.parse_error.101] parse error at line 5
```
→ Check `runtime_config.json` for syntax errors

**Missing Required Fields:**
```
[CRITICAL] Missing required parameter: broker_address
```
→ Check Firestore document has all required fields

## Performance Characteristics

- **Config Change Latency:** ~2-5 seconds (Firestore → Restart → Running)
- **Crash Recovery Time:** ~5 seconds (Watchdog interval)
- **Memory Footprint:** 
  - Python: ~50-100 MB
  - C++: ~20-50 MB (varies with message load)
- **CPU Usage:**
  - Python: <1% (idle), ~2-3% (during restart)
  - C++: 5-20% (varies with message throughput)

## Troubleshooting

### Problem: "Service account file not found"

**Solution:** Ensure `nova_database_cred.json` exists:
```bash
ls -la nova_database_cred.json
```

### Problem: "C++ executable not found"

**Solution:** Build the C++ executable:
```bash
./build.sh
ls -la bin/uwb_bridge
```

### Problem: "Failed to parse Firestore data"

**Solution:** Check that Firestore document has the correct structure:
- Use Firebase Console to inspect document
- Verify all required fields exist
- Check field types match expected format

### Problem: "C++ process failed to start"

**Solution:**
1. Run C++ manually to see error:
   ```bash
   ./bin/uwb_bridge --config config/runtime_config.json
   ```
2. Check MQTT credentials
3. Check network connectivity
4. Verify config file is valid JSON

### Problem: "Process keeps crashing"

**Solution:**
1. Check C++ logs: `tail -f uwb_bridge.log`
2. Validate MQTT brokers are reachable
3. Check if ports 443/8883 are open
4. Verify credentials are correct

## Security Considerations

### 🔒 Credential Storage

- Store `nova_database_cred.json` securely
- Never commit to Git
- Restrict file permissions:
  ```bash
  chmod 600 nova_database_cred.json
  ```

### 🔒 MQTT Credentials

- Firestore stores MQTT passwords
- Config file contains passwords in plaintext
- Restrict config file permissions:
  ```bash
  chmod 600 config/runtime_config.json
  ```

### 🔒 Systemd Security

- Run as non-root user (`User=uwb`)
- Use `ProtectSystem=strict`
- Use `PrivateTmp=true`
- Restrict write access with `ReadWritePaths`

## Future Enhancements

- [ ] Health check HTTP endpoint
- [ ] Prometheus metrics export
- [ ] Configuration validation UI
- [ ] Multi-site support (multiple C++ instances)
- [ ] Encrypted configuration files
- [ ] Automatic log rotation for Python logs

## License

Copyright © 2026. All rights reserved.

## Support

For issues or questions, please contact the development team.
