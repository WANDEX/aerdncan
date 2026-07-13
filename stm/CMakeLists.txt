# cmake_minimum_required(VERSION 3.22)

## This file is generated only once,
## and is not re-generated if converter is called multiple times.
##
## User is free to modify the file as much as necessary

set(CANARD_BASE    "${CMAKE_SOURCE_DIR}/3rdparty/dronecan/libcanard")
set(DSDLC_GEN_BASE "${CMAKE_SOURCE_DIR}/dsdlc_generated")

add_library(canard STATIC)
target_include_directories(canard PUBLIC
  ${CANARD_BASE}
  # ${CANARD_BASE}/drivers/linux
  ${CANARD_BASE}/drivers/stm32
  ${DSDLC_GEN_BASE}/include
)
target_sources(canard PRIVATE
  ${CANARD_BASE}/canard.c

  ## add socketcan and multicast drivers for linux
  # ${CANARD_BASE}/drivers/socketcan/socketcan.c
  # ${CANARD_BASE}/drivers/mcast/mcast.c
  # ${CANARD_BASE}/drivers/linux/linux.c

  ## add drivers for stm32
  ${CANARD_BASE}/drivers/stm32/canard_stm32.c

  ## add in generated code
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.NodeStatus.c
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.GetNodeInfo_res.c
  ${DSDLC_GEN_BASE}/src/uavcan.equipment.power.BatteryInfo.c
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.dynamic_node_id.Allocation.c
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.param.GetSet_req.c
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.param.GetSet_res.c
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.param.ExecuteOpcode_req.c
  ${DSDLC_GEN_BASE}/src/uavcan.protocol.param.ExecuteOpcode_res.c
)
target_compile_definitions(canard PUBLIC
  _GNU_SOURCE # || _BSD_SOURCE is needed for the canard_stm32.c usleep()
  ##
  ## default frame size is not enough to fit battery info!
  ##
  ## CANARD_CAN_FRAME_MAX_DATA_LEN               8U
  ## UAVCAN_EQUIPMENT_POWER_BATTERYINFO_MAX_SIZE 55
  ## CANARD_CANFD_FRAME_MAX_DATA_LEN             64U
  CANARD_ENABLE_CANFD
)

## Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

## Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

## Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ## Add user defined library search paths
)

## Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ## Add user sources here
)

## Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ## Add user defined include paths
)

## Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  ## Add user defined symbols
)

## Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

## Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
  stm32cubemx
  ## Add user defined libraries
  canard
)
