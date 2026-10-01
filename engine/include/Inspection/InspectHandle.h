#pragma once

#include <memory>
#include <string>

#include "Inspection/InspectField.h"

/**
 * @brief Lifetime-safe reference to an InspectProvider. Never holds a raw pointer;
 *  callers must check IsValid() (or rely on IInspectionService to no-op safely) before use.
 */
class InspectHandle
{
public:
  InspectHandle() = default;
  InspectHandle(std::weak_ptr<InspectProvider> providerRef, std::string providerId)
    : provider(std::move(providerRef))
    , id(std::move(providerId))
  {
  }

  bool IsValid() const { return !provider.expired(); }
  const std::string& GetId() const { return id; }

  std::shared_ptr<InspectProvider> Lock() const { return provider.lock(); }

private:
  std::weak_ptr<InspectProvider> provider;
  std::string id;
};
