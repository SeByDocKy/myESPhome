#!/usr/bin/env python3
"""
APsystems EZ1 BLE OTA Firmware Updater

Usage: python3 ota.py firmware.bin [-f] [-v]

Protocol: BLE GATT → BLUFI custom data frames → AES-128-CBC encrypted JSON
Firmware chunks: raw binary via BLUFI, 480 bytes, CRC16-MODBUS

Requirements: pip install bleak cryptography
"""

import asyncio, json, os, struct, sys, time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("pip install bleak cryptography"); sys.exit(1)

# BLE UUIDs
SERVICE_UUID = "0000fffe-0000-1000-8000-00805f9b34fb"
WRITE_UUID   = "0000ff0a-0000-1000-8000-00805f9b34fb"
NOTIFY_UUID  = "0000ff0b-0000-1000-8000-00805f9b34fb"

# AES-128-CBC keys (from APK)
AES_KEY = b"E7MiPPrs9v6i3DY3"
AES_IV  = b"8914934610490056"

# BLUFI frame constants
TYPE_CUSTOM = 0x4D
FC_FRAG = 0x10

# OTA constants
PACK_SIZE = 480
CODE_UP_TO_DATE = 1004
CODE_READY = 1010
CODE_NEXT = 1011
CODE_RETRY = 1012
CODE_COMPLETE = 1013
CODE_FAILED = 1014


# ── Crypto ────────────────────────────────────────────────────────────────────

def aes_encrypt(text: str) -> bytes:
    """AES-CBC encrypt JSON string → hex-encoded ASCII (app→device format)."""
    data = text.encode()
    data += b"\x00" * ((16 - len(data) % 16) % 16)
    ct = Cipher(algorithms.AES(AES_KEY), modes.CBC(AES_IV)).encryptor().update(data)
    return ct.hex().encode("ascii")

def aes_decrypt(raw: bytes) -> str:
    """Decrypt raw AES-CBC ciphertext → JSON string (device→app format)."""
    if len(raw) % 16: raw += b"\x00" * (16 - len(raw) % 16)
    pt = Cipher(algorithms.AES(AES_KEY), modes.CBC(AES_IV)).decryptor().update(raw)
    return pt.rstrip(b"\x00").decode("utf-8", errors="replace")


# ── CRC16-MODBUS ──────────────────────────────────────────────────────────────

def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return ((crc & 0xFF) << 8) | ((crc >> 8) & 0xFF)

def build_chunk(module: int, pack_num: int, firmware: bytes) -> bytes:
    offset = pack_num * PACK_SIZE
    chunk = firmware[offset:offset + PACK_SIZE]
    header = struct.pack("<BBII", 2, module, pack_num, len(chunk))
    return header + chunk + struct.pack("<I", crc16_modbus(chunk))


# ── BLUFI Transport ───────────────────────────────────────────────────────────

class Blufi:
    def __init__(self, client, verbose=False):
        self.client = client
        self.seq = 0
        self.q = asyncio.Queue()
        self.verbose = verbose

    async def start(self):
        await self.client.start_notify(NOTIFY_UUID, lambda _, d: self.q.put_nowait(d))

    async def send(self, content: bytes):
        """Send data as BLUFI custom data frames, fragmenting at 42 bytes."""
        if len(content) <= 42:
            frame = bytes([TYPE_CUSTOM, 0x00, self.seq & 0xFF, len(content)]) + content
            self.seq += 1
            if self.verbose: print(f"  -> {len(frame)}B: {frame[:40].hex()}...")
            await self.client.write_gatt_char(WRITE_UUID, frame, response=True)
            return

        off = 0
        while off < len(content):
            remaining = len(content) - off
            if remaining <= 42:
                chunk = content[off:]
                fc = 0x00
                payload = chunk
                off = len(content)
            else:
                chunk = content[off:off + 40]
                fc = FC_FRAG
                payload = struct.pack("<H", remaining) + chunk
                off += 40
            frame = bytes([TYPE_CUSTOM, fc, self.seq & 0xFF, len(payload)]) + payload
            self.seq += 1
            if self.verbose: print(f"  -> {len(frame)}B: {frame[:40].hex()}...")
            await self.client.write_gatt_char(WRITE_UUID, frame, response=True)

    async def recv(self, timeout=10.0) -> bytes | None:
        """Receive and reassemble fragmented BLUFI response."""
        buf = b""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                raw = await asyncio.wait_for(self.q.get(), deadline - time.monotonic())
            except (asyncio.TimeoutError, ValueError):
                break
            if len(raw) < 4: continue
            fc, payload = raw[1], raw[4:4 + raw[3]]
            if fc & FC_FRAG:
                buf += payload[2:] if len(payload) >= 2 else payload
            else:
                return buf + payload
        return buf or None

    def drain(self):
        while not self.q.empty():
            try: self.q.get_nowait()
            except asyncio.QueueEmpty: break


