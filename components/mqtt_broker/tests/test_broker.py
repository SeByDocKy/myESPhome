# Host test of the broker core. Build the helper first (from this directory):
#   g++ -std=c++20 -g -fsanitize=address,undefined -I.. broker_host.cpp ../mqtt_broker.cpp -o broker_host
# then run: python3 test_broker.py   (needs paho-mqtt)
import subprocess, time, threading, socket, struct, sys, os
import paho.mqtt.client as mqtt

BIN = os.path.join(os.path.dirname(__file__), "broker_host")
PORT = 18830
results = []

def check(name, cond, extra=""):
    results.append((name, bool(cond)))
    print(("PASS " if cond else "FAIL ") + name + (" -> " + str(extra) if (extra and not cond) else ""))

def start_broker(user="", pw="", port=PORT):
    args = [BIN, str(port)] + ([user, pw] if user else [])
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
    lines = []
    def reader():
        for ln in p.stdout:
            lines.append(ln.rstrip())
    t = threading.Thread(target=reader, daemon=True); t.start()
    for _ in range(100):
        if any(l.startswith("READY") for l in lines): break
        time.sleep(0.05)
    return p, lines

def new_client(cid, proto=mqtt.MQTTv311, user=None, pw=None, keepalive=30, port=PORT, will=None):
    c = mqtt.Client(client_id=cid, protocol=proto)
    if user: c.username_pw_set(user, pw)
    c.received = []
    c.connect_rc = None
    def on_connect(cl, ud, flags, rc, props=None): cl.connect_rc = rc
    def on_message(cl, ud, msg): cl.received.append((msg.topic, bytes(msg.payload), msg.qos))
    c.on_connect = on_connect; c.on_message = on_message
    if will: c.will_set(*will)
    c.connect("127.0.0.1", port, keepalive)
    c.loop_start()
    for _ in range(100):
        if c.connect_rc is not None: break
        time.sleep(0.05)
    return c

