# UWB Bridge Load Testing Guide

## Quick Start

### 1. Start the UWB Bridge Service
```bash
cd ~/Drobot/Transform_frames/uwb_bridge_cpp
./build/bin/uwb_bridge -c config/app_config.json
```

### 2. Run Load Test (in another terminal)

**Burst Test - 1000 messages at once:**
```bash
cd ~/Drobot/Transform_frames/uwb_bridge_cpp
python3 tools/load_test.py -n 1000 -m burst
```

**Steady Test - 500 messages at 100/sec:**
```bash
python3 tools/load_test.py -n 500 -m steady
```

**Test with production broker:**
```bash
python3 tools/load_test.py -H 192.168.1.166 -n 1000
```

## Test Options

```bash
python3 tools/load_test.py --help

Options:
  -H, --host HOST         MQTT broker host (default: localhost)
  -p, --port PORT         MQTT broker port (default: 1883)
  -n, --num-messages N    Number of messages (default: 1000)
  -m, --mode {burst|steady}  
                          burst: send all at once
                          steady: ~100 msg/sec
  -q, --qos {0,1,2}       MQTT QoS level (default: 1)
```

## What the Test Does

1. **Publishes** dummy UWB messages in Pozyx format to `tags/#` topics
2. **Subscribes** to `processed/tags/#` to receive transformed messages
3. **Measures** end-to-end latency from publish to receipt
4. **Reports** statistics:
   - Message success rate
   - Min/Max/Mean/Median latency
   - P50/P95/P99 percentiles
   - Throughput

## Expected Output

```
============================================================
UWB MQTT Bridge Load Tester
============================================================
Broker:   localhost:1883
Messages: 1000
Mode:     BURST
QoS:      1

✓ Publisher connected to localhost:1883
✓ Subscriber connected to localhost:1883
✓ Subscribed to processed/tags/#

============================================================
Publishing 1000 messages...
Mode: BURST (QoS=1)
============================================================

  Sent 100/1000 messages...
  Sent 200/1000 messages...
  ...
  Sent 1000/1000 messages...

✓ Published 1000 messages in 2.34s
  Throughput: 427.4 msg/sec

Waiting for processed messages (timeout: 30s)...
  Received: 100/1000
  Received: 500/1000
  Received: 1000/1000

✓ Received 1000 processed messages in 5.67s

============================================================
PERFORMANCE STATISTICS
============================================================

Message Flow:
  Sent:     1000
  Received: 1000
  Lost:     0
  Success:  100.0%

End-to-End Latency (ms):
  Min:      2.34
  Max:      45.67
  Mean:     8.92
  Median:   7.23
  StdDev:   5.12

Percentiles:
  P50:      7.23
  P95:      18.45
  P99:      32.11

============================================================
```

## Monitoring Bridge Performance

While the load test runs, watch the bridge logs for `[LATENCY]` and `[PUBLISH]` entries:

```
[LATENCY] Tag 99: Transform=245μs, Total=250μs | (2389.00, 3505.00)mm -> (2.389, 3.505)m
[PUBLISH] Publish=1200μs, End-to-end=1450μs
```

## Performance Targets

**Good Performance:**
- P95 latency: < 20ms
- Success rate: > 99%
- Transform time: < 500μs
- No message loss at 1000 msg burst

**If Performance Issues:**
1. Check CPU usage: `top` or `htop`
2. Check MQTT broker load
3. Increase log level to "warn" in app_config.json
4. Consider message batching or queuing

## Stress Testing

For extreme load testing:

```bash
# 10,000 messages burst
python3 tools/load_test.py -n 10000

# Run multiple publishers in parallel
for i in {1..5}; do
  python3 tools/load_test.py -n 1000 &
done
wait
```

## Dependencies

The load test script requires:
```bash
pip3 install paho-mqtt
```

Or system package:
```bash
sudo apt install python3-paho-mqtt
```
