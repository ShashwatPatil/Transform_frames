#!/bin/bash
# Test HiveMQ connection with mosquitto

HIVEMQ_HOST="63f9c72485ab42b28b77c5640699e17b.s1.eu.hivemq.cloud"
HIVEMQ_PORT="8883"

echo "==================================================="
echo "Testing HiveMQ Cloud Connection"
echo "==================================================="
echo ""
echo "Please enter your HiveMQ credentials:"
read -p "Username: " USERNAME
read -sp "Password: " PASSWORD
echo ""
echo ""

echo "Testing connection..."
echo "Subscribing to test topic for 5 seconds..."

timeout 5s mosquitto_sub \
  -h "$HIVEMQ_HOST" \
  -p "$HIVEMQ_PORT" \
  -t "uwb/test/#" \
  -u "$USERNAME" \
  -P "$PASSWORD" \
  --capath /etc/ssl/certs \
  -v

EXIT_CODE=$?

echo ""
echo "==================================================="
if [ $EXIT_CODE -eq 124 ]; then
  echo "✓ Connection successful! (timeout as expected)"
  echo "Your credentials are working."
elif [ $EXIT_CODE -eq 0 ]; then
  echo "✓ Connection successful!"
  echo "Your credentials are working."
else
  echo "✗ Connection failed with exit code: $EXIT_CODE"
  echo ""
  echo "Common error codes:"
  echo "  5 = Not authorized (wrong username/password)"
  echo "  1 = Connection refused"
  echo ""
  echo "Troubleshooting:"
  echo "  1. Check username/password (case-sensitive)"
  echo "  2. Verify cluster is active in HiveMQ dashboard"
  echo "  3. Check firewall allows port 8883"
fi
echo "==================================================="
