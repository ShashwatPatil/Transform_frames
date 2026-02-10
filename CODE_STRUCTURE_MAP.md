# Code Structure Map - Firestore Data Access

## Overview

This document shows **exactly where in the code** the Firestore structure is defined - which collections, documents, and fields to access.

## Previous vs Current

### Previous (JSON Files)
```cpp
// Old way - Read from local files
ConfigLoader::loadFromFile("config/app_config.json");
FloorplanTransformer::fromConfigFile("config/transform_config.json");
```

### Current (Firestore)
```cpp
// New way - Read from Firestore
FirestoreManager::fetchAppConfig();           // Reads from Firestore
FirestoreManager::startTransformListener();   // Listens to Firestore
```

## Firestore Structure Definition in Code

### 1. Collection and Document Paths

**File**: `uwb_bridge_cpp/src/FirestoreManager.cpp`

#### AppConfig Location (Line ~145)
```cpp
std::future<AppConfig> FirestoreManager::fetchAppConfig() {
    // ...
    
    // 🎯 THIS IS WHERE THE PATH IS DEFINED
    auto doc_ref = db_->Collection("configs").Document("app_config");
    //                              ^^^^^^^^              ^^^^^^^^^^
    //                           Collection ID         Document ID
    
    doc_ref.Get().OnCompletion([](const DocumentSnapshot& snapshot) {
        // Read fields from this document
    });
}
```

**Firestore Path**: `projects/nova-40708/databases/(default)/documents/configs/app_config`

#### TransformConfig Location (Line ~185)
```cpp
bool FirestoreManager::startTransformListener(...) {
    // ...
    
    // 🎯 THIS IS WHERE THE PATH IS DEFINED
    auto doc_ref = db_->Collection("configs").Document("transform_config");
    //                              ^^^^^^^^              ^^^^^^^^^^^^^^^^^
    //                           Collection ID            Document ID
    
    transform_listener_ = doc_ref.AddSnapshotListener([](const DocumentSnapshot& snapshot) {
        // Listen for changes to this document
    });
}
```

**Firestore Path**: `projects/nova-40708/databases/(default)/documents/configs/transform_config`

### 2. Field Mappings - AppConfig

**File**: `uwb_bridge_cpp/src/FirestoreManager.cpp`  
**Function**: `parseAppConfig()` (Lines 270-360)

This is where each Firestore field is mapped to C++ config variables:

```cpp
AppConfig FirestoreManager::parseAppConfig(const DocumentSnapshot& snapshot) {
    AppConfig config;
    
    // 🎯 MQTT FIELDS - These field names must match Firestore
    config.mqtt.dual_mode = snapshot.Get("mqtt_dual_mode").boolean_value();
    //                                   ^^^^^^^^^^^^^^^^^
    //                               Firestore field name
    
    config.mqtt.source_broker.broker_address = snapshot.Get("mqtt_broker").string_value();
    //                                                       ^^^^^^^^^^^^
    //                                                  Firestore field name
    
    config.mqtt.source_broker.client_id = snapshot.Get("mqtt_client_id").string_value();
    config.mqtt.source_broker.username = snapshot.Get("mqtt_username").string_value();
    config.mqtt.source_broker.password = snapshot.Get("mqtt_password").string_value();
    config.mqtt.source_broker.source_topic = snapshot.Get("mqtt_source_topic").string_value();
    config.mqtt.source_broker.dest_topic_prefix = snapshot.Get("mqtt_dest_topic_prefix").string_value();
    config.mqtt.source_broker.qos = snapshot.Get("mqtt_qos").integer_value();
    config.mqtt.source_broker.keepalive_interval = snapshot.Get("mqtt_keepalive").integer_value();
    config.mqtt.source_broker.clean_session = snapshot.Get("mqtt_clean_session").boolean_value();
    config.mqtt.source_broker.use_ssl = snapshot.Get("mqtt_use_ssl").boolean_value();
    
    // 🎯 LOGGING FIELDS
    config.log_level = snapshot.Get("log_level").string_value();
    config.log_file = snapshot.Get("log_file").string_value();
    config.log_rotation_size_mb = snapshot.Get("log_rotation_size_mb").integer_value();
    config.log_rotation_count = snapshot.Get("log_rotation_count").integer_value();
    
    return config;
}
```

