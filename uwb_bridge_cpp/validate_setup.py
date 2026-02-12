#!/usr/bin/env python3
"""
Configuration Validator for UWB Bridge Manager

This script validates that your setup is correct before running the bridge manager.
It checks:
1. Firebase credentials
2. Firestore connectivity
3. C++ executable
4. Configuration parsing

Usage: python3 validate_setup.py
"""

import json
import os
import sys
from pathlib import Path

# Color codes for terminal output
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
RESET = '\033[0m'

def print_header(text):
    """Print a formatted header."""
    print(f"\n{BLUE}{'=' * 60}{RESET}")
    print(f"{BLUE}{text:^60}{RESET}")
    print(f"{BLUE}{'=' * 60}{RESET}\n")

def print_success(text):
    """Print success message."""
    print(f"{GREEN}✓ {text}{RESET}")

def print_error(text):
    """Print error message."""
    print(f"{RED}✗ {text}{RESET}")

def print_warning(text):
    """Print warning message."""
    print(f"{YELLOW}⚠ {text}{RESET}")

def check_python_version():
    """Check Python version."""
    print("Checking Python version...")
    if sys.version_info < (3, 8):
        print_error(f"Python 3.8+ required, found {sys.version_info.major}.{sys.version_info.minor}")
        return False
    print_success(f"Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")
    return True

def check_firebase_credentials():
    """Check if Firebase credentials file exists."""
    print("\nChecking Firebase credentials...")
    cred_file = "nova_database_cred.json"
    
    if not os.path.exists(cred_file):
        print_error(f"Credentials file not found: {cred_file}")
        print(f"  → Place your service account JSON at: {os.path.abspath(cred_file)}")
        return False
    
    # Try to parse it
    try:
        with open(cred_file, 'r') as f:
            creds = json.load(f)
        
        required_fields = ['project_id', 'private_key', 'client_email']
        missing = [f for f in required_fields if f not in creds]
        
        if missing:
            print_error(f"Invalid credentials file. Missing fields: {', '.join(missing)}")
            return False
        
        print_success(f"Credentials file found: {cred_file}")
        print(f"  Project ID: {creds.get('project_id')}")
        print(f"  Client Email: {creds.get('client_email')}")
        return True
        
    except json.JSONDecodeError as e:
        print_error(f"Invalid JSON in credentials file: {e}")
        return False
    except Exception as e:
        print_error(f"Error reading credentials: {e}")
        return False

def check_firebase_module():
    """Check if firebase-admin is installed."""
    print("\nChecking Firebase Admin SDK...")
    try:
        import firebase_admin
        print_success(f"firebase-admin installed (version {firebase_admin.__version__})")
        return True
    except ImportError as e:
        print_error("firebase-admin not installed")
        print(f"  → Install with: pip install --user firebase-admin")
        print(f"  Import error details: {e}")
        return False
    except Exception as e:
        print_error(f"firebase-admin import failed: {e}")
        print("  This may be due to package conflicts. Try:")
        print("  → pip install --user --upgrade firebase-admin google-auth requests urllib3")
        return False

def check_cpp_executable():
    """Check if C++ executable exists and is executable."""
    print("\nChecking C++ executable...")
    exe_path = "./build/bin/uwb_bridge"
    
    if not os.path.exists(exe_path):
        print_error(f"C++ executable not found: {exe_path}")
        print("  → Build with: ./build.sh")
        return False
    
    if not os.access(exe_path, os.X_OK):
        print_warning(f"Executable found but not executable: {exe_path}")
        print("  → Fix with: chmod +x bin/uwb_bridge")
        return False
    
    print_success(f"Executable found: {exe_path}")
    
    # Check binary info
    file_size = os.path.getsize(exe_path) / (1024 * 1024)
    print(f"  Size: {file_size:.2f} MB")
    
    return True

def check_firestore_connection():
    """Test Firestore connection."""
    print("\nTesting Firestore connection...")
    
    try:
        import firebase_admin
        from firebase_admin import credentials, firestore
        
        # Initialize Firebase
        cred = credentials.Certificate("nova_database_cred.json")
        
        # Check if already initialized
        try:
            app = firebase_admin.get_app()
            print_warning("Firebase already initialized, skipping")
        except ValueError:
            app = firebase_admin.initialize_app(cred)
        
        db = firestore.client()
        
        # Try to connect
        print("  Attempting to connect to Firestore...")
        
        # Just list collections to test connection
        collections = list(db.collections())
        
        print_success("Successfully connected to Firestore")
        print(f"  Found {len(collections)} collection(s)")
        
        return True
        
    except Exception as e:
        print_error(f"Failed to connect to Firestore: {e}")
        print("  Check:")
        print("    - Internet connection")
        print("    - Service account permissions")
        print("    - Firestore is enabled in Firebase project")
        return False

def check_config_directory():
    """Check if config directory exists."""
    print("\nChecking configuration directory...")
    config_dir = "config"
    
    if not os.path.exists(config_dir):
        print_warning(f"Config directory not found: {config_dir}")
        print("  Creating directory...")
        os.makedirs(config_dir, exist_ok=True)
        print_success(f"Created: {config_dir}")
    else:
        print_success(f"Config directory exists: {config_dir}")
    
    return True

def main():
    """Main validation function."""
    print_header("UWB Bridge Manager - Setup Validator")
    
    checks = [
        ("Python Version", check_python_version),
        ("Firebase Credentials", check_firebase_credentials),
        ("Firebase Admin SDK", check_firebase_module),
        ("C++ Executable", check_cpp_executable),
        ("Config Directory", check_config_directory),
    ]
    
    results = []
    
    # Run basic checks
    for name, check_func in checks:
        try:
            result = check_func()
            results.append((name, result))
        except Exception as e:
            print_error(f"Unexpected error in {name}: {e}")
            results.append((name, False))
    
    # Firestore connection test (optional, requires credentials)
    if results[1][1] and results[2][1]:  # If credentials and SDK are OK
        try:
            result = check_firestore_connection()
            results.append(("Firestore Connection", result))
        except Exception as e:
            print_error(f"Unexpected error testing Firestore: {e}")
            results.append(("Firestore Connection", False))
    
    # Summary
    print_header("Validation Summary")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for name, result in results:
        status = f"{GREEN}PASS{RESET}" if result else f"{RED}FAIL{RESET}"
        print(f"  {name:.<40} {status}")
    
    print(f"\n{passed}/{total} checks passed")
    
    if passed == total:
        print_success("\n🎉 All checks passed! You're ready to run the bridge manager.")
        print(f"\n{BLUE}Start with:{RESET} python3 bridge_manager.py")
        return 0
    else:
        print_error(f"\n❌ {total - passed} check(s) failed. Please fix the issues above.")
        return 1

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print(f"\n\n{YELLOW}Validation cancelled by user{RESET}")
        sys.exit(1)
    except Exception as e:
        print_error(f"\nUnexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
