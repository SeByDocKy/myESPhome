#!/usr/bin/env python3
"""Independent re-implementation of the frame layout, to cross-check fwupdate_core.h.

Written from the reference code (OpenDTU branch FirmwareUpdate_HM_IV: FirmwareCommand.cpp,
FirmwareDataCommand.cpp, HM_Abstract::enqueueFirmwareRow) and NOT from fwupdate_core.h.
usage: ref_frames.py <file.hex> <inverter_sn_hex> <dtu_sn_hex>
"""
import sys


def crc16(data, crc=0xFFFF):
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def ids(serial):
    return list((serial & 0xFFFFFFFF).to_bytes(4, "big"))


def main(path, inv_hex, dtu_hex):
    inv, dtu = ids(int(inv_hex, 16)), ids(int(dtu_hex, 16))
    rown = 0
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        raw = bytes.fromhex(line[1:])
        row = raw[:-1]  # drop the Intel HEX checksum
        rown += 1
        chunks = [row[i : i + 16] for i in range(0, len(row), 16)]
        pkts = []  # (number, payload, with_crc)
        no = 1
        for i, c in enumerate(chunks):
            last = i == len(chunks) - 1
            fits = last and len(c) <= 14
            pkts.append((0x80 + no if fits else no, c, fits))
            no += 1
            if last and not fits:
                pkts.append((0x80 + no, b"", True))
                no += 1
        for i, (num, payload, with_crc) in enumerate(pkts):
            f = bytes([0x0E] + inv + dtu + [num]) + payload
            if with_crc:
                f += crc16(row).to_bytes(2, "big")
            x = 0
            for b in f:
                x ^= b
            f += bytes([x])
            print(f"row {rown} pkt {i + 1}/{len(pkts)} ack={1 if num & 0x80 else 0}: " + " ".join(f"{b:02X}" for b in f))
        if row[3] == 0x01:
            break


if __name__ == "__main__":
    main(*sys.argv[1:4])
