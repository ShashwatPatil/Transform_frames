#!/usr/bin/env python3
"""
Load testing script for UWB MQTT Bridge
Publishes dummy UWB messages to test performance and latency under load
"""

import argparse
import json
import random
import statistics
import threading
import time
from datetime import datetime

import paho.mqtt.client as mqtt


class LoadTester:
    def __init__(self, broker_host, broker_port, num_messages, burst_mode=True, qos=1, 
                 source_topic="tags", dest_topic="processed/tags", verbose=False):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.num_messages = num_messages
        self.burst_mode = burst_mode
        self.qos = qos
        self.source_topic = source_topic  # Where to publish (e.g., "tags" or "test/tags")
        self.dest_topic = dest_topic      # Where to subscribe (e.g., "processed/tags" or "test/processed")
        self.verbose = verbose
        
        # Statistics
        self.messages_sent = 0
        self.messages_received = 0
        self.latencies = []
        self.send_times = {}
        self.lock = threading.Lock()
        
        # Connection ready flags
        self.publisher_ready = False
        self.subscriber_ready = False
        
        # MQTT clients
        self.publisher = None
        self.subscriber = None
        
        # Sample tag IDs for testing
        # self.tag_ids = ["99", "111", "200000768", "test_tag_1", "test_tag_2"]
        
    def on_connect_pub(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"✓ Publisher connected to {self.broker_host}:{self.broker_port}")
            self.publisher_ready = True
        else:
            print(f"✗ Publisher connection failed with code {rc}")
            
    def on_connect_sub(self, client, userdata, flags, rc):
        if rc == 0:
            print(f"✓ Subscriber connected to {self.broker_host}:{self.broker_port}")
            # Subscribe to processed messages
            subscribe_topic = f"{self.dest_topic}/#"
            client.subscribe(subscribe_topic, qos=self.qos)
            print(f"✓ Subscribed to {subscribe_topic}")
            self.subscriber_ready = True
        else:
            print(f"✗ Subscriber connection failed with code {rc}")
    
    def on_message(self, client, userdata, msg):
        """Handle received processed messages"""
        receive_time = time.time()
        
        if self.verbose:
            print(f"[RECV] Topic: {msg.topic}, Payload: {msg.payload.decode()[:100]}...")
        
        try:
            payload = json.loads(msg.payload.decode())
            tag_id = payload.get("tag_id")
            
            if self.verbose:
                print(f"[RECV] Parsed tag_id: {tag_id}")
            
            # Calculate latency if we have the send time
            msg_key = f"{tag_id}"
            with self.lock:
                if msg_key in self.send_times:
                    latency_ms = (receive_time - self.send_times[msg_key]) * 1000
                    self.latencies.append(latency_ms)
                    del self.send_times[msg_key]
                    if self.verbose:
                        print(f"[LATENCY] {tag_id}: {latency_ms:.2f}ms")
                
                self.messages_received += 1
                
        except Exception as e:
            print(f"Error processing received message: {e}")
    
    def generate_pozyx_message(self, tag_id):
        """Generate a dummy Pozyx-format UWB message"""
        # Random coordinates within realistic range (0-50000 mm)
        x = random.randint(500, 50000)
        y = random.randint(500, 50000)
        z = random.randint(0, 3000)
        
        # Pozyx format: array with nested data.coordinates structure
        message = [{
            "tagId": tag_id,
            "data": {
                "coordinates": {
                    "x": x,
                    "y": y,
                    "z": z
                }
            }
        }]
        
        return json.dumps(message)
    
    def publish_messages(self):
        """Publish test messages"""
        # Wait for publisher to be ready
        print("\nWaiting for publisher connection...")
        timeout = 10
        start_wait = time.time()
        while not self.publisher_ready and (time.time() - start_wait) < timeout:
            time.sleep(0.1)
        
        if not self.publisher_ready:
            print("✗ Publisher not ready after timeout!")
            return
        
        print(f"\n{'='*60}")
        print(f"Publishing {self.num_messages} messages...")
        print(f"Mode: {'BURST' if self.burst_mode else 'STEADY'} (QoS={self.qos})")
        print(f"Topic: {self.source_topic}/<tag_id>")
        print(f"{'='*60}\n")
        
        start_time = time.time()
        
        for i in range(self.num_messages):
            tag_id = str(i)
            topic = f"{self.source_topic}/{tag_id}"
            payload = self.generate_pozyx_message(tag_id)
            
            if self.verbose and i < 3:
                print(f"[SEND] Topic: {topic}, Payload: {payload}")
            
            # Record send time for latency calculation
            msg_key = f"{tag_id}"
            send_time = time.time()
            with self.lock:
                self.send_times[msg_key] = send_time
            
            # Publish
            result = self.publisher.publish(topic, payload, qos=self.qos)
            
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                print(f"✗ Failed to publish message {i+1}")
            else:
                with self.lock:
                    self.messages_sent += 1
            
            # In steady mode, add small delay between messages
            if not self.burst_mode and i < self.num_messages - 1:
                time.sleep(0.01)  # 10ms delay = 100 msg/sec
            
            # Progress indicator
            if (i + 1) % 100 == 0:
                print(f"  Sent {i+1}/{self.num_messages} messages...")
        
        publish_duration = time.time() - start_time
        print(f"\n✓ Published {self.messages_sent} messages in {publish_duration:.2f}s")
        print(f"  Throughput: {self.messages_sent/publish_duration:.1f} msg/sec")
    
    def wait_for_responses(self, timeout=30):
        """Wait for processed messages to arrive"""
        print(f"\nWaiting for processed messages (timeout: {timeout}s)...")
        
        start_wait = time.time()
        last_count = 0
        
        while (time.time() - start_wait) < timeout:
            with self.lock:
                current_count = self.messages_received
            
            # Print progress
            if current_count != last_count:
                print(f"  Received: {current_count}/{self.messages_sent}")
                last_count = current_count
            
            # Check if all messages received
            if current_count >= self.messages_sent:
                break
            
            time.sleep(0.5)
        
        wait_duration = time.time() - start_wait
        print(f"\n✓ Received {self.messages_received} processed messages in {wait_duration:.2f}s")
    
    def print_statistics(self):
        """Print latency statistics"""
        print(f"\n{'='*60}")
        print("PERFORMANCE STATISTICS")
        print(f"{'='*60}")
        
        with self.lock:
            print(f"\nMessage Flow:")
            print(f"  Sent:     {self.messages_sent}")
            print(f"  Received: {self.messages_received}")
            print(f"  Lost:     {self.messages_sent - self.messages_received}")
            print(f"  Success:  {(self.messages_received/self.messages_sent*100):.1f}%")
            
            if self.latencies:
                print(f"\nEnd-to-End Latency (ms):")
                print(f"  Min:      {min(self.latencies):.2f}")
                print(f"  Max:      {max(self.latencies):.2f}")
                print(f"  Mean:     {statistics.mean(self.latencies):.2f}")
                print(f"  Median:   {statistics.median(self.latencies):.2f}")
                print(f"  StdDev:   {statistics.stdev(self.latencies):.2f}" if len(self.latencies) > 1 else "  StdDev:   N/A")
                
                # Percentiles
                sorted_latencies = sorted(self.latencies)
                p50 = sorted_latencies[len(sorted_latencies) * 50 // 100]
                p95 = sorted_latencies[len(sorted_latencies) * 95 // 100]
                p99 = sorted_latencies[len(sorted_latencies) * 99 // 100]
                
                print(f"\nPercentiles:")
                print(f"  P50:      {p50:.2f}")
                print(f"  P95:      {p95:.2f}")
                print(f"  P99:      {p99:.2f}")
            else:
                print("\n⚠ No latency data collected")
        
        print(f"\n{'='*60}\n")
    
    def run(self):
        """Run the load test"""
        try:
            # Setup publisher
            self.publisher = mqtt.Client(client_id="load_test_publisher")
            self.publisher.on_connect = self.on_connect_pub
            self.publisher.connect(self.broker_host, self.broker_port, keepalive=60)
            self.publisher.loop_start()
            
            # Setup subscriber
            self.subscriber = mqtt.Client(client_id="load_test_subscriber")
            self.subscriber.on_connect = self.on_connect_sub
            self.subscriber.on_message = self.on_message
            self.subscriber.connect(self.broker_host, self.broker_port, keepalive=60)
            self.subscriber.loop_start()
            
            # Wait for both connections to be ready
            print("\nWaiting for MQTT connections...")
            timeout = 10
            start_wait = time.time()
            while (not self.publisher_ready or not self.subscriber_ready) and (time.time() - start_wait) < timeout:
                time.sleep(0.1)
            
            if not self.publisher_ready or not self.subscriber_ready:
                print("✗ MQTT connections not ready!")
                print(f"  Publisher: {'✓' if self.publisher_ready else '✗'}")
                print(f"  Subscriber: {'✓' if self.subscriber_ready else '✗'}")
                return
            
            print("✓ Both MQTT clients ready\n")
            time.sleep(0.5)  # Extra safety margin
            
            # Publish messages
            self.publish_messages()
            
            # Wait for responses
            self.wait_for_responses()
            
            # Print statistics
            self.print_statistics()
            
        except KeyboardInterrupt:
            print("\n\n⚠ Test interrupted by user")
            self.print_statistics()
        
        finally:
            # Cleanup
            if self.publisher:
                self.publisher.loop_stop()
                self.publisher.disconnect()
            if self.subscriber:
                self.subscriber.loop_stop()
                self.subscriber.disconnect()


def main():
    parser = argparse.ArgumentParser(
        description='Load test the UWB MQTT Bridge',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Burst mode - send 1000 messages as fast as possible
  %(prog)s -n 1000 -m burst
  
  # Steady mode - send 500 messages at ~100/sec
  %(prog)s -n 500 -m steady
  
  # Test with production broker
  %(prog)s -H 192.168.1.166 -n 1000
  
  # Quick test with 100 messages
  %(prog)s -n 100
        """
    )
    
    parser.add_argument('-H', '--host', default='localhost',
                        help='MQTT broker host (default: localhost)')
    parser.add_argument('-p', '--port', type=int, default=1883,
                        help='MQTT broker port (default: 1883)')
    parser.add_argument('-n', '--num-messages', type=int, default=1000,
                        help='Number of messages to send (default: 1000)')
    parser.add_argument('-m', '--mode', choices=['burst', 'steady'], default='burst',
                        help='burst: send all at once, steady: 100 msg/sec (default: burst)')
    parser.add_argument('-q', '--qos', type=int, choices=[0, 1, 2], default=1,
                        help='MQTT QoS level (default: 1)')
    parser.add_argument('--source-topic', default='tags',
                        help='Source topic prefix to publish to (default: tags, for test config use: test/tags)')
    parser.add_argument('--dest-topic', default='processed/tags',
                        help='Destination topic prefix to subscribe (default: processed/tags, for test config use: test/processed)')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable verbose debug output')
    
    args = parser.parse_args()
    
    print(f"\n{'='*60}")
    print("UWB MQTT Bridge Load Tester")
    print(f"{'='*60}")
    print(f"Broker:       {args.host}:{args.port}")
    print(f"Messages:     {args.num_messages}")
    print(f"Mode:         {args.mode.upper()}")
    print(f"QoS:          {args.qos}")
    print(f"Source Topic: {args.source_topic}/#")
    print(f"Dest Topic:   {args.dest_topic}/#")
    print(f"Verbose:      {args.verbose}")
    
    tester = LoadTester(
        broker_host=args.host,
        broker_port=args.port,
        num_messages=args.num_messages,
        burst_mode=(args.mode == 'burst'),
        qos=args.qos,
        source_topic=args.source_topic,
        dest_topic=args.dest_topic,
        verbose=args.verbose
    )
    
    tester.run()


if __name__ == '__main__':
    main()
