#!/usr/bin/env python3
"""
Tag Tracker - MQTT to Firebase RTDB Bridge
===========================================

This script implements a batched producer-consumer architecture:
1. MQTT callbacks (producer) enqueue updates without blocking
2. Firebase writer thread (consumer) batches writes for efficiency
3. Inactivity janitor thread monitors tag staleness
4. All Firebase writes batched in single multi-path update

Architecture: Single-threaded batched writes eliminate MQTT callback latency

Author: Generated for UWB Tag Tracking
Version: 2.0.0 (Batched Producer-Consumer)
"""

import json
import logging
import math
import os
import queue
import signal
import sys
import threading
import time
from datetime import datetime, timezone
from typing import Dict, Optional, Tuple

import firebase_admin
import paho.mqtt.client as mqtt
from firebase_admin import credentials, db, firestore
from google.cloud.firestore_v1 import DocumentSnapshot

# ==============================================================================
# Configuration Constants
# ==============================================================================

# Paths
SERVICE_ACCOUNT_PATH = "../uwb_bridge_cpp/nova_database_cred.json"

# Firebase Configuration
FIREBASE_RTDB_URL = "https://nova-40708-default-rtdb.firebaseio.com"
FIRESTORE_COLLECTION = "setups"
FIRESTORE_DOCUMENT = "&GSP&Office&29607"
FIRESTORE_ENV_PATH = "environment"
FIRESTORE_POZYX_DOC = "pozyx"

# Movement threshold (meters) - Tags must move > 30cm to be considered moving
MOVEMENT_THRESHOLD = 0.3

# Update rate for moving tags (Hz) - 1Hz = once per second
UPDATE_RATE_HZ = 1 
MIN_UPDATE_INTERVAL = 1.0 / UPDATE_RATE_HZ  # seconds between updates

# Inactivity timeout (seconds) - 1 minute
INACTIVITY_TIMEOUT = 60

# Inactivity check interval (seconds)
INACTIVITY_CHECK_INTERVAL = 60

# Batched writer configuration
BATCH_MAX_SIZE = 100  # Maximum items per batch
BATCH_TIMEOUT = 0.5   # Seconds to wait for first item

# ==============================================================================
# Logging Setup
# ==============================================================================

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] [TagTracker] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# ==============================================================================
# TagTracker Class
# ==============================================================================