# ── OTA Updater ───────────────────────────────────────────────────────────────

class OTA:
    def __init__(self, verbose=False):
        self.client = None
        self.blufi = None
        self.device_name = ""
        self.device_id = ""
        self.verbose = verbose

    async def connect(self):
        print("Scanning for APsystems devices...")
        devices = await BleakScanner.discover(timeout=10, return_adv=True)
        target = None
        for d, adv in devices.values():
            name = d.name or adv.local_name or ""
            if "EZ1_" in name:
                print(f"  Found: {name} ({d.address}) RSSI={adv.rssi}")
                target, self.device_name = d, name

        if not target:
            raise RuntimeError("No APsystems device found")

        self.device_id = self.device_name.split("_", 1)[1] if "_" in self.device_name else self.device_name
        print(f"Connecting to {self.device_name}...")
        self.client = BleakClient(target.address)
        await self.client.connect()
        self.blufi = Blufi(self.client, self.verbose)
        await self.blufi.start()

    async def send_cmd(self, identifier, method="get", cmd_type="property",
                       cmd_id="1", data=None) -> dict | None:
        cmd = {"id": cmd_id, "deviceId": self.device_id, "type": cmd_type,
               "method": method, "identifier": identifier,
               "company": "apsystems", "companyKey": "AmS4SV9oy3gk",
               "version": "1.0", "productKey": "EZ1"}
        if data is not None:
            cmd["data"] = data
        else:
            cmd["params"] = {}

        self.blufi.drain()
        await self.blufi.send(aes_encrypt(json.dumps(cmd, separators=(",", ":"))))
        resp = await self.blufi.recv()
        if not resp: return None
        try:
            return json.loads(aes_decrypt(resp))
        except Exception:
            return None

    async def wait_resp(self, timeout=30) -> dict | None:
        resp = await self.blufi.recv(timeout=timeout)
        if not resp: return None
        try:
            return json.loads(aes_decrypt(resp))
        except Exception:
            return None

    @staticmethod
    def get_code(resp) -> int | None:
        if not isinstance(resp, dict): return None
        code = resp.get("code")
        if code is not None: return int(code)
        d = resp.get("data")
        if isinstance(d, dict) and d.get("code") is not None:
            return int(d["code"])
        return None

    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()


# ── Main ──────────────────────────────────────────────────────────────────────

async def main():
    if len(sys.argv) < 2:
        print("Usage: python3 ota.py firmware.bin [-f] [-v]")
        print("  -f  force (skip version check)")
        print("  -v  verbose BLE debug"); return

    fw_path = sys.argv[1]
    verbose = "-v" in sys.argv
    force = "-f" in sys.argv
    firmware = open(fw_path, "rb").read()
    total = (len(firmware) + PACK_SIZE - 1) // PACK_SIZE

    # Extract version from ESP32 image
    ver = ""
    pos = firmware.find(b'\x32\x54\xCD\xAB')
    if pos >= 0:
        ver = firmware[pos+16:pos+48].split(b'\x00')[0].decode('ascii', errors='replace')
    print(f"File: {os.path.basename(fw_path)} ({len(firmware):,}B) version={ver or '?'}")

    ota = OTA(verbose=verbose)
    await ota.connect()

    try:
        info = await ota.send_cmd("deviceInfo")
        if info and "data" in info:
            d = info["data"]
            print(f"Device: {d.get('ty','?')} FW={d.get('devVer','?')} DCM={d.get('dcmver','?')}")

        ota_ver = ver + "_f" if force else ver
        resp = await ota.send_cmd("bleUpdate", method="upgrade_post", cmd_type="ota", cmd_id="6",
            data={"version": ota_ver, "module": "0", "packSize": str(PACK_SIZE), "fileSize": str(len(firmware))})
        if resp is None:
            resp = await ota.wait_resp(timeout=15)
        code = ota.get_code(resp)
        if code == CODE_UP_TO_DATE:
            print("Already up to date. Use -f to force."); return
        if code != CODE_READY:
            print(f"Device response: {code} (expected {CODE_READY})"); return

        t0, retries = time.monotonic(), 0
        for i in range(total):
            await ota.blufi.send(build_chunk(0, i, firmware))
            resp = await ota.wait_resp(timeout=10)
            code = ota.get_code(resp)

            if code == CODE_NEXT:
                retries = 0
                pct = (i + 1) * 100 // total
                sys.stdout.write(f"\r  [{pct:3d}%] {i+1}/{total}"); sys.stdout.flush()
            elif code == CODE_COMPLETE:
                print(f"\r  [100%] Done in {time.monotonic()-t0:.0f}s — rebooting"); return
            elif code == CODE_FAILED:
                print(f"\n  FAILED at chunk {i}"); return
            else:
                retries += 1
                if retries > 3: print(f"\n  Too many retries"); return

        print(f"\r  [100%] Done in {time.monotonic()-t0:.0f}s — rebooting")
    finally:
        await ota.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
