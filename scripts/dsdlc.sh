#!/bin/sh
## generate code using dronecan_dsdlc.py.
## supports arbitrary amount of trailing arguments passed to dronecan_dsdlc.py.
## supports any arguments || by default generates contents of uavcan subdir.

set -ex

DSDLC="3rdparty/dronecan/dronecan_dsdlc/dronecan_dsdlc.py"
DSDL="3rdparty/dronecan/dsdl"

## clean build every time
[ -d dsdlc_generated ] && rm -rf dsdlc_generated

echo "Generating code using dronecan_dsdlc.py ..."
python3 "$DSDLC" --output dsdlc_generated ${*:-$DSDL/uavcan}
