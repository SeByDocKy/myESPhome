#!/usr/bin/env python3
import sys
import requests
from pathlib import Path

if len(sys.argv) != 2:
    sys.exit(f"Usage: {sys.argv[0]} <url>")

url = sys.argv[1]
filename = Path(url).name

headers = {
    "Authorization": "Basic QyFWcEdUeTFvcDVZOnVzN1BAd3lwQyQkbw==",
    "User-Agent": "okhttp/4.9.0",
}

r = requests.get(url, headers=headers)
r.raise_for_status()

Path(filename).write_bytes(r.content)
print(f"Downloaded: {filename}")