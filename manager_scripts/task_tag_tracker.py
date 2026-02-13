#!/usr/bin/env python3
"""
Task Tag Tracker - High-Frequency Tag Streaming for Active Tasks
=================================================================

This script monitors assigned tasks and streams high-frequency position data
for tags involved in active tasks.

Architecture:
1. MQTT callbacks update tag cache continuously
2. Firebase RTDB listener monitors assigned_tasks
3. Periodic writer streams tags involved in tasks at 10Hz
4. No movement threshold - streams all position updates for task tags

Author: Generated for UWB Task Tag Tracking
Version: 1.0.0
"""

import json
import logging
import os
import signal
import sys
import threading
import time
from datetime import datetime, timezone
from typing import Dict, Optional, Set

import firebase_admin
import paho.mqtt.client as mqtt
from firebase_admin import credentials, db, firestore

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

# RTDB paths
RTDB_ASSIGNED_TASKS_PATH = "setups/&GSP&Office&29607/assigned_tasks"
RTDB_TASK_TAGS_PATH = "pozyx/&GSP&Office&29607/task_tags"

# Streaming rate for task tags (Hz) - 10Hz = 100ms between updates
TASK_TAG_STREAM_RATE_HZ = 10.0
TASK_TAG_STREAM_INTERVAL = 1.0 / TASK_TAG_STREAM_RATE_HZ

# ==============================================================================
# Logging Setup
# ==============================================================================

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] [TaskTagTracker] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# ==============================================================================
# TaskTagTracker Class
# ==============================================================================

