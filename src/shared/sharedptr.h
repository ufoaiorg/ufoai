/**
 * @file
 * @brief Alias for std::shared_ptr, replacing the former custom implementation.
 */

#pragma once

#include <memory>

template<class T>
using SharedPtr = std::shared_ptr<T>;
