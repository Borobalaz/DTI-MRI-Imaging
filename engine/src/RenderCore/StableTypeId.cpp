#include "RenderCore/StableTypeId.h"

#include <cassert>
#include <mutex>
#include <unordered_map>

namespace
{
  constexpr StableTypeId kFnvOffsetBasis = 14695981039346656037ull;
  constexpr StableTypeId kFnvPrime = 1099511628211ull;

  StableTypeId Fnv1a64(std::string_view value)
  {
    StableTypeId hash = kFnvOffsetBasis;
    for (const unsigned char character : value)
    {
      hash ^= static_cast<StableTypeId>(character);
      hash *= kFnvPrime;
    }

    return hash;
  }

  std::mutex& TypeRegistryMutex()
  {
    static std::mutex mutex;
    return mutex;
  }

  std::unordered_map<StableTypeId, std::string>& TypeNames()
  {
    static std::unordered_map<StableTypeId, std::string> names;
    return names;
  }
}

StableTypeId StableTypeIdFromName(std::string_view canonicalName)
{
  return Fnv1a64(canonicalName);
}

std::string StableTypeDebugName(StableTypeId typeId)
{
  std::lock_guard<std::mutex> lock(TypeRegistryMutex());
  const auto iterator = TypeNames().find(typeId);
  if (iterator == TypeNames().end())
  {
    return std::string("unknown");
  }

  return iterator->second;
}

StableTypeId detail::RegisterStableTypeId(StableTypeId typeId, std::string_view debugName)
{
  std::lock_guard<std::mutex> lock(TypeRegistryMutex());

  auto [iterator, inserted] = TypeNames().emplace(typeId, debugName);
  if (!inserted)
  {
    assert(iterator->second == debugName && "StableTypeId collision detected");
  }

  return typeId;
}