class TaskTagTracker:
    """Streams high-frequency position data for tags involved in active tasks."""
    
    def __init__(self):
        """Initialize the Task Tag Tracker."""
        self.mqtt_client: Optional[mqtt.Client] = None
        self.running = False
        
        # Tag data cache: tag_id -> {x, y, z, last_seen_time}
        self.tag_cache: Dict[str, Dict] = {}
        
        # Set of tag IDs currently involved in active tasks
        self.active_task_tags: Set[str] = set()
        
        # Lock for thread-safe access to cache and active tags
        self.cache_lock = threading.Lock()
        
        # Worker threads
        self.periodic_writer_thread: Optional[threading.Thread] = None
        
        # MQTT configuration
        self.mqtt_config: Optional[Dict] = None
        
        # Firebase clients
        self.firestore_db: Optional[firestore.Client] = None
        self.config_listener = None
        self.tasks_listener = None
    
    def parse_mqtt_config(self, dest_broker: Dict) -> Optional[Dict]:
        """
        Parse MQTT configuration from Firestore dest_broker data.
        
        Args:
            dest_broker: Destination broker configuration from Firestore
            
        Returns:
            Optional[Dict]: Parsed MQTT configuration or None if invalid
        """
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
    
    def initialize_firebase(self) -> bool:
        """
        Initialize Firebase Admin SDK with RTDB and Firestore support.
        
        Returns:
            bool: True if initialization succeeded, False otherwise
        """
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
        self.firestore_db = firestore.client()
        
        logger.info("Firebase RTDB and Firestore initialized successfully")
        logger.info(f"Database URL: {FIREBASE_RTDB_URL}")
        return True
    
    def parse_task_for_tags(self, task_data: Dict) -> Set[str]:
        """
        Parse task data to extract tag IDs that need to be tracked.
        
        DUMMY IMPLEMENTATION - To be replaced with actual logic later.
        
        Args:
            task_data: Task data from Firebase RTDB
            
        Returns:
            Set[str]: Set of tag IDs involved in this task
        """
        # TODO: Implement actual task parsing logic
        # For now, check if task has a 'follow_tag' or 'tags' field
        
        tag_ids = set()
        
        # Example: Check for various possible field names
        if 'follow_tag' in task_data:
            tag_ids.add(str(task_data['follow_tag']))
        
        if 'tag_id' in task_data:
            tag_ids.add(str(task_data['tag_id']))
        
        if 'tags' in task_data and isinstance(task_data['tags'], list):
            tag_ids.update(str(tid) for tid in task_data['tags'])
        
        if 'target_tag' in task_data:
            tag_ids.add(str(task_data['target_tag']))
        
        return tag_ids
    
    def on_tasks_change(self, event):
        """
        Callback for RTDB assigned_tasks listener.
        
        Args:
            event: Firebase RTDB event containing task changes
        """
        if event.event_type == 'put':
            # Full data or new child
            path = event.path
            data = event.data
            
            if data is None:
                # Tasks collection deleted
                logger.info("All tasks removed")
                with self.cache_lock:
                    self.active_task_tags.clear()
                return
            
            # Rebuild active task tags from all tasks
            new_active_tags = set()
            
            if isinstance(data, dict):
                for task_id, task_data in data.items():
                    if isinstance(task_data, dict):
                        # Check if task is active
                        task_status = task_data.get('status', 'active')
                        if task_status in ['active', 'in_progress', 'pending']:
                            # Parse task for involved tags
                            task_tags = self.parse_task_for_tags(task_data)
                            new_active_tags.update(task_tags)
                            if task_tags:
                                logger.info(f"Task {task_id} [{task_status}] involves tags: {', '.join(task_tags)}")
            
            # Update active task tags
            with self.cache_lock:
                old_tags = self.active_task_tags.copy()
                self.active_task_tags = new_active_tags
                
                # Log changes
                added = new_active_tags - old_tags
                removed = old_tags - new_active_tags
                
                if added:
                    logger.info(f"▶ Started streaming tags: {', '.join(added)}")
                if removed:
                    logger.info(f"■ Stopped streaming tags: {', '.join(removed)}")
                
                if not new_active_tags:
                    logger.info("No active task tags to stream")
                else:
                    logger.info(f"Active task tags: {', '.join(new_active_tags)}")
        
        elif event.event_type == 'patch':
            # Partial update - rebuild from current state
            logger.debug("Task data patched, fetching full state")
            # Re-fetch entire tasks to ensure consistency
            tasks_ref = db.reference(RTDB_ASSIGNED_TASKS_PATH)
            all_tasks = tasks_ref.get()
            
            if all_tasks:
                # Process like 'put' event
                new_active_tags = set()
                for task_id, task_data in all_tasks.items():
                    if isinstance(task_data, dict):
                        task_status = task_data.get('status', 'active')
                        if task_status in ['active', 'in_progress', 'pending']:
                            task_tags = self.parse_task_for_tags(task_data)
                            new_active_tags.update(task_tags)
                
                with self.cache_lock:
                    self.active_task_tags = new_active_tags
    
    def periodic_writer_loop(self):
        """
        Periodic writer thread: Streams task tag positions at high frequency.
        
        Wakes up every TASK_TAG_STREAM_INTERVAL (100ms for 10Hz) and writes
        latest position for all tags involved in active tasks.
        """
        logger.info(f"Periodic writer thread started (rate: {TASK_TAG_STREAM_RATE_HZ}Hz)")
        
        while self.running:
            time.sleep(TASK_TAG_STREAM_INTERVAL)
            
            if not self.running:
                break
            
            batch_updates = {}
            
            with self.cache_lock:
                # Get copy of active task tags
                task_tags = self.active_task_tags.copy()
                
                # Stream data for each active task tag
                for tag_id in task_tags:
                    if tag_id in self.tag_cache:
                        cached = self.tag_cache[tag_id]
                        
                        # Prepare update
                        path = f"{RTDB_TASK_TAGS_PATH}/{tag_id}"
                        data = {
                            'x': cached['x'],
                            'y': cached['y'],
                            'z': cached['z'],
                            'timestamp': datetime.now(timezone.utc).isoformat()
                        }
                        batch_updates[path] = data
            
            # Perform batched write if there are updates
            if batch_updates:
                db.reference('/').update(batch_updates)
                logger.debug(f"▶ Streamed {len(batch_updates)} task tag(s)")
        
        logger.info("Periodic writer thread stopped")
    
    def process_tag_data(self, tag_id: str, x: float, y: float, z: float):
        """
        Process incoming tag data - updates cache only.
        
        Args:
            tag_id: Tag identifier
            x: X coordinate (meters)
            y: Y coordinate (meters)
            z: Z coordinate (meters)
        """
        current_time = time.time()
        
        with self.cache_lock:
            if tag_id in self.tag_cache:
                # Update existing tag
                self.tag_cache[tag_id]['x'] = x
                self.tag_cache[tag_id]['y'] = y
                self.tag_cache[tag_id]['z'] = z
                self.tag_cache[tag_id]['last_seen_time'] = current_time
            else:
                # New tag
                self.tag_cache[tag_id] = {
                    'x': x,
                    'y': y,
                    'z': z,
                    'last_seen_time': current_time
                }
                logger.debug(f"New tag in cache: {tag_id}")
    
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
        # Disconnect existing client if any
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            logger.info("Disconnected previous MQTT client")
        
        # Create new MQTT client
        client_id = f"task_tag_tracker_{int(time.time())}"
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
        self.setup_mqtt_client(new_mqtt_config)
        logger.info("MQTT client reconnected successfully with new configuration")
    
    def setup_firestore_listener(self) -> bool:
        """
        Setup Firestore listener for MQTT configuration updates.
        
        Returns:
            bool: True if listener setup succeeded, False otherwise
        """
        # Build Firestore document path
        doc_path = f"{FIRESTORE_COLLECTION}/{FIRESTORE_DOCUMENT}/{FIRESTORE_ENV_PATH}/{FIRESTORE_POZYX_DOC}"
        
        logger.info(f"Setting up Firestore listener on: {doc_path}")
        
        # Get document reference
        doc_ref = self.firestore_db.collection(FIRESTORE_COLLECTION).document(FIRESTORE_DOCUMENT).collection(FIRESTORE_ENV_PATH).document(FIRESTORE_POZYX_DOC)
        
        # Attach snapshot listener
        self.config_listener = doc_ref.on_snapshot(self.on_config_change)
        
        logger.info("Firestore listener attached successfully")
        return True
    
    def setup_rtdb_tasks_listener(self) -> bool:
        """
        Setup RTDB listener for assigned_tasks updates.
        
        Returns:
            bool: True if listener setup succeeded, False otherwise
        """
        logger.info(f"Setting up RTDB listener on: {RTDB_ASSIGNED_TASKS_PATH}")
        
        # Get reference to assigned_tasks
        tasks_ref = db.reference(RTDB_ASSIGNED_TASKS_PATH)
        
        # Attach listener
        self.tasks_listener = tasks_ref.listen(self.on_tasks_change)
        
        logger.info("RTDB tasks listener attached successfully")
        return True
    
    def run(self) -> int:
        """
        Main run loop for the Task Tag Tracker.
        
        Returns:
            int: Exit code (0 for success, non-zero for error)
        """
        logger.info("=================================================")
        logger.info("  Task Tag Tracker - High-Frequency Streaming")
        logger.info("  Version: 1.0.0")
        logger.info("=================================================")
        
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
        max_wait = 10
        wait_interval = 0.5
        elapsed = 0
        
        while not self.mqtt_config and elapsed < max_wait:
            time.sleep(wait_interval)
            elapsed += wait_interval
        
        if not self.mqtt_config:
            logger.critical("Timeout waiting for MQTT configuration from Firestore")
            return 1
        
        logger.info("Initial MQTT configuration loaded successfully")
        
        # Setup RTDB listener for assigned_tasks
        if not self.setup_rtdb_tasks_listener():
            logger.critical("Failed to setup RTDB tasks listener")
            return 1
        
        # Set running flag BEFORE starting threads
        self.running = True
        
        # Start periodic writer thread
        self.periodic_writer_thread = threading.Thread(
            target=self.periodic_writer_loop,
            daemon=True
        )
        self.periodic_writer_thread.start()
        logger.info("Periodic writer thread started")
        
        logger.info("Task Tag Tracker running. Press Ctrl+C to stop.")
        logger.info(f"Stream rate: {TASK_TAG_STREAM_RATE_HZ}Hz ({TASK_TAG_STREAM_INTERVAL*1000:.0f}ms interval)")
        logger.info(f"RTDB output: {RTDB_TASK_TAGS_PATH}")
        logger.info(f"Mode: High-frequency streaming (no movement threshold)")
        
        # Keep main thread alive
        while self.running:
            time.sleep(1)
        
        return 0
    
    def cleanup(self):
        """Cleanup resources before exit."""
        logger.info("Cleaning up...")
        
        self.running = False
        
        # Stop MQTT client
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            logger.info("MQTT client disconnected")
        
        # Stop Firestore listener
        if self.config_listener:
            self.config_listener.unsubscribe()
            logger.info("Firestore listener stopped")
        
        # Stop RTDB tasks listener
        if self.tasks_listener:
            self.tasks_listener.close()
            logger.info("RTDB tasks listener stopped")
        
        # Wait for periodic writer thread to stop
        if self.periodic_writer_thread and self.periodic_writer_thread.is_alive():
            logger.info("Waiting for periodic writer thread to stop...")
            self.periodic_writer_thread.join(timeout=5)
        
        logger.info("Cleanup complete")

# ==============================================================================
# Signal Handlers
# ==============================================================================

tracker: Optional[TaskTagTracker] = None

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
    tracker = TaskTagTracker()
    exit_code = 0
    
    # Run with cleanup in finally
    tracker.running = True
    exit_code = tracker.run()
    tracker.cleanup()
    
    return exit_code

if __name__ == "__main__":
    sys.exit(main())
