// Redirect local includes of "mqtt_client.h" to the IDF esp-mqtt header so
// that translation units which include this name receive the esp-mqtt types.
//
// This uses an absolute path to the IDF component include file to avoid
// recursion with the local include directory which appears earlier on the
// compiler include search path.

#pragma once
#include "/home/des/esp/esp-idf/components/mqtt/esp-mqtt/include/mqtt_client.h"
