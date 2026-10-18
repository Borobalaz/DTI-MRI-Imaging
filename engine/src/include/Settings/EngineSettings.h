#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Inspection/InspectField.h"

/**
 * @brief Thread-safe static/global settings catalog. Unlike a plain registry of borrowed
 *  pointers, this class OWNS each setting's canonical value, keyed by id -- Declare() installs
 *  getter/setter closures that read/write this class's own storage (not the declaring object's
 *  member), so any engine-internal code can query or set any setting by id via the typed
 *  accessors below, independent of whoever declared it and independent of that object's lifetime.
 *  Declarations are permanent for the process -- there is no Unregister, because nothing captures
 *  the declaring object anymore, so nothing can dangle if it's destroyed.
 *
 *  Safe to call Declare() from any constructor, any number of times, in any order relative to
 *  Engine's or any other subsystem's construction (mirrors why EngineLog.h's registry is static
 *  rather than Engine-instance-owned).
 */
class EngineSettings
{
public:
  // Declares a setting from a locally-built InspectField (build it with MakeInspectField exactly
  // like any other InspectField call site: id/displayName/groupName/type/defaultValue; optionally
  // customize minimum/maximum/step/enumOptions/readOnly on it before calling Declare, same as
  // existing call sites do for e.g. Camera::GetInspectFields()). Declare() installs its own
  // getter/setter closures on `descriptor` that read/write this registry's canonical storage for
  // `descriptor->id` -- any getter/setter you passed to MakeInspectField is overwritten and ignored.
  //
  // Idempotent: if `descriptor->id` is already declared (by this or any other call site),
  // `descriptor` is discarded and the pre-existing canonical field is returned unchanged (the
  // stored value is NOT reset; the original descriptor metadata wins). If displayName/groupName/
  // type differs from the original declaration, a one-time warning is logged (likely an id
  // collision/typo), but the call still succeeds and returns the original field.
  static InspectFieldPtr Declare(InspectFieldPtr descriptor);

  // Direct typed access by id, callable by any engine-internal code regardless of who declared
  // the setting. If `id` was never declared, or was declared with an incompatible type, the Get*
  // overload logs a one-time warning and returns `fallback`; the Set* overload logs a one-time
  // warning and leaves the stored value untouched.
  static bool GetBool(const std::string& id, bool fallback = false);
  static void SetBool(const std::string& id, bool value);

  static int GetInt(const std::string& id, int fallback = 0);
  static void SetInt(const std::string& id, int value);

  static double GetNumber(const std::string& id, double fallback = 0.0);
  static void SetNumber(const std::string& id, double value);

  static std::string GetText(const std::string& id, const std::string& fallback = {});
  static void SetText(const std::string& id, const std::string& value);

  static glm::vec3 GetVector3(const std::string& id, const glm::vec3& fallback = glm::vec3(0.0f));
  static void SetVector3(const std::string& id, const glm::vec3& value);

  static std::vector<InspectFieldPtr> Snapshot(); // for SettingsService only
  static std::uint64_t Generation();
};
