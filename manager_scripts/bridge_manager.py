#!/usr/bin/env python3.12
"""
UWB Bridge Manager - Python Controller
========================================

This script manages the C++ UWB Bridge process by:
1. Connecting to Firebase Firestore to fetch configuration
2. Listening for real-time configuration changes
3. Writing config to a JSON file atomically
4. Managing the C++ process lifecycle (start/restart/monitor)

Author: Generated for Sidecar Architecture
Version: 1.0.0
"""

import json
import logging
import os
import signal
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from typing import Any, Dict, Optional

import firebase_admin
from firebase_admin import credentials, firestore
from google.cloud.firestore_v1 import DocumentSnapshot

# ==============================================================================
# Configuration Constants
# ==============================================================================

# Firebase Configuration
SERVICE_ACCOUNT_PATH = "../uwb_bridge_cpp/nova_database_cred.json"
FIRESTORE_COLLECTION = "setups"
FIRESTORE_DOCUMENT = "&GSP&Office&29607"
FIRESTORE_ENV_PATH = "environment"
FIRESTORE_POZYX_DOC = "pozyx"

# C++ Process Configuration
CPP_EXECUTABLE = "../uwb_bridge_cpp/build/bin/uwb_bridge"
CONFIG_FILE_PATH = "../uwb_bridge_cpp/config/runtime_config.json"

# Process Management
RESTART_DELAY_SECONDS = 2
WATCHDOG_CHECK_INTERVAL = 5  # Check if process crashed every 5 seconds

# ==============================================================================
# Logging Setup
# ==============================================================================

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] [BridgeManager] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# ==============================================================================
# BridgeManager Class
# ==============================================================================