### 3. Field Mappings - TransformConfig

**File**: `uwb_bridge_cpp/src/FirestoreManager.cpp`  
**Function**: `parseTransformConfig()` (Lines 370-385)

```cpp
TransformConfig FirestoreManager::parseTransformConfig(const DocumentSnapshot& snapshot) {
    TransformConfig config;
    
    // 🎯 TRANSFORM FIELDS - These field names must match Firestore
    config.origin_x = snapshot.Get("origin_x").double_value();
    //                              ^^^^^^^^^
    //                         Firestore field name
    
    config.origin_y = snapshot.Get("origin_y").double_value();
    config.scale = snapshot.Get("scale").double_value();
    config.rotation_rad = snapshot.Get("rotation_rad").double_value();
    config.x_flipped = snapshot.Get("x_flipped").boolean_value();
    config.y_flipped = snapshot.Get("y_flipped").boolean_value();
    config.frame_id = snapshot.Get("frame_id").string_value();
    config.output_units = snapshot.Get("output_units").string_value();
    
    return config;
}
```

## Complete Mapping Table

### AppConfig Document (`configs/app_config`)

| Firestore Field | Type | C++ Variable | Line in Code |
|----------------|------|--------------|--------------|
| `mqtt_dual_mode` | boolean | `config.mqtt.dual_mode` | ~278 |
| `mqtt_broker` | string | `config.mqtt.source_broker.broker_address` | ~314 |
| `mqtt_client_id` | string | `config.mqtt.source_broker.client_id` | ~316 |
| `mqtt_username` | string | `config.mqtt.source_broker.username` | ~318 |
| `mqtt_password` | string | `config.mqtt.source_broker.password` | ~320 |
| `mqtt_source_topic` | string | `config.mqtt.source_broker.source_topic` | ~322 |
| `mqtt_dest_topic_prefix` | string | `config.mqtt.source_broker.dest_topic_prefix` | ~324 |
| `mqtt_qos` | number | `config.mqtt.source_broker.qos` | ~326 |
| `mqtt_keepalive` | number | `config.mqtt.source_broker.keepalive_interval` | ~328 |
| `mqtt_clean_session` | boolean | `config.mqtt.source_broker.clean_session` | ~330 |
| `mqtt_use_ssl` | boolean | `config.mqtt.source_broker.use_ssl` | ~332 |
| `log_level` | string | `config.log_level` | ~351 |
| `log_file` | string | `config.log_file` | ~352 |
| `log_rotation_size_mb` | number | `config.log_rotation_size_mb` | ~353 |
| `log_rotation_count` | number | `config.log_rotation_count` | ~355 |

### TransformConfig Document (`configs/transform_config`)

| Firestore Field | Type | C++ Variable | Line in Code |
|----------------|------|--------------|--------------|
| `origin_x` | number | `config.origin_x` | ~377 |
| `origin_y` | number | `config.origin_y` | ~378 |
| `scale` | number | `config.scale` | ~379 |
| `rotation_rad` | number | `config.rotation_rad` | ~380 |
| `x_flipped` | boolean | `config.x_flipped` | ~381 |
| `y_flipped` | boolean | `config.y_flipped` | ~382 |
| `frame_id` | string | `config.frame_id` | ~383 |
| `output_units` | string | `config.output_units` | ~384 |

## How to Change the Structure

### To Change Collection or Document Name

Edit `FirestoreManager.cpp`:

```cpp
// Current:
auto doc_ref = db_->Collection("configs").Document("app_config");

// To change collection name:
auto doc_ref = db_->Collection("my_custom_collection").Document("app_config");
//                              ^^^^^^^^^^^^^^^^^^^^^

// To change document name:
auto doc_ref = db_->Collection("configs").Document("my_app_settings");
//                                                  ^^^^^^^^^^^^^^^^^
```

### To Add/Remove Fields

Edit the `parseAppConfig()` or `parseTransformConfig()` functions:

```cpp
// Add a new field
config.my_new_setting = snapshot.Get("my_new_field").string_value();
//                                   ^^^^^^^^^^^^^
//                            Must match Firestore field name

// Then update ConfigLoader.hpp to add the field to the struct
struct AppConfig {
    // ... existing fields ...
    std::string my_new_setting;  // Add here
};
```

