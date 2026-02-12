# Quick Start: UWB Bridge Manager

This guide will get you up and running in 5 minutes.

## Prerequisites

- Python 3.8 or higher
- C++ compiler (GCC 9+ or Clang 10+)
- CMake 3.15 or higher
- Firebase service account credentials

## Step 1: Install Python Dependencies (1 minute)

```bash
cd uwb_bridge_cpp
pip3 install -r requirements.txt
```

## Step 2: Place Firebase Credentials (1 minute)

Copy your Firebase service account JSON file to the project directory:

```bash
cp /path/to/your/firebase-credentials.json nova_database_cred.json
```

**Important:** This file contains sensitive credentials. Never commit it to Git!

## Step 3: Build C++ Executable (2 minutes)

```bash
./build.sh
```

This will compile the C++ bridge and place the executable at `./bin/uwb_bridge`.

## Step 4: Configure Firestore (Optional)

If your Firestore path is different, edit `bridge_manager.py`:

```python
# Firebase Configuration (lines 27-31)
FIRESTORE_COLLECTION = "setups"
FIRESTORE_DOCUMENT = "&GSP&Office&29607"
FIRESTORE_ENV_PATH = "environment"
FIRESTORE_POZYX_DOC = "pozyx"
```

## Step 5: Run! (1 minute)

```bash
python3 bridge_manager.py
```

**Expected Output:**

```
[2026-02-12 10:30:00] [INFO] [BridgeManager] ==============================================
[2026-02-12 10:30:00] [INFO] [BridgeManager]   UWB Bridge Manager - Python Controller
[2026-02-12 10:30:00] [INFO] [BridgeManager]   Version: 1.0.0
[2026-02-12 10:30:00] [INFO] [BridgeManager] ==============================================
[2026-02-12 10:30:00] [INFO] [BridgeManager] Initializing Firebase...
[2026-02-12 10:30:01] [INFO] [BridgeManager] Firebase initialized successfully
[2026-02-12 10:30:02] [INFO] [BridgeManager] Configuration loaded successfully
[2026-02-12 10:30:03] [INFO] [BridgeManager] C++ process started successfully (PID: 12345)
[2026-02-12 10:30:03] [INFO] [BridgeManager] Bridge Manager running. Press Ctrl+C to stop.
```

## Verify It Works

### 1. Check Processes

```bash
# Python controller should be running
ps aux | grep bridge_manager

# C++ worker should be running
ps aux | grep uwb_bridge
```

### 2. Check Logs

```bash
# C++ application logs
tail -f uwb_bridge.log
```

### 3. Test Configuration Update

1. Open Firebase Console
2. Navigate to: `setups/&GSP&Office&29607/environment/pozyx`
3. Change any value (e.g., `transform.scale`)
4. Watch the logs - should see:
   ```
   [INFO] Configuration change detected in Firestore
   [INFO] Restarting C++ process...
   [INFO] Successfully applied new configuration
   ```

## Stop

Press `Ctrl+C`. The manager will gracefully stop the C++ process and clean up.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `Service account file not found` | Ensure `nova_database_cred.json` exists in the current directory |
| `C++ executable not found` | Run `./build.sh` to compile the C++ application |
| `Permission denied` | Make executable: `chmod +x bin/uwb_bridge` |
| `Failed to connect to Firebase` | Check internet connection and service account permissions |
| `C++ process crashed` | Check `uwb_bridge.log` for errors. Verify MQTT credentials. |

## Next Steps

- Read [BRIDGE_MANAGER_README.md](BRIDGE_MANAGER_README.md) for detailed documentation
- Set up as a systemd service for production deployment
- Configure monitoring and alerting

## Architecture Diagram

```
Firebase Firestore
       ↓
bridge_manager.py (Python) → config.json
       ↓
uwb_bridge (C++) → MQTT Broker
```

**happy bridging!** 🚀
