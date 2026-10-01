#pragma once

#include <initializer_list>
#include <string>
#include <unordered_set>

#include "RenderCore/StableTypeId.h"

using RendererTypeId = StableTypeId;
using CapabilityId = StableTypeId;

class CapabilitySet
{
public:
  CapabilitySet() = default;
  CapabilitySet(std::initializer_list<CapabilityId> values)
    : capabilities(values)
  {
  }

  bool Contains(CapabilityId capability) const
  {
    return capabilities.find(capability) != capabilities.end();
  }

  bool ContainsAll(const CapabilitySet& required) const
  {
    for (const CapabilityId capability : required.capabilities)
    {
      if (!Contains(capability))
      {
        return false;
      }
    }

    return true;
  }

  const std::unordered_set<CapabilityId>& Values() const
  {
    return capabilities;
  }

private:
  std::unordered_set<CapabilityId> capabilities;
};

struct RendererDescriptor
{
  RendererTypeId type = 0;
  std::string name;
  CapabilitySet capabilities;
};

namespace RenderCapabilities
{
  inline const CapabilityId Rasterization = StableTypeIdFromName("capability.rasterization");
  inline const CapabilityId RayTracing = StableTypeIdFromName("capability.raytracing");
  inline const CapabilityId Compute = StableTypeIdFromName("capability.compute");
  inline const CapabilityId DeferredShading = StableTypeIdFromName("capability.deferred");
  inline const CapabilityId ForwardShading = StableTypeIdFromName("capability.forward");
  inline const CapabilityId ParticipatingMedia = StableTypeIdFromName("capability.participatingMedia");
  inline const CapabilityId SignedDistanceFields = StableTypeIdFromName("capability.signedDistanceFields");
}