class BridgeManager:
    """Manages the C++ UWB Bridge process and Firebase configuration."""
    
    def __init__(self):
        """Initialize the Bridge Manager."""
        self.db: Optional[firestore.Client] = None
        self.cpp_process: Optional[subprocess.Popen] = None
        self.current_config: Optional[Dict[str, Any]] = None
        self.running = False
        self.unsubscribe = None  # Firestore listener unsubscribe function
        
    def initialize_firebase(self) -> bool:
        """
        Initialize Firebase Admin SDK with service account credentials.
        
        Returns:
            bool: True if initialization succeeded, False otherwise
        """
        try:
            logger.info(f"Initializing Firebase with service account: {SERVICE_ACCOUNT_PATH}")
            
            if not os.path.exists(SERVICE_ACCOUNT_PATH):
                logger.error(f"Service account file not found: {SERVICE_ACCOUNT_PATH}")
                return False
            
            cred = credentials.Certificate(SERVICE_ACCOUNT_PATH)
            firebase_admin.initialize_app(cred)
            self.db = firestore.client()
            
            logger.info("Firebase initialized successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to initialize Firebase: {e}")
            return False
    
    def parse_firestore_data(self, doc_snapshot: DocumentSnapshot) -> Optional[Dict[str, Any]]:
        """
        Parse Firestore document snapshot into the required JSON configuration format.
        
        Args:
            doc_snapshot: Firestore document snapshot
            
        Returns:
            Dictionary containing the configuration, or None if parsing fails
        """
        try:
            if not doc_snapshot.exists:
                logger.warning("Firestore document does not exist")
                return None
            
            data = doc_snapshot.to_dict()
            if not data:
                logger.warning("Firestore document is empty")
                return None
            
            logger.debug(f"Raw Firestore data: {json.dumps(data, indent=2)}")
            
            # Extract source broker configuration
            source_broker = data.get('source_broker', {})
            dest_broker = data.get('dest_broker', {})
            transform = data.get('transform', {})
            
            # Build configuration matching the C++ expected format
            config = {
                "source_broker": {
                    "broker_address": source_broker.get('broker_address', ''),
                    "port": source_broker.get('port', 1883),
                    "use_ssl": source_broker.get('use_ssl', False),
                    "use_websockets": source_broker.get('use_websockets', False),
                    "ws_path": source_broker.get('ws_path', '/mqtt'),
                    "username": source_broker.get('username', ''),
                    "password": source_broker.get('password', ''),
                    "source_topic": source_broker.get('source_topic', ''),
                    "client_id": source_broker.get('client_id', 'uwb_bridge_subscriber'),
                    "qos": source_broker.get('qos', 1),
                    "keepalive_interval": source_broker.get('keepalive_interval', 60),
                    "clean_session": source_broker.get('clean_session', True)
                },
                "dest_broker": {
                    "broker_address": dest_broker.get('broker_address', ''),
                    "port": dest_broker.get('port', 8883),
                    "use_ssl": dest_broker.get('use_ssl', True),
                    "username": dest_broker.get('username', ''),
                    "password": dest_broker.get('password', ''),
                    "dest_topic_prefix": dest_broker.get('dest_topic_prefix', 'uwb/processed/tags/'),
                    "client_id": dest_broker.get('client_id', 'uwb_bridge_publisher'),
                    "qos": dest_broker.get('qos', 1)
                },
                "transform": {
                    "origin_x": transform.get('origin_x', 0.0),
                    "origin_y": transform.get('origin_y', 0.0),
                    "rotation": transform.get('rotation', 0.0),
                    "scale": transform.get('scale', 1.0),
                    "x_flip": transform.get('x_flip', 1),
                    "y_flip": transform.get('y_flip', -1),
                    "frame_id": transform.get('frame_id', 'floorplan_pixel_frame'),
                    "output_units": transform.get('output_units', 'meters')
                },
                "logging": {
                    "log_level": data.get('log_level', 'info'),
                    "log_file": data.get('log_file', 'uwb_bridge.log')
                }
            }
            
            # Validate required fields
            if not config['source_broker']['broker_address']:
                logger.error("Missing source broker address")
                return None
            
            if not config['source_broker']['source_topic']:
                logger.error("Missing source topic")
                return None
            
            if not config['dest_broker']['broker_address']:
                logger.error("Missing destination broker address")
                return None
            
            logger.info("Successfully parsed Firestore configuration")
            return config
            
        except Exception as e:
            logger.error(f"Error parsing Firestore data: {e}", exc_info=True)
            return None
    
    def write_config_atomic(self, config: Dict[str, Any], target_path: str) -> bool:
        """
        Write configuration to file atomically to prevent corrupted reads.
        
        Uses a temporary file + rename strategy for atomic writes.
        
        Args:
            config: Configuration dictionary to write
            target_path: Target file path
            
        Returns:
            bool: True if write succeeded, False otherwise
        """
        try:
            # Ensure target directory exists
            target_dir = os.path.dirname(target_path)
            os.makedirs(target_dir, exist_ok=True)
            
            # Write to temporary file in the same directory
            # (must be same filesystem for atomic rename)
            fd, temp_path = tempfile.mkstemp(
                dir=target_dir,
                prefix='.tmp_config_',
                suffix='.json'
            )
            
            try:
                with os.fdopen(fd, 'w') as f:
                    json.dump(config, f, indent=2)
                    f.flush()
                    os.fsync(f.fileno())  # Force write to disk
                
                # Atomic rename
                os.replace(temp_path, target_path)
                logger.info(f"Configuration written atomically to: {target_path}")
                return True
                
            except Exception as e:
                # Clean up temp file on error
                try:
                    os.unlink(temp_path)
                except Exception:
                    pass
                raise e
                
        except Exception as e:
            logger.error(f"Failed to write configuration: {e}")
            return False
    
    def start_cpp_process(self) -> bool:
        """
        Start the C++ executable with the configuration file.
        
        Returns:
            bool: True if process started successfully, False otherwise
        """
        try:
            if self.cpp_process:
                logger.warning("C++ process is already running")
                return False
            
            if not os.path.exists(CPP_EXECUTABLE):
                logger.error(f"C++ executable not found: {CPP_EXECUTABLE}")
                return False
            
            if not os.path.exists(CONFIG_FILE_PATH):
                logger.error(f"Configuration file not found: {CONFIG_FILE_PATH}")
                return False
            
            # Build command
            cmd = [CPP_EXECUTABLE, "--config", CONFIG_FILE_PATH]
            
            logger.info(f"Starting C++ process: {' '.join(cmd)}")
            
            # Start process
            self.cpp_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,  # Line buffered
                universal_newlines=True
            )
            
            # Give it a moment to start
            time.sleep(0.5)
            
            # Check if it's still running
            if self.cpp_process.poll() is not None:
                stdout, stderr = self.cpp_process.communicate()
                logger.error("C++ process failed to start")
                logger.error(f"STDOUT: {stdout}")
                logger.error(f"STDERR: {stderr}")
                self.cpp_process = None
                return False
            
            logger.info(f"C++ process started successfully (PID: {self.cpp_process.pid})")
            return True
            
        except Exception as e:
            logger.error(f"Failed to start C++ process: {e}")
            self.cpp_process = None
            return False
    
    def stop_cpp_process(self, timeout: int = 10) -> bool:
        """
        Stop the C++ process gracefully.
        
        Args:
            timeout: Maximum time to wait for graceful shutdown (seconds)
            
        Returns:
            bool: True if process stopped successfully, False otherwise
        """
        try:
            if not self.cpp_process:
                logger.debug("No C++ process to stop")
                return True
            
            pid = self.cpp_process.pid
            logger.info(f"Stopping C++ process (PID: {pid})...")
            
            # Send SIGTERM for graceful shutdown
            self.cpp_process.terminate()
            
            # Wait for process to exit
            try:
                stdout, stderr = self.cpp_process.communicate(timeout=timeout)
                logger.info("C++ process stopped gracefully")
                if stdout:
                    logger.debug(f"Process stdout: {stdout[:500]}")  # Log first 500 chars
                if stderr:
                    logger.debug(f"Process stderr: {stderr[:500]}")
            except subprocess.TimeoutExpired:
                logger.warning("Process did not stop gracefully, killing...")
                self.cpp_process.kill()
                self.cpp_process.wait()
                logger.info("C++ process killed")
            
            self.cpp_process = None
            return True
            
        except Exception as e:
            logger.error(f"Error stopping C++ process: {e}")
            self.cpp_process = None
            return False
    
    def restart_cpp_process(self) -> bool:
        """
        Restart the C++ process (stop then start).
        
        Returns:
            bool: True if restart succeeded, False otherwise
        """
        logger.info("Restarting C++ process...")
        
        # Stop existing process
        self.stop_cpp_process()
        
        # Wait a bit before restarting
        time.sleep(RESTART_DELAY_SECONDS)
        
        # Start new process
        return self.start_cpp_process()
    
    def check_process_health(self) -> bool:
        """
        Check if the C++ process is still running.
        
        Returns:
            bool: True if process is running, False if it crashed
        """
        if not self.cpp_process:
            return False
        
        # Check if process has exited
        return_code = self.cpp_process.poll()
        
        if return_code is not None:
            # Process has exited
            logger.error(f"C++ process crashed with return code: {return_code}")
            
            # Try to get any remaining output
            try:
                stdout, stderr = self.cpp_process.communicate(timeout=1)
                if stdout:
                    logger.error(f"Process stdout: {stdout}")
                if stderr:
                    logger.error(f"Process stderr: {stderr}")
            except Exception:
                pass
            
            self.cpp_process = None
            return False
        
        return True
    
    def on_config_change(self, 
                        doc_snapshot: list[DocumentSnapshot],
                        changes: list[Any],
                        read_time: datetime) -> None:
        """
        Callback for Firestore real-time updates.
        
        Args:
            doc_snapshot: List of document snapshots
            changes: List of changes
            read_time: Time of the snapshot
        """
        try:
            logger.info("Configuration change detected in Firestore")
            
            if not doc_snapshot:
                logger.warning("Empty document snapshot received")
                return
            
            # Parse the new configuration
            new_config = self.parse_firestore_data(doc_snapshot[0])
            
            if not new_config:
                logger.error("Failed to parse new configuration, keeping current config")
                return
            
            # Check if configuration actually changed
            if new_config == self.current_config:
                logger.info("Configuration unchanged, no action needed")
                return
            
            logger.info("Configuration changed, updating...")
            
            # Write new configuration
            if not self.write_config_atomic(new_config, CONFIG_FILE_PATH):
                logger.error("Failed to write new configuration")
                return
            
            self.current_config = new_config
            
            # Restart C++ process to apply new configuration
            if not self.restart_cpp_process():
                logger.error("Failed to restart C++ process with new configuration")
            else:
                logger.info("Successfully applied new configuration")
            
        except Exception as e:
            logger.error(f"Error handling configuration change: {e}", exc_info=True)
    
    def setup_firestore_listener(self) -> bool:
        """
        Setup Firestore real-time listener for configuration changes.
        
        Returns:
            bool: True if listener setup succeeded, False otherwise
        """
        try:
            # Build document reference path
            doc_ref = (self.db.collection(FIRESTORE_COLLECTION)
                      .document(FIRESTORE_DOCUMENT)
                      .collection(FIRESTORE_ENV_PATH)
                      .document(FIRESTORE_POZYX_DOC))
            
            logger.info(f"Setting up Firestore listener on: {doc_ref.path}")
            
            # Attach listener
            self.unsubscribe = doc_ref.on_snapshot(self.on_config_change)
            
            logger.info("Firestore listener active")
            return True
            
        except Exception as e:
            logger.error(f"Failed to setup Firestore listener: {e}")
            return False
    
    def load_initial_config(self) -> bool:
        """
        Load initial configuration from Firestore.
        
        Returns:
            bool: True if initial config loaded successfully, False otherwise
        """
        try:
            # Get document reference
            doc_ref = (self.db.collection(FIRESTORE_COLLECTION)
                      .document(FIRESTORE_DOCUMENT)
                      .collection(FIRESTORE_ENV_PATH)
                      .document(FIRESTORE_POZYX_DOC))
            
            logger.info("Loading initial configuration from Firestore...")
            
            # Fetch document
            doc_snapshot = doc_ref.get()
            
            # Parse configuration
            config = self.parse_firestore_data(doc_snapshot)
            
            if not config:
                logger.error("Failed to parse initial configuration")
                return False
            
            # Write configuration file
            if not self.write_config_atomic(config, CONFIG_FILE_PATH):
                logger.error("Failed to write initial configuration")
                return False
            
            self.current_config = config
            logger.info("Initial configuration loaded successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to load initial configuration: {e}")
            return False
    
    def run(self) -> int:
        """
        Main run loop for the Bridge Manager.
        
        Returns:
            int: Exit code (0 for success, non-zero for error)
        """
        try:
            logger.info("==============================================")
            logger.info("  UWB Bridge Manager - Python Controller")
            logger.info("  Version: 1.0.0")
            logger.info("==============================================")
            
            # Initialize Firebase
            if not self.initialize_firebase():
                logger.critical("Firebase initialization failed")
                return 1
            
            # Load initial configuration from Firestore
            if not self.load_initial_config():
                logger.critical("Failed to load initial configuration")
                return 1
            
            # Setup real-time listener
            if not self.setup_firestore_listener():
                logger.critical("Failed to setup Firestore listener")
                return 1
            
            # Start C++ process
            if not self.start_cpp_process():
                logger.critical("Failed to start C++ process")
                return 1
            
            self.running = True
            logger.info("Bridge Manager running. Press Ctrl+C to stop.")
            
            # Main watchdog loop
            while self.running:
                time.sleep(WATCHDOG_CHECK_INTERVAL)
                
                # Check if C++ process is still alive
                if not self.check_process_health():
                    logger.warning("C++ process died unexpectedly, restarting...")
                    
                    if not self.restart_cpp_process():
                        logger.error("Failed to restart C++ process")
                        # Keep trying in the loop
                    else:
                        logger.info("C++ process restarted successfully")
            
            return 0
            
        except KeyboardInterrupt:
            logger.info("Received keyboard interrupt, shutting down...")
            return 0
        except Exception as e:
            logger.critical(f"Fatal error in main loop: {e}", exc_info=True)
            return 1
        finally:
            self.cleanup()
    
    def cleanup(self) -> None:
        """Cleanup resources before exit."""
        logger.info("Cleaning up...")
        
        self.running = False
        
        # Unsubscribe from Firestore listener
        if self.unsubscribe:
            try:
                self.unsubscribe.unsubscribe()
                logger.info("Firestore listener unsubscribed")
            except Exception as e:
                logger.error(f"Error unsubscribing from Firestore: {e}")
        
        # Stop C++ process
        self.stop_cpp_process()
        
        logger.info("Cleanup complete")

# ==============================================================================
# Signal Handlers
# ==============================================================================

manager: Optional[BridgeManager] = None

def signal_handler(signum, frame):
    """Handle termination signals gracefully."""
    logger.info(f"Received signal {signum}, initiating shutdown...")
    if manager:
        manager.running = False

# ==============================================================================
# Main Entry Point
# ==============================================================================

def main() -> int:
    """Main entry point."""
    global manager
    
    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create and run manager
    manager = BridgeManager()
    return manager.run()

if __name__ == "__main__":
    sys.exit(main())
