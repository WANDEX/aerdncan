
CANARD_BASE=../3rdparty/dronecan/libcanard

CC=gcc
CFLAGS = -g -Wall -I../dsdlc_generated/include -I$(CANARD_BASE) -I$(CANARD_BASE)/drivers/linux

LIBS=$(CANARD_BASE)/canard.c

## add socketcan and multicast drivers for linux
LIBS+=$(CANARD_BASE)/drivers/socketcan/socketcan.c
LIBS+=$(CANARD_BASE)/drivers/mcast/mcast.c
LIBS+=$(CANARD_BASE)/drivers/linux/linux.c

## add in generated code
LIBS+=../dsdlc_generated/src/uavcan.protocol.NodeStatus.c
LIBS+=../dsdlc_generated/src/uavcan.protocol.GetNodeInfo_res.c
LIBS+=../dsdlc_generated/src/uavcan.equipment.power.BatteryInfo.c
LIBS+=../dsdlc_generated/src/uavcan.protocol.dynamic_node_id.Allocation.c
LIBS+=../dsdlc_generated/src/uavcan.protocol.param.GetSet_req.c
LIBS+=../dsdlc_generated/src/uavcan.protocol.param.GetSet_res.c
LIBS+=../dsdlc_generated/src/uavcan.protocol.param.ExecuteOpcode_req.c
LIBS+=../dsdlc_generated/src/uavcan.protocol.param.ExecuteOpcode_res.c

all: battery_node

battery_node: battery_node.c $(LIBS)
	$(CC) -o battery_node battery_node.c $(LIBS) $(CFLAGS)

clean:
	rm -rf battery_node
