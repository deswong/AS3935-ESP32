"""Utilities for validating AS3935 register JSON and MQTT URIs.

These helpers are host-side validators for unit tests and examples. They do not
interact with device hardware.
"""
import re
import json
import sys
from urllib.parse import urlparse

HEX_RE = re.compile(r"^0x[0-9a-fA-F]+$")


def parse_register_key(key: str) -> int:
    """Parse a register key that can be hex (0x..) or decimal string.

    Raises ValueError for invalid formats.
    """
    if isinstance(key, int):
        return key
    if not isinstance(key, str):
        raise ValueError("register keys must be string or int")
    key = key.strip()
    if HEX_RE.match(key):
        return int(key, 16)
    if key.isdigit():
        return int(key, 10)
    raise ValueError(f"invalid register key: {key!r}")


def validate_register_map(obj) -> dict:
    """Validate a JSON object mapping register addresses to 0-255 integer values.

    Returns a normalized dict {int_address: int_value}.
    Raises ValueError on any invalid entry.
    """
    if not isinstance(obj, dict):
        raise ValueError("register map must be a JSON object/dict")
    out = {}
    for k, v in obj.items():
        addr = parse_register_key(k)
        if not (0 <= addr <= 0xFF):
            raise ValueError(f"register address out of range: {addr}")
        if not isinstance(v, int):
            # allow numeric strings
            if isinstance(v, str) and v.isdigit():
                v = int(v)
            else:
                raise ValueError(f"register value must be integer 0-255 for key {k}")
        if not (0 <= v <= 0xFF):
            raise ValueError(f"register value out of range 0-255 for key {k}")
        out[addr] = v
    return out


def validate_mqtt_uri(uri: str) -> dict:
    """Validate an MQTT URI like mqtt://host:1883 or mqtts://host:8883.

    Returns parsed components dict with scheme, host, port, and secure boolean.
    Raises ValueError if invalid.
    """
    if not isinstance(uri, str) or not uri:
        raise ValueError("uri must be a non-empty string")
    parsed = urlparse(uri)
    scheme = parsed.scheme.lower()
    if scheme not in ("mqtt", "mqtts", "ws", "wss"):
        # accept ws/wss for websocket transports
        raise ValueError(f"unsupported scheme: {scheme}")
    host = parsed.hostname
    if not host:
        raise ValueError("missing host in URI")
    port = parsed.port
    if port is None:
        # use defaults
        port = 8883 if scheme in ("mqtts", "wss") else 1883
    secure = scheme in ("mqtts", "wss")
    return {"scheme": scheme, "host": host, "port": port, "secure": secure}


def load_json_from_string(s: str):
    try:
        return json.loads(s)
    except json.JSONDecodeError as e:
        raise ValueError(f"invalid JSON: {e}")


if __name__ == "__main__":
    # tiny CLI for quick checks
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("file", help="path to JSON register file or '-' for stdin")
    args = p.parse_args()
    data = None
    if args.file == "-":
        data = load_json_from_string(sys.stdin.read())
    else:
        with open(args.file, "r") as f:
            data = load_json_from_string(f.read())
    print(validate_register_map(data))