def wait_for(pred, timeout=3.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if pred(): return True
        time.sleep(0.05)
    return pred()

# ---------------- 1. basic pub/sub, QoS 0/1/2, v3.1.1 ----------------
p, lines = start_broker()
try:
    sub = new_client("sub1")
    check("connect v3.1.1 accepted", sub.connect_rc == 0, sub.connect_rc)
    sub.subscribe([("hb/device/+/action", 1)])
    time.sleep(0.3)
    pub = new_client("pub1")
    for q in (0, 1, 2):
        info = pub.publish("hb/device/SN123/action", b'{"type":25}%d' % q, qos=q)
        info.wait_for_publish(timeout=3)
        check("publish qos%d acknowledged" % q, info.is_published())
    check("subscriber got 3 messages", wait_for(lambda: len(sub.received) == 3), sub.received)
    check("payload intact", sub.received and sub.received[0][1] == b'{"type":25}0')

    # wildcard rules
    sub.subscribe([("hb/#", 0), ("#", 0)])
    time.sleep(0.2)
    sub.received.clear()
    pub.publish("hb/device/SN123/status", b"x").wait_for_publish()
    pub.publish("$SYS/test", b"y").wait_for_publish()
    time.sleep(0.4)
    topics = [t for t, _, _ in sub.received]
    check("'#' matches hb/device/SN123/status", "hb/device/SN123/status" in topics, topics)
    check("'#' does not match $SYS topics", "$SYS/test" not in topics, topics)

    # broker-originated publish (tick) reaches '#' subscriber
    check("broker originated publish delivered", wait_for(lambda: any(t == "hb/broker/tick" for t, _, _ in sub.received), 3.0))

    # big payload (7 KB, fragmented over several TCP reads)
    big = b"A" * 7000
    sub.received.clear()
    pub.publish("hb/device/SN123/status", big).wait_for_publish()
    check("7 KB payload delivered intact", wait_for(lambda: any(len(pl) == 7000 for _, pl, _ in sub.received)), [len(x[1]) for x in sub.received])

    # oversized publish discarded, connection survives
    sub.received.clear()
    pub.publish("hb/device/SN123/status", b"B" * 20000).wait_for_publish()
    time.sleep(0.5)
    check("20 KB payload discarded", not any(len(pl) == 20000 for _, pl, _ in sub.received))
    pub.publish("hb/device/SN123/status", b"after").wait_for_publish()
    check("connection survives oversized publish", wait_for(lambda: any(pl == b"after" for _, pl, _ in sub.received)), sub.received[-3:])

    # unsubscribe
    sub.unsubscribe("#")
    sub.unsubscribe("hb/#")
    time.sleep(0.3)
    sub.received.clear()
    pub.publish("hb/device/SN123/status", b"z").wait_for_publish()
    time.sleep(0.4)
    check("no delivery after UNSUBSCRIBE of # and hb/#", not any(t == "hb/device/SN123/status" for t, _, _ in sub.received), sub.received)

    # will message on abrupt disconnect
    sub.subscribe([("will/topic", 0)])
    time.sleep(0.2)
    w = new_client("willer", will=("will/topic", b"gone", 0, False))
    time.sleep(0.2)
    w._sock.close()  # abrupt
    check("will message published on connection loss", wait_for(lambda: any(t == "will/topic" and pl == b"gone" for t, pl, _ in sub.received), 3.0), sub.received)
    w.loop_stop()

    # clean disconnect: no will
    sub.received.clear()
    w2 = new_client("willer2", will=("will/topic", b"gone2", 0, False))
    time.sleep(0.2)
    w2.disconnect(); w2.loop_stop()
    time.sleep(0.6)
    check("no will message after clean DISCONNECT", not any(pl == b"gone2" for _, pl, _ in sub.received), sub.received)

    # takeover with same client id
    a = new_client("dup"); time.sleep(0.2)
    b = new_client("dup"); time.sleep(0.4)
    check("second client with same id accepted", b.connect_rc == 0)
    a.loop_stop(); b.loop_stop(); b.disconnect()

    # ---------------- v5 ----------------
    v5s = mqtt.Client(client_id="v5sub", protocol=mqtt.MQTTv5)
    v5s.received = []; v5s.rc = None
    v5s.on_connect = lambda c, u, f, rc, pr=None: setattr(c, "rc", rc)
    v5s.on_message = lambda c, u, m: c.received.append((m.topic, bytes(m.payload)))
    v5s.connect("127.0.0.1", PORT); v5s.loop_start()
    wait_for(lambda: v5s.rc is not None)
    check("connect v5 accepted", v5s.rc == 0 or str(v5s.rc) == "Success", v5s.rc)
    v5s.subscribe("v5/#", qos=1)
    time.sleep(0.3)
    pub.publish("v5/x", b"hello5").wait_for_publish()
    check("v5 subscriber receives v3.1.1 publish", wait_for(lambda: ("v5/x", b"hello5") in v5s.received), v5s.received)
    v5p = mqtt.Client(client_id="v5pub", protocol=mqtt.MQTTv5)
    v5p.connect("127.0.0.1", PORT); v5p.loop_start(); time.sleep(0.3)
    sub.subscribe([("v5pub/#", 0)]); time.sleep(0.2)
    v5p.publish("v5pub/x", b"from5", qos=1).wait_for_publish()
    check("v5 publisher QoS1 acknowledged + routed to v3 subscriber", wait_for(lambda: any(pl == b"from5" for _, pl, _ in sub.received)), sub.received[-3:])
    v5s.loop_stop(); v5p.loop_stop(); pub.loop_stop(); sub.loop_stop()
finally:
    p.terminate(); p.wait(timeout=5)
    sanitizer = [l for l in lines if "ERROR: AddressSanitizer" in l or "runtime error" in l]
    check("no sanitizer errors in broker run", not sanitizer, sanitizer[:3])

# ---------------- 2. authentication ----------------
p, lines = start_broker("jackery", "s3cret", port=PORT + 1)
try:
    bad = new_client("bad", user="jackery", pw="wrong", port=PORT + 1)
    check("wrong password refused", bad.connect_rc not in (0, None), bad.connect_rc)
    bad.loop_stop()
    nouser = new_client("nouser", port=PORT + 1)
    check("missing credentials refused", nouser.connect_rc not in (0, None), nouser.connect_rc)
    nouser.loop_stop()
    ok = new_client("good", user="jackery", pw="s3cret", port=PORT + 1)
    check("correct credentials accepted", ok.connect_rc == 0, ok.connect_rc)
    ok.loop_stop()
finally:
    p.terminate(); p.wait(timeout=5)
    sanitizer = [l for l in lines if "ERROR: AddressSanitizer" in l or "runtime error" in l]
    check("no sanitizer errors in auth run", not sanitizer, sanitizer[:3])

# ---------------- 3. keep-alive expiry and raw malformed input ----------------
p, lines = start_broker(port=PORT + 2)
try:
    # raw socket: CONNECT with keepalive 2 s, then silence -> dropped after ~3 s (1.5 x)
    s = socket.create_connection(("127.0.0.1", PORT + 2))
    pkt_body = b"\x00\x04MQTT\x04\x02\x00\x02\x00\x02ka"
    s.sendall(bytes([0x10, len(pkt_body)]) + pkt_body)
    s.settimeout(1.0)
    connack = s.recv(4)
    check("raw CONNACK is 20 02 00 00", connack == b"\x20\x02\x00\x00", connack)
    t0 = time.time()
    s.settimeout(6.0)
    try:
        data = s.recv(1)
        dropped = (data == b"")
    except Exception as e:
        dropped = False
    el = time.time() - t0
    check("keep-alive expiry closes idle client (~3 s)", dropped and 2.0 <= el <= 5.0, "%.1fs" % el)
    s.close()

    # no CONNECT at all -> dropped after connect timeout (10 s) -- skip waiting, test garbage first byte instead
    s2 = socket.create_connection(("127.0.0.1", PORT + 2))
    s2.sendall(b"\x30\x05\x00\x01a\x00\x00")  # PUBLISH before CONNECT
    s2.settimeout(2.0)
    try:
        d = s2.recv(1)
        check("PUBLISH before CONNECT closes connection", d == b"")
    except Exception as e:
        check("PUBLISH before CONNECT closes connection", False, e)
    s2.close()

    # malformed: 5-byte remaining length
    s3 = socket.create_connection(("127.0.0.1", PORT + 2))
    s3.sendall(b"\x10\xff\xff\xff\xff\x7f")
    s3.settimeout(2.0)
    try:
        d = s3.recv(1)
        check("malformed remaining length closes connection", d == b"")
    except Exception as e:
        check("malformed remaining length closes connection", False, e)
    s3.close()

    # byte-by-byte delivery of a valid CONNECT + SUBSCRIBE + PUBLISH (fragmentation)
    s4 = socket.create_connection(("127.0.0.1", PORT + 2)); s4.settimeout(2.0)
    body = b"\x00\x04MQTT\x04\x02\x00\x3c\x00\x04frag"
    stream = bytes([0x10, len(body)]) + body
    sub_body = b"\x00\x01" + b"\x00\x05a/b/c" + b"\x00"
    stream += bytes([0x82, len(sub_body)]) + sub_body
    pub_body = b"\x00\x05a/b/c" + b"hello"
    stream += bytes([0x30, len(pub_body)]) + pub_body
    for i in range(len(stream)):
        s4.sendall(stream[i:i+1]); time.sleep(0.005)
    time.sleep(0.5)
    got = b""
    try:
        while True:
            chunk = s4.recv(100)
            if not chunk: break
            got += chunk
            if len(got) >= 4 + 5 + 14: break
    except Exception:
        pass
    check("byte-by-byte CONNECT/SUBSCRIBE/PUBLISH produces CONNACK+SUBACK+echo", got.startswith(b"\x20\x02\x00\x00\x90\x03\x00\x01\x00") and b"a/b/chello" in got, got)
    s4.close()
finally:
    p.terminate(); p.wait(timeout=5)
    sanitizer = [l for l in lines if "ERROR: AddressSanitizer" in l or "runtime error" in l]
    check("no sanitizer errors in raw run", not sanitizer, sanitizer[:3])

failed = [n for n, ok in results if not ok]
print("\n%d checks, %d failed" % (len(results), len(failed)))
sys.exit(1 if failed else 0)
