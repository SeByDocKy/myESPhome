#!/bin/sh
# Builds and runs the host tests, then cross-checks the frames against the independent Python implementation.
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -Wall -Wextra -Werror -I../../components/hms_fwupdate test_core.cpp -o /tmp/hms_fwupdate_test_core
/tmp/hms_fwupdate_test_core
python3 make_sample_hex.py /tmp/hms_fwupdate_sample.hex
INV=114300001234
DTU=199980171976
/tmp/hms_fwupdate_test_core dump /tmp/hms_fwupdate_sample.hex $INV $DTU > /tmp/frames_cpp.txt
python3 ref_frames.py /tmp/hms_fwupdate_sample.hex $INV $DTU > /tmp/frames_py.txt
diff /tmp/frames_cpp.txt /tmp/frames_py.txt && echo "frames identical to the independent reference ($(wc -l < /tmp/frames_cpp.txt) packets)"
