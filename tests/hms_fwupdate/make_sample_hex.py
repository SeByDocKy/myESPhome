#!/usr/bin/env python3
"""Writes a synthetic Intel HEX file with the row sizes that exercise every packetization case.
The first row carries an identity that matches an HMS-2T 'B' inverter (serial prefix 1143)."""
import random
import sys

random.seed(7)


def rec(addr, rtype, data):
    body = bytes([len(data), addr >> 8, addr & 0xFF, rtype]) + bytes(data)
    return ":" + (body + bytes([(-sum(body)) & 0xFF])).hex().upper()


lines = []
ident = bytes([0xF0, 0x11, 0x10, 0x01]) + bytes(random.randrange(256) for _ in range(12))
lines.append(rec(0xF800, 0x00, ident))
addr = 0x4000
for size in (16, 16, 15, 14, 13, 1, 16, 32, 4, 2):
    lines.append(rec(addr, 0x00, bytes(random.randrange(256) for _ in range(size))))
    addr += size
lines.append(rec(0x0000, 0x01, b""))
open(sys.argv[1], "w").write("\n".join(lines) + "\n")
