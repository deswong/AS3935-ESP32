"""
Simple MQTT subscriber for AS3935 lightning events.

Usage:
    pip install -r requirements.txt
    python scripts/mqtt_sub_example.py --broker 192.168.1.10 --port 1883 --topic as3935/lightning

It prints received messages and a small parse of the JSON payload.
"""
import argparse
import json
import time
from paho.mqtt import client as mqtt_client


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(userdata['topic'])
    else:
        print("Failed to connect, return code %d\n", rc)


def on_message(client, userdata, msg):
    payload = msg.payload.decode('utf-8', errors='ignore')
    print(f"[{msg.topic}] {payload}")
    try:
        j = json.loads(payload)
        print(" -> parsed:", j)
    except Exception:
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--broker', required=True)
    parser.add_argument('--port', type=int, default=1883)
    parser.add_argument('--topic', default='as3935/lightning')
    args = parser.parse_args()

    userdata = {'topic': args.topic}
    client = mqtt_client.Client(client_id=f"as3935_sub_{int(time.time())}")
    client.user_data_set(userdata)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(args.broker, args.port, 60)
    client.loop_forever()


if __name__ == '__main__':
    main()
