/**
 * @file gattlib_utils.hpp
 * @author Cristian Troncoso <ctroncoso.ai@gmail.com>
 * @date 2025-06-10
 *
 * @brief Utility functions for gattlib
 */

#pragma once

#include <string>

namespace blecpp {
/**
 * @brief Helper function to convert gattlib errors codes to string
 * @param error_code The error code
 * @return The string error
 */
std::string gattLibErrorToString(int error_code);

} // namespace blecpp
