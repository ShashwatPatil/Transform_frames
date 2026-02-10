# Firebase Firestore Migration Checklist

Use this checklist to track your migration progress.

## Pre-Migration

### Firebase Setup
- [ ] Firebase account created
- [ ] Firebase project created
- [ ] Firestore Database enabled (Production mode)
- [ ] Project ID and API Key obtained
- [ ] Firestore security rules configured
- [ ] Service account created (optional, for production)

### Local Environment
- [ ] Linux development environment ready
- [ ] CMake 3.15+ installed
- [ ] C++17 compiler available (GCC 7+ or Clang 5+)
- [ ] Git repository up to date
- [ ] Build dependencies installed (Eigen3, etc.)

## Installation

### Firebase C++ SDK
- [ ] Firebase SDK downloaded (v11.0.0 or later)
- [ ] SDK extracted to `/opt/firebase_cpp_sdk`
- [ ] Verified: `libfirebase_app.a` exists
- [ ] Verified: `libfirebase_firestore.a` exists
- [ ] Include directory verified: `/opt/firebase_cpp_sdk/include`

### Environment Variables
- [ ] `FIREBASE_PROJECT_ID` set
- [ ] `FIREBASE_API_KEY` set
- [ ] `FIREBASE_CPP_SDK_DIR` set
- [ ] `LD_LIBRARY_PATH` updated with Firebase libs
- [ ] Variables added to `~/.bashrc` or `~/.profile`
- [ ] Current shell reloaded: `source ~/.bashrc`

## Firestore Configuration

### Database Structure
- [ ] Collection `configs` created
- [ ] Document `configs/app_config` created
- [ ] Document `configs/transform_config` created
- [ ] All required fields populated in `app_config`
- [ ] All required fields populated in `transform_config`
- [ ] Test data values verified

### Security Rules
- [ ] Security rules configured
- [ ] Rules allow authenticated read access
- [ ] Rules allow authenticated write access
- [ ] Rules tested in Firestore Console

### Test Data
- [ ] Sample `app_config` matches your MQTT setup
- [ ] Sample `transform_config` has valid transform values
- [ ] Tested reading documents via Python/JavaScript
- [ ] Tested writing/updating documents

## Code Changes

### libuwb_transform
- [ ] Code changes applied to `CMakeLists.txt`
- [ ] Code changes applied to `FloorplanTransformer.hpp`
- [ ] Code changes applied to `FloorplanTransformer.cpp`
- [ ] No compilation errors
- [ ] No linting/static analysis warnings

### uwb_bridge_cpp
- [ ] Code changes applied to `CMakeLists.txt`
- [ ] `FirestoreManager.hpp` created
- [ ] `FirestoreManager.cpp` created
- [ ] Code changes applied to `main.cpp`
- [ ] No compilation errors
- [ ] No linting/static analysis warnings

## Building

### libuwb_transform
- [ ] Build directory created/cleaned
- [ ] CMake configuration successful
- [ ] Firebase SDK detected by CMake
- [ ] Build completed without errors
- [ ] Shared library created: `libuwb_transform.so`

### uwb_bridge_cpp
- [ ] Build directory created/cleaned
- [ ] CMake configuration successful
- [ ] Firebase SDK detected by CMake
- [ ] Transform library linked correctly
- [ ] Build completed without errors
- [ ] Executable created: `bin/uwb_bridge`

## Testing

### Backward Compatibility (JSON Mode)
- [ ] Run with existing JSON config: `./bin/uwb_bridge -c config/app_config.json`
- [ ] Application starts successfully
- [ ] MQTT connection established
- [ ] Transform operations work correctly
- [ ] No crashes or errors
- [ ] Performance unchanged

### Firestore Mode (Basic)
- [ ] Environment variables verified
- [ ] Run with Firestore: `./bin/uwb_bridge --firestore`
- [ ] Firebase initialization successful
- [ ] AppConfig fetched from Firestore
- [ ] Application starts successfully
- [ ] MQTT connection established
- [ ] Transform operations work correctly

