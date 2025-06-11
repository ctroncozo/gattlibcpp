/**
 * @file gattlib_utils.cpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 *
 * @brief Utility functions for gattlib
 */

#include "gattlib_utils.hpp"

// NOLINTBEGIN(*) Do not check the library
#include <gattlib.h>
// NOLINTEND(*)

namespace blecpp {
std::string gattLibErrorToString(int errorCode) {
  switch (errorCode) {
  case GATTLIB_SUCCESS:
    return "GATTLIB_SUCCESS";
  case GATTLIB_INVALID_PARAMETER:
    return "GATTLIB_INVALID_PARAMETER";
  case GATTLIB_NOT_FOUND:
    return "GATTLIB_NOT_FOUND";
  case GATTLIB_TIMEOUT:
    return "GATTLIB_TIMEOUT";
  case GATTLIB_OUT_OF_MEMORY:
    return "GATTLIB_OUT_OF_MEMORY";
  case GATTLIB_NOT_SUPPORTED:
    return "GATTLIB_NOT_SUPPORTED";
  case GATTLIB_DEVICE_ERROR:
    return "GATTLIB_DEVICE_ERROR";
  case GATTLIB_DEVICE_NOT_CONNECTED:
    return "GATTLIB_DEVICE_NOT_CONNECTED";
  case GATTLIB_NO_ADAPTER:
    return "GATTLIB_NO_ADAPTER";
  case GATTLIB_BUSY:
    return "GATTLIB_BUSY";
  case GATTLIB_UNEXPECTED:
    return "GATTLIB_UNEXPECTED";
  case GATTLIB_ADAPTER_CLOSE:
    return "GATTLIB_ADAPTER_CLOSE";
  case GATTLIB_DEVICE_DISCONNECTED:
    return "GATTLIB_DEVICE_DISCONNECTED";
  case GATTLIB_ERROR_DBUS:
    return "GATTLIB_ERROR_DBUS";
  case GATTLIB_ERROR_BLUEZ:
    return "GATTLIB_ERROR_BLUEZ";
  case GATTLIB_ERROR_UNIX:
    return "GATTLIB_ERROR_UNIX";
  default:
    return "Unknown error code";
  }
}
} // namespace blecpp
