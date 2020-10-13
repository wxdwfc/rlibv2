#pragma once

#if defined(__GNUC__) && __GNUC__ < 7
#include <experimental/optional>
#ifndef optional
#define optional experimental::optional
#endif
#else
#include <optional>
#endif

namespace rdmaio {
template <typename T> using Option = std::optional<T>;
}