### Firestore Mode (Real-Time Updates)
- [ ] Application running in Firestore mode
- [ ] Open Firestore Console
- [ ] Update `transform_config` document
- [ ] Change `rotation_rad` value
- [ ] Log message: "Updated transform matrix from Firestore"
- [ ] Transforms use new configuration
- [ ] No application restart needed
- [ ] Multiple updates work correctly

### Thread Safety
- [ ] Multiple transform operations concurrent with config update
- [ ] No crashes during concurrent access
- [ ] No data corruption
- [ ] Performance acceptable under load

### Error Handling
- [ ] Graceful handling of missing Firestore documents
- [ ] Proper error messages for connection failures
- [ ] Timeout handling works correctly
- [ ] Application doesn't crash on Firestore errors
- [ ] Logging provides useful debug information

## Performance

### Benchmarks
- [ ] Startup time measured (JSON mode)
- [ ] Startup time measured (Firestore mode)
- [ ] Startup overhead acceptable (<2 seconds)
- [ ] Transform operation latency unchanged
- [ ] Memory usage measured and acceptable
- [ ] CPU usage normal

### Load Testing
- [ ] High-frequency transform operations tested
- [ ] Concurrent config updates tested
- [ ] Memory leaks checked (valgrind)
- [ ] Long-running stability verified (>1 hour)
- [ ] Resource usage stable over time

### Firestore Usage
- [ ] Read operations count verified in Firebase Console
- [ ] Write operations count verified
- [ ] Within free tier limits
- [ ] Listener connections stable
- [ ] No unexpected charges

## Documentation

### Code Documentation
- [ ] Comments added to new code
- [ ] Doxygen comments updated
- [ ] README files updated
- [ ] Build instructions current
- [ ] Usage examples provided

### User Documentation
- [ ] FIREBASE_README.md reviewed
- [ ] FIREBASE_MIGRATION.md reviewed
- [ ] FIRESTORE_SETUP.md reviewed
- [ ] QUICK_REFERENCE.md reviewed
- [ ] IMPLEMENTATION_SUMMARY.md reviewed

## Deployment

### Staging Environment
- [ ] Code deployed to staging
- [ ] Firestore staging database configured
- [ ] Staging tests passed
- [ ] Performance acceptable
- [ ] No issues identified

### Production Preparation
- [ ] Systemd service file created/updated
- [ ] Service file tested
- [ ] Firestore production database configured
- [ ] Production security rules reviewed
- [ ] Backup plan prepared
- [ ] Rollback plan prepared

### Production Deployment
- [ ] Production credentials configured
- [ ] Service deployed
- [ ] Health checks passing
- [ ] Monitoring configured
- [ ] Logs accessible
- [ ] Real-time updates verified

## Monitoring & Maintenance

### Operational
- [ ] Application logs monitored
- [ ] Firebase Console usage monitored
- [ ] Alert thresholds configured
- [ ] On-call procedures updated
- [ ] Incident response plan ready

### Post-Deployment
- [ ] First 24 hours monitoring completed
- [ ] No critical issues
- [ ] Performance meets SLAs
- [ ] User feedback collected
- [ ] Documentation finalized

## Rollback Plan

### Preparation
- [ ] Previous version preserved
- [ ] Rollback procedure documented
- [ ] Rollback tested in staging
- [ ] JSON config files preserved

### If Needed
- [ ] Stop new service
- [ ] Deploy previous version
- [ ] Switch back to JSON mode
- [ ] Verify functionality
- [ ] Document lessons learned

## Post-Migration

### Optimization
- [ ] Performance tuning completed
- [ ] Resource usage optimized
- [ ] Firestore usage optimized
- [ ] Code cleanup done

### Knowledge Transfer
- [ ] Team trained on new system
- [ ] Documentation reviewed with team
- [ ] Operations procedures updated
- [ ] Support team briefed

### Future Enhancements
- [ ] Enhancement ideas documented
- [ ] Feature requests logged
- [ ] Technical debt identified
- [ ] Roadmap updated

---

## Summary

**Total Items:** ~130
**Completed:** ___ / 130

**Current Phase:** ______________

**Blockers:** 
- 
- 

**Notes:**
- 
- 

**Last Updated:** ___________
**Updated By:** ___________
