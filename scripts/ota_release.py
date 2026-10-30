#!/usr/bin/env python3
"""Build, stage, checksum, and optionally publish a TerraSmart OTA image."""

import argparse
import json
import shutil
import subprocess
import sys
import zlib
from pathlib import Path
from urllib.parse import quote


PROJECT_DIR = Path(__file__).resolve().parent.parent
CONFIG_PATH = PROJECT_DIR / "ota_config.json"

# Maps PlatformIO build environments to the OTA target IDs and image names.
TARGETS = {
    "d1_mini_node": (17, "d1_mini_node.bin"),
    "d1_mini_float_switch_node": (18, "d1_mini_float_switch_node.bin"),
    "d1_mini_relay_node": (20, "d1_mini_relay_node.bin"),
    "esp32dev_gateway": (1, "esp32dev_gateway.bin"),
    "esp32dev_mqtt_gateway": (0, "esp32dev_mqtt_gateway.bin"),
    "d1_mini_mqtt_gateway": (0, "d1_mini_mqtt_gateway.bin"),
}
FORWARDED_IMAGE_LIMITS = {
    1: 0x1E0000,  # ESP-NOW gateway OTA slot from partitions_ota.csv
}
DEFAULT_FORWARDED_IMAGE_LIMIT = 700000


def load_config():
    if not CONFIG_PATH.is_file():
        return {}
    with CONFIG_PATH.open(encoding="utf-8") as config_file:
        config = json.load(config_file)
    if not isinstance(config, dict):
        raise RuntimeError("ota_config.json must contain a JSON object")
    return config


def find_tool(*names):
    for name in names:
        executable = shutil.which(name)
        if executable:
            return executable
    raise RuntimeError("Required command not found: " + " or ".join(names))


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Build a firmware, copy it to Home Assistant www, and prepare its OTA command."
        )
    )
    parser.add_argument("--env", required=True, choices=sorted(TARGETS))
    parser.add_argument(
        "--gateway-id",
        help="stable gateway_id that must execute this OTA command (gw-<MAC>)",
    )
    parser.add_argument(
        "--www-dir",
        help="local path to Home Assistant's www directory (overrides ota_config.json)",
    )
    parser.add_argument(
        "--base-url",
        help="base URL reachable by the ESP32, e.g. http://10.0.0.216:8123",
    )
    parser.add_argument(
        "--payload-file",
        help="also save the generated JSON payload to this file",
    )
    parser.add_argument(
        "--publish", action="store_true", help="publish the command to MQTT after building"
    )
    args = parser.parse_args()

    try:
        config = load_config()
        www_dir_value = args.www_dir or config.get("home_assistant_www_dir")
        base_url = (args.base_url or config.get("home_assistant_base_url", "")).rstrip("/")
        if not www_dir_value or not base_url:
            raise RuntimeError(
                "Both --www-dir and --base-url are required when not configured in ota_config.json."
            )

        pio = find_tool("pio", "platformio")
        target, filename = TARGETS[args.env]
        print(f"Building PlatformIO environment: {args.env}", flush=True)
        subprocess.run([pio, "run", "-e", args.env], cwd=PROJECT_DIR, check=True)

        image = PROJECT_DIR / ".pio" / "build" / args.env / "firmware.bin"
        if not image.is_file():
            raise RuntimeError(f"Build did not produce {image}")

        www_dir = Path(www_dir_value).expanduser()
        if not www_dir.is_dir():
            raise RuntimeError(
                f"Home Assistant www directory does not exist or is not mounted: {www_dir}\n"
                "Mount the Home Assistant config share and set its www path in ota_config.json."
            )
        staged_image = www_dir / filename
        # GVFS/Samba mounts may not support copying ownership/timestamps, which
        # shutil.copy2 attempts. The firmware only needs its bytes on the share.
        shutil.copyfile(image, staged_image)

        data = image.read_bytes()
        size = len(data)
        crc32 = zlib.crc32(data) & 0xFFFFFFFF
        url = f"{base_url}/local/{quote(filename)}"
        command = {
            "target": target,
            "url": url,
            "size": size,
            "crc32": crc32,
        }
        gateway_id = args.gateway_id or config.get("ota_gateway_id")
        if not gateway_id:
            raise RuntimeError(
                "Set ota_gateway_id to the target pair's gateway_id before "
                "preparing an OTA command."
            )
        command["gateway_id"] = gateway_id
        payload = json.dumps(command, separators=(",", ":"))

        payload_file = args.payload_file
        if payload_file:
            payload_path = Path(payload_file).expanduser()
            payload_path.parent.mkdir(parents=True, exist_ok=True)
            payload_path.write_text(payload + "\n", encoding="utf-8")
            print(f"Payload saved to: {payload_path}")

        print(f"\nFirmware copied to: {staged_image}")
        print(f"OTA payload for topic {config.get('mqtt_topic', 'terrasmart/ota')}:")
        print(payload)

        forwarded_limit = FORWARDED_IMAGE_LIMITS.get(
            target, DEFAULT_FORWARDED_IMAGE_LIMIT
        )
        if target != 0 and size > forwarded_limit:
            print(
                f"WARNING: this image exceeds the current {forwarded_limit}-byte limit for "
                "OTA forwarded through the ESP-NOW gateway; do not publish this target yet.",
                file=sys.stderr,
            )
            if args.publish:
                raise RuntimeError("Refusing to publish an image above the forwarding limit")

        if args.publish:
            broker = config.get("mqtt_broker")
            if not broker:
                raise RuntimeError("Set mqtt_broker in ota_config.json before publishing")
            mosquitto_pub = find_tool("mosquitto_pub")
            publish_args = [
                mosquitto_pub,
                "-h",
                broker,
                "-p",
                str(config.get("mqtt_port", 1883)),
                "-q",
                "1",
                "-t",
                config.get("mqtt_topic", "terrasmart/ota"),
                "-m",
                payload,
            ]
            if config.get("mqtt_username"):
                publish_args.extend(["-u", config["mqtt_username"]])
            if config.get("mqtt_password"):
                publish_args.extend(["-P", config["mqtt_password"]])
            subprocess.run(publish_args, check=True)
            print("MQTT publish accepted by mosquitto_pub (QoS 1, not retained).")
        else:
            print("Not published. Review the payload, then run the VS Code publish task if ready.")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"OTA release failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