class TagTracker:
    """Tracks UWB tags and updates Firebase RTDB with movement-based filtering."""
    
    def __init__(self):
        """Initialize the Tag Tracker."""
        self.mqtt_client: Optional[mqtt.Client] = None
        self.running = False
        
        # Local cache: tag_id -> {x, y, z, last_seen_time, last_written_x, last_written_y, last_written_z, last_write_time}
        self.tag_cache: Dict[str, Dict] = {}
        
        # Set of tags we're actively listening to (seen at least once)
        self.listening_set: set = set()
        
        # Lock for thread-safe access to cache
        self.cache_lock = threading.Lock()
        
        # Worker threads
        self.periodic_writer_thread: Optional[threading.Thread] = None
        self.inactivity_thread: Optional[threading.Thread] = None
        
        # MQTT configuration
        self.mqtt_config: Optional[Dict] = None
        
        # Firebase Firestore
        self.db: Optional[firestore.Client] = None
        self.config_listener = None
    
    def parse_mqtt_config(self, dest_broker: Dict) -> Optional[Dict]:
        """
        Parse MQTT configuration from Firestore dest_broker data.
        
        Args:
            dest_broker: Destination broker configuration from Firestore
            
        Returns:
            Optional[Dict]: Parsed MQTT configuration or None if invalid
        """
        try:
            if not dest_broker:
                logger.error("Missing dest_broker configuration")
                return None
            
            mqtt_config = {
                'broker_address': dest_broker.get('broker_address', ''),
                'port': dest_broker.get('port', 8883),
                'username': dest_broker.get('username', ''),
                'password': dest_broker.get('password', ''),
                'use_ssl': dest_broker.get('use_ssl', True),
                'topic': 'uwb/processed/tags/#'
            }
            
            # Parse broker address if it contains protocol
            broker_addr = mqtt_config['broker_address']
            if '://' in broker_addr:
                # Extract hostname from URL like ssl://hostname:port
                parts = broker_addr.split('://')
                if len(parts) > 1:
                    hostname_port = parts[1].split(':')
                    mqtt_config['broker_address'] = hostname_port[0]
                    if len(hostname_port) > 1:
                        mqtt_config['port'] = int(hostname_port[1])
            
            if not mqtt_config['broker_address']:
                logger.error("Invalid broker address in configuration")
                return None
            
            logger.info(f"MQTT config parsed: {mqtt_config['broker_address']}:{mqtt_config['port']}")
            return mqtt_config
            
        except Exception as e:
            logger.error(f"Failed to parse MQTT config: {e}")
            return None
    
    def initialize_firebase(self) -> bool:
        """
        Initialize Firebase Admin SDK with RTDB and Firestore support.
        
        Returns:
            bool: True if initialization succeeded, False otherwise
        """
        try:
            logger.info(f"Initializing Firebase with service account: {SERVICE_ACCOUNT_PATH}")
            
            if not os.path.exists(SERVICE_ACCOUNT_PATH):
                logger.error(f"Service account file not found: {SERVICE_ACCOUNT_PATH}")
                return False
            
            # Initialize Firebase with RTDB URL
            cred = credentials.Certificate(SERVICE_ACCOUNT_PATH)
            firebase_admin.initialize_app(cred, {
                'databaseURL': FIREBASE_RTDB_URL
            })
            
            # Initialize Firestore client
            self.db = firestore.client()
            
            logger.info("Firebase RTDB and Firestore initialized successfully")
            logger.info(f"Database URL: {FIREBASE_RTDB_URL}")
            return True
            
        except Exception as e:
            logger.error(f"Failed to initialize Firebase: {e}")
            return False
    
    def calculate_distance(self, pos1: Tuple[float, float, float], 
                          pos2: Tuple[float, float, float]) -> float:
        """
        Calculate 3D Euclidean distance between two positions.
        
        Args:
            pos1: First position (x, y, z)
            pos2: Second position (x, y, z)
            
        Returns:
            float: Distance in meters
        """
        dx = pos1[0] - pos2[0]
        dy = pos1[1] - pos2[1]
        dz = pos1[2] - pos2[2]
        return math.sqrt(dx*dx + dy*dy + dz*dz)
    
    def write_tag_update(self, tag_id: str, x: float, y: float, z: float, 
                        status: str = "active") -> None:
        """
        Write a Firebase RTDB update immediately.
        
        Args:
            tag_id: Tag identifier
            x: X coordinate (meters)
            y: Y coordinate (meters)
            z: Z coordinate (meters)
            status: Tag status ("active" or "inactive")
        """        # Construct the path and data
        path = f"pozyx/&GSP&Office&29607/tags/{tag_id}"
        data = {
            'x': x,
            'y': y,
            'z': z,
            'status': status,
            'timestamp': datetime.now(timezone.utc).isoformat()
        }
        
        # Write directly to Firebase
        db.reference(path).update(data)
        logger.info(f"✓ Updated {tag_id} -> ({x:.3f}, {y:.3f}, {z:.3f}) [{status}]")
        
    
    def periodic_writer_loop(self):
        """
        Periodic writer thread: Checks tags every UPDATE_INTERVAL and writes those that moved.
        
        This timer-driven approach:
        1. Wakes up every MIN_UPDATE_INTERVAL seconds
        2. Checks which tags have moved > MOVEMENT_THRESHOLD since last write
        3. Batches writes for all moved tags in a single Firebase update
        4. Stationary tags are NOT written (saves bandwidth and rate limits)
        """
        logger.info(f"Periodic writer thread started (interval: {MIN_UPDATE_INTERVAL:.1f}s)")
        
        while self.running:
            time.sleep(MIN_UPDATE_INTERVAL)
            
            if not self.running:
                break
            
            current_time = time.time()
            batch_updates = {}
            
            with self.cache_lock:
                for tag_id, cached in list(self.tag_cache.items()):
                    # Get current and last written positions
                    current_pos = (cached['x'], cached['y'], cached['z'])
                    
                    # Check if this tag has been written before
                    if 'last_written_x' in cached:
                        last_written_pos = (cached['last_written_x'], cached['last_written_y'], cached['last_written_z'])
                        distance = self.calculate_distance(current_pos, last_written_pos)
                        
                        # Only write if tag moved significantly
                        if distance > MOVEMENT_THRESHOLD:
                            # Prepare update
                            path = f"pozyx/&GSP&Office&29607/tags/{tag_id}"
                            data = {
                                'x': cached['x'],
                                'y': cached['y'],
                                'z': cached['z'],
                                'status': 'active',
                                'timestamp': datetime.now(timezone.utc).isoformat()
                            }
                            batch_updates[path] = data
                            
                            # Update last written position in cache
                            cached['last_written_x'] = cached['x']
                            cached['last_written_y'] = cached['y']
                            cached['last_written_z'] = cached['z']
                            cached['last_write_time'] = current_time
                            
                            # logger.info(f"Queued update: {tag_id} moved {distance:.3f}m -> ({cached['x']:.3f}, {cached['y']:.3f}, {cached['z']:.3f})")
                        else:
                            # logger.debug(f"Skip {tag_id}: only moved {distance:.3f}m < {MOVEMENT_THRESHOLD}m")
                            pass
                    else:
                        # First write for this tag
                        path = f"pozyx/&GSP&Office&29607/tags/{tag_id}"
                        data = {
                            'x': cached['x'],
                            'y': cached['y'],
                            'z': cached['z'],
                            'status': 'active',
                            'timestamp': datetime.now(timezone.utc).isoformat()
                        }
                        batch_updates[path] = data
                        
                        # Record first write position
                        cached['last_written_x'] = cached['x']
                        cached['last_written_y'] = cached['y']
                        cached['last_written_z'] = cached['z']
                        cached['last_write_time'] = current_time
                        
                        logger.info(f"Initial write: {tag_id} at ({cached['x']:.3f}, {cached['y']:.3f}, {cached['z']:.3f})")
            
            # Perform batched write if there are updates
            if batch_updates:
                db.reference('/').update(batch_updates)
                # logger.info(f"✓ Wrote {len(batch_updates)} tag update(s) to RTDB")

            else:
                logger.debug("No tags moved, skipping write")
        
        logger.info("Periodic writer thread stopped")

    
    def process_tag_data(self, tag_id: str, x: float, y: float, z: float):
        """
        Process incoming tag data - updates cache only, no immediate writes.
        
        The periodic writer thread will check cache and write only tags that moved.
        This approach:
        - Always stores latest position
        - Lets periodic writer decide what to write
        - Prevents spam from stationary tags
        
        Args:
            tag_id: Tag identifier
            x: X coordinate (meters)
            y: Y coordinate (meters)
            z: Z coordinate (meters)
        """
        current_time = time.time()
        
        with self.cache_lock:
            if tag_id in self.tag_cache:
                # Update existing tag - just update position and last_seen
                self.tag_cache[tag_id]['x'] = x
                self.tag_cache[tag_id]['y'] = y
                self.tag_cache[tag_id]['z'] = z
                self.tag_cache[tag_id]['last_seen_time'] = current_time
                logger.debug(f"Updated cache: {tag_id} at ({x:.3f}, {y:.3f}, {z:.3f})")
            else:
                # New tag - add to cache (periodic writer will handle first write)
                logger.info(f"New tag discovered: {tag_id} at ({x:.3f}, {y:.3f}, {z:.3f})")
                self.tag_cache[tag_id] = {
                    'x': x,
                    'y': y,
                    'z': z,
                    'last_seen_time': current_time
                    # last_written_* will be set on first write by periodic writer
                }
                self.listening_set.add(tag_id)
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """Callback when MQTT client connects."""
        if rc == 0:
            logger.info("Connected to MQTT broker")
            if self.mqtt_config:
                topic = self.mqtt_config['topic']
                client.subscribe(topic)
                logger.info(f"Subscribed to topic: {topic}")
        else:
            logger.error(f"Failed to connect to MQTT broker, return code: {rc}")
    
    def on_mqtt_disconnect(self, client, userdata, rc):
        """Callback when MQTT client disconnects."""
        if rc != 0:
            logger.warning(f"Unexpected MQTT disconnect, return code: {rc}")
        else:
            logger.info("MQTT client disconnected")
    
    def on_mqtt_message(self, client, userdata, msg):
        """Callback when MQTT message received."""

        # Parse JSON payload
        payload = json.loads(msg.payload.decode('utf-8'))
        
        # Handle array of tags
        if isinstance(payload, list):
            for tag_obj in payload:
                tag_id = tag_obj.get('tagId')
                data = tag_obj.get('data', {})
                coords = data.get('coordinates', {})
                
                if tag_id and coords:
                    x = coords.get('x', 0.0)
                    y = coords.get('y', 0.0)
                    z = coords.get('z', 0.0)
                    
                    self.process_tag_data(tag_id, x, y, z)
                # print(f"Received tag: {tag_id} at ({x:.3f}, {y:.3f}, {z:.3f})")
        
        # Handle single tag object
        elif isinstance(payload, dict):
            tag_id = payload.get('tagId')
            data = payload.get('data', {})
            coords = data.get('coordinates', {})
            
            if tag_id and coords:
                x = coords.get('x', 0.0)
                y = coords.get('y', 0.0)
                z = coords.get('z', 0.0)
                
                self.process_tag_data(tag_id, x, y, z)
    
    def setup_mqtt_client(self, config: Dict) -> bool:
        """
        Setup and connect MQTT client with given configuration.
        
        Args:
            config: MQTT configuration dictionary
            
        Returns:
            bool: True if setup succeeded, False otherwise
        """
        try:
            # Disconnect existing client if any
            if self.mqtt_client:
                try:
                    self.mqtt_client.loop_stop()
                    self.mqtt_client.disconnect()
                    logger.info("Disconnected previous MQTT client")
                except:
                    pass
            
            # Create new MQTT client
            client_id = f"tag_tracker_{int(time.time())}"
            self.mqtt_client = mqtt.Client(client_id=client_id)
            
            # Set callbacks
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
            self.mqtt_client.on_message = self.on_mqtt_message
            
            # Set credentials
            if config['username'] and config['password']:
                self.mqtt_client.username_pw_set(
                    config['username'],
                    config['password']
                )
            
            # Enable SSL/TLS if needed
            if config['use_ssl']:
                self.mqtt_client.tls_set()
            
            # Connect to broker
            logger.info(f"Connecting to MQTT broker: {config['broker_address']}:{config['port']}")
            self.mqtt_client.connect(
                config['broker_address'],
                config['port'],
                keepalive=60
            )
            
            # Start network loop
            self.mqtt_client.loop_start()
            
            # Update current config
            self.mqtt_config = config
            
            return True
            
        except Exception as e:
            logger.error(f"Failed to setup MQTT client: {e}")
            return False
    
    def inactivity_monitor(self):
        """
        Janitor thread: Monitors tag inactivity and enqueues inactive status.
        
        Runs every INACTIVITY_CHECK_INTERVAL seconds and marks tags
        as inactive if they haven't been seen for INACTIVITY_TIMEOUT seconds.
        
        NEVER calls Firebase directly - pushes to queue instead.
        """
        logger.info("Inactivity monitor (janitor) thread started")
        
        while self.running:
            time.sleep(INACTIVITY_CHECK_INTERVAL)
            
            if not self.running:
                break
            
            current_time = time.time()
            inactive_tags = []
            
            # Find inactive tags
            with self.cache_lock:
                for tag_id, cached in self.tag_cache.items():
                    time_since_last_seen = current_time - cached['last_seen_time']
                    
                    if time_since_last_seen > INACTIVITY_TIMEOUT:
                        inactive_tags.append((tag_id, cached))
            
            # Enqueue inactive status updates
            if inactive_tags:
                logger.info(f"Found {len(inactive_tags)} inactive tag(s)")
                
                for tag_id, cached in inactive_tags:
                    logger.info(f"Tag {tag_id} inactive (last seen {int(current_time - cached['last_seen_time'])} sec ago)")
                    
                    # Write inactive status directly
                    self.write_tag_update(
                        tag_id,
                        cached['x'],
                        cached['y'],
                        cached['z'],
                        "inactive"
                    )
                    
                    # Remove from listening set so it's treated as new if it returns
                    with self.cache_lock:
                        if tag_id in self.listening_set:
                            self.listening_set.remove(tag_id)
                            logger.debug(f"Removed {tag_id} from listening set")
        
        logger.info("Inactivity monitor thread stopped")
    
    def on_config_change(self, doc_snapshots, changes, read_time):
        """
        Firestore listener callback for configuration changes.
        
        Args:
            doc_snapshots: List of document snapshots
            changes: List of changes
            read_time: Read timestamp
        """

        if not doc_snapshots:
            logger.warning("Configuration document not found in Firestore")
            return
        
        doc = doc_snapshots[0]
        if not doc.exists:
            logger.error("Configuration document does not exist")
            return
        
        config_data = doc.to_dict()
        logger.info("Configuration update received from Firestore")
        
        # Extract dest_broker configuration
        dest_broker = config_data.get('dest_broker', {})
        
        if not dest_broker:
            logger.error("Missing dest_broker in Firestore configuration")
            return
        
        # Parse new MQTT configuration
        new_mqtt_config = self.parse_mqtt_config(dest_broker)
        
        if not new_mqtt_config:
            logger.error("Failed to parse MQTT configuration from Firestore")
            return
        
        # Check if configuration actually changed
        if self.mqtt_config:
            config_changed = (
                new_mqtt_config['broker_address'] != self.mqtt_config['broker_address'] or
                new_mqtt_config['port'] != self.mqtt_config['port'] or
                new_mqtt_config['username'] != self.mqtt_config['username'] or
                new_mqtt_config['password'] != self.mqtt_config['password']
            )
            
            if not config_changed:
                logger.debug("MQTT configuration unchanged, skipping reconnect")
                return
            
            logger.info("MQTT configuration changed, reconnecting...")
        else:
            logger.info("Initial MQTT configuration loaded from Firestore")
        
        # Reconnect MQTT client with new configuration
        if self.setup_mqtt_client(new_mqtt_config):
            logger.info("MQTT client reconnected successfully with new configuration")
        else:
            logger.error("Failed to reconnect MQTT client with new configuration")

    
    def setup_firestore_listener(self) -> bool:
        """
        Setup Firestore listener for MQTT configuration updates.
        
        Returns:
            bool: True if listener setup succeeded, False otherwise
        """
        try:
            # Build Firestore document path
            doc_path = f"{FIRESTORE_COLLECTION}/{FIRESTORE_DOCUMENT}/{FIRESTORE_ENV_PATH}/{FIRESTORE_POZYX_DOC}"
            
            logger.info(f"Setting up Firestore listener on: {doc_path}")
            
            # Get document reference
            doc_ref = self.db.collection(FIRESTORE_COLLECTION).document(FIRESTORE_DOCUMENT).collection(FIRESTORE_ENV_PATH).document(FIRESTORE_POZYX_DOC)
            
            # Attach snapshot listener
            self.config_listener = doc_ref.on_snapshot(self.on_config_change)
            
            logger.info("Firestore listener attached successfully")
            return True
            
        except Exception as e:
            logger.error(f"Failed to setup Firestore listener: {e}")
            return False
    
    def run(self) -> int:
        """
        Main run loop for the Tag Tracker.
        
        Returns:
            int: Exit code (0 for success, non-zero for error)
        """
        try:
            logger.info("==============================================")
            logger.info("  Tag Tracker - MQTT to Firebase RTDB")
            logger.info("  Version: 2.0.0 (Batched Producer-Consumer)")
            logger.info("==============================================")
            
            # Initialize Firebase (RTDB + Firestore)
            if not self.initialize_firebase():
                logger.critical("Failed to initialize Firebase")
                return 1
            
            # Setup Firestore listener for MQTT configuration
            if not self.setup_firestore_listener():
                logger.critical("Failed to setup Firestore configuration listener")
                return 1
            
            logger.info("Waiting for initial configuration from Firestore...")
            
            # Wait for initial configuration to load
            max_wait = 10  # seconds
            wait_interval = 0.5
            elapsed = 0
            
            while not self.mqtt_config and elapsed < max_wait:
                time.sleep(wait_interval)
                elapsed += wait_interval
            
            if not self.mqtt_config:
                logger.critical("Timeout waiting for MQTT configuration from Firestore")
                logger.critical(f"Check Firestore document: {FIRESTORE_COLLECTION}/{FIRESTORE_DOCUMENT}/{FIRESTORE_ENV_PATH}/{FIRESTORE_POZYX_DOC}")
                return 1
            
            logger.info("Initial MQTT configuration loaded successfully")
            
            # Set running flag BEFORE starting threads
            self.running = True
            
            # Start periodic writer thread
            self.periodic_writer_thread = threading.Thread(
                target=self.periodic_writer_loop,
                daemon=True
            )
            self.periodic_writer_thread.start()
            logger.info("Periodic writer thread started")
            
            # Start inactivity monitor thread (janitor)
            self.inactivity_thread = threading.Thread(
                target=self.inactivity_monitor,
                daemon=True
            )
            self.inactivity_thread.start()
            logger.info("Inactivity monitor started")
            
            logger.info("Tag Tracker running. Press Ctrl+C to stop.")
            logger.info(f"Movement threshold: {MOVEMENT_THRESHOLD}m")
            logger.info(f"Update interval: {MIN_UPDATE_INTERVAL:.1f}s ({UPDATE_RATE_HZ}Hz)")
            logger.info(f"Inactivity timeout: {INACTIVITY_TIMEOUT}s")
            logger.info(f"Mode: Timer-driven (only writes tags that moved > threshold)")
            logger.info(f"Configuration source: Firestore (real-time updates enabled)")
            
            # Keep main thread alive
            while self.running:
                time.sleep(1)
            
            return 0
            
        except KeyboardInterrupt:
            logger.info("Received keyboard interrupt, shutting down...")
            return 0
        except Exception as e:
            logger.critical(f"Fatal error in main loop: {e}", exc_info=True)
            return 1
        finally:
            self.cleanup()
    
    def cleanup(self):
        """Cleanup resources before exit."""
        logger.info("Cleaning up...")
        
        self.running = False
        
        # Stop MQTT client
        if self.mqtt_client:
            try:
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
                logger.info("MQTT client disconnected")
            except Exception as e:
                logger.error(f"Error disconnecting MQTT client: {e}")
        
        # Stop Firestore listener
        if self.config_listener:
            try:
                self.config_listener.unsubscribe()
                logger.info("Firestore listener stopped")
            except Exception as e:
                logger.error(f"Error stopping Firestore listener: {e}")
        
        # Wait for periodic writer thread to stop
        if self.periodic_writer_thread and self.periodic_writer_thread.is_alive():
            logger.info("Waiting for periodic writer thread to stop...")
            self.periodic_writer_thread.join(timeout=5)
        
        # Wait for inactivity thread to stop
        if self.inactivity_thread and self.inactivity_thread.is_alive():
            logger.info("Waiting for inactivity monitor to stop...")
            self.inactivity_thread.join(timeout=5)
        
        logger.info("Cleanup complete")

# ==============================================================================
# Signal Handlers
# ==============================================================================

tracker: Optional[TagTracker] = None

def signal_handler(signum, frame):
    """Handle termination signals gracefully."""
    logger.info(f"Received signal {signum}, initiating shutdown...")
    if tracker:
        tracker.running = False

# ==============================================================================
# Main Entry Point
# ==============================================================================

def main() -> int:
    """Main entry point."""
    global tracker
    
    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create and run tracker
    tracker = TagTracker()
    return tracker.run()

if __name__ == "__main__":
    sys.exit(main())