### To Change Field Names

```cpp
// Old:
config.log_level = snapshot.Get("log_level").string_value();

// New (if you renamed field in Firestore to "logging_level"):
config.log_level = snapshot.Get("logging_level").string_value();
//                              ^^^^^^^^^^^^^^
//                        Must match new Firestore field name
```

## Visual Flow Diagram

```
┌────────────────────────────────────────────────────────────┐
│               Firestore Database (nova-40708)              │
│                                                            │
│  Collection: "configs"                                     │
│  ├─ Document: "app_config"                                │
│  │  ├─ Field: mqtt_broker = "tcp://..."                   │
│  │  ├─ Field: mqtt_source_topic = "tags/#"                │
│  │  ├─ Field: mqtt_qos = 1                                │
│  │  └─ Field: log_level = "info"                          │
│  │                                                         │
│  └─ Document: "transform_config"                          │
│     ├─ Field: origin_x = 1000.0                           │
│     ├─ Field: origin_y = 2000.0                           │
│     ├─ Field: scale = 0.5                                 │
│     └─ Field: rotation_rad = 1.5708                       │
└────────────────────────────────────────────────────────────┘
                          │
                          │ Network (HTTPS)
                          │
                          ▼
┌────────────────────────────────────────────────────────────┐
│              FirestoreManager.cpp                          │
│                                                            │
│  📍 Line 145: Fetch app_config                            │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ auto doc_ref = db_->Collection("configs")            │ │
│  │                     .Document("app_config");          │ │
│  │                                                       │ │
│  │ doc_ref.Get() → parseAppConfig(snapshot)             │ │
│  └──────────────────────────────────────────────────────┘ │
│                          │                                 │
│                          ▼                                 │
│  📍 Line 270: Parse fields                                │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ snapshot.Get("mqtt_broker") → config.mqtt.broker     │ │
│  │ snapshot.Get("mqtt_source_topic") → config.topic     │ │
│  │ snapshot.Get("log_level") → config.log_level         │ │
│  └──────────────────────────────────────────────────────┘ │
│                          │                                 │
│                          ▼                                 │
│  📍 Line 185: Listen to transform_config                  │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ auto doc_ref = db_->Collection("configs")            │ │
│  │                     .Document("transform_config");    │ │
│  │                                                       │ │
│  │ doc_ref.AddSnapshotListener() → Real-time updates    │ │
│  └──────────────────────────────────────────────────────┘ │
│                          │                                 │
│                          ▼                                 │
│  📍 Line 370: Parse transform fields                      │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ snapshot.Get("origin_x") → config.origin_x           │ │
│  │ snapshot.Get("scale") → config.scale                 │ │
│  │ snapshot.Get("rotation_rad") → config.rotation_rad   │ │
│  └──────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌────────────────────────────────────────────────────────────┐
│                   Your Application                         │
│                                                            │
│  AppConfig config;                                         │
│    config.mqtt.broker_address = "tcp://..."               │
│    config.mqtt.source_topic = "tags/#"                    │
│    config.log_level = "info"                              │
│                                                            │
│  TransformConfig transform;                                │
│    transform.origin_x = 1000.0                            │
│    transform.scale = 0.5                                  │
└────────────────────────────────────────────────────────────┘
```

## Summary

The Firestore structure is defined in these specific locations:

1. **Collection/Document Paths**: Lines 145 and 185 in `FirestoreManager.cpp`
   - `db_->Collection("configs").Document("app_config")`
   - `db_->Collection("configs").Document("transform_config")`

2. **Field Mappings**: Lines 270-360 in `FirestoreManager.cpp`
   - Each `snapshot.Get("field_name")` defines a Firestore field
   - Field names must EXACTLY match what's in your Firestore database

3. **To Modify**:
   - Change paths → Edit lines 145 and 185
   - Add/remove fields → Edit `parseAppConfig()` and `parseTransformConfig()`
   - Rename fields → Update all `snapshot.Get("field_name")` calls

This is the equivalent of the JSON file structure, but now it's code that reads from Firestore instead of files!
