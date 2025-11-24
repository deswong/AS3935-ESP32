"""
Tiny Server-Sent Events (SSE) subscriber using requests and sseclient.

Usage:
    pip install -r requirements.txt
    python scripts/sse_sub_example.py --url http://192.168.4.1/api/events/stream

It prints every SSE event and attempts to parse JSON payloads.
"""
import argparse
import json
import requests
from sseclient import SSEClient


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--url', required=True)
    args = parser.parse_args()

    print(f"Connecting to SSE {args.url}...")
    client = SSEClient(args.url)
    for event in client:
        print(f"event: {event.event} data: {event.data}")
        try:
            j = json.loads(event.data)
            print(' -> parsed:', j)
        except Exception:
            pass

if __name__ == '__main__':
    main()
