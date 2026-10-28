#pragma once

#include <cstdint>
#include <string>
#include <string_view>

using StableTypeId = std::uint64_t;

StableTypeId StableTypeIdFromName(std::string_view canonicalName);
std::string StableTypeDebugName(StableTypeId typeId);

namespace detail
{
  StableTypeId RegisterStableTypeId(StableTypeId typeId, std::string_view debugName);
}

template<typename T>
StableTypeId typeId()
{
#if defined(__FUNCSIG__)
  constexpr std::string_view debugName = __FUNCSIG__;
#else
  constexpr std::string_view debugName = __PRETTY_FUNCTION__;
#endif

  static const StableTypeId cachedTypeId = detail::RegisterStableTypeId(
    StableTypeIdFromName(debugName),
    debugName);
  return cachedTypeId;
}
