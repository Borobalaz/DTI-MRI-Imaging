#pragma once

#include <string>

/**
 * @brief Value-type snapshot of an inspectable object for list/row display.
 *  Never holds a reference into the engine, so it can outlive the underlying object safely.
 */
struct InspectObjectSummary
{
  std::string id;           // stable identifier (InspectHandle::GetId())
  std::string displayName;  // human-readable name shown in the UI
  bool hasVisibility = false;
  bool isVisible = true;
};
