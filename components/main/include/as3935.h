/**
 * @file as3935.h
 * @brief Public API for AS3935 Lightning Detector
 * 
 * This header now re-exports the adapter layer which wraps the esp_as3935 library.
 * The API remains the same for backward compatibility.
 */

#pragma once

// Include the adapter header which provides the full API
#include "as3935_adapter.h"
