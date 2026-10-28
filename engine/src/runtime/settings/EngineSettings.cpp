#include "runtime/settings/EngineSettings.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

#include "foundation/log/EngineLog.h"

namespace
{
  std::mutex& SettingsMutex()
  {
    static std::mutex mutex;
    return mutex;
  }

  std::vector<InspectFieldPtr>& Entries()
  {
    static std::vector<InspectFieldPtr> entries;
    return entries;
  }

  std::unordered_map<std::string, InspectValue>& Values()
  {
    static std::unordered_map<std::string, InspectValue> values;
    return values;
  }

  std::uint64_t& GenerationCounter()
  {
    static std::uint64_t generation = 0;
    return generation;
  }

  // Assumes the caller already holds SettingsMutex().
  InspectField* FindEntryLocked(const std::string& id)
  {
    std::vector<InspectFieldPtr>& entries = Entries();
    const auto existing = std::find_if(entries.begin(), entries.end(), [&id](const InspectFieldPtr& entry)
    {
      return entry && entry->id == id;
    });

    return existing == entries.end() ? nullptr : existing->get();
  }

  InspectValue RawGetValue(const std::string& id)
  {
    std::lock_guard<std::mutex> lock(SettingsMutex());

    std::unordered_map<std::string, InspectValue>& values = Values();
    const auto existing = values.find(id);
    if (existing == values.end())
    {
      EngineLog::WarningOnce("EngineSettings: '" + id + "' queried before being declared", "EngineSettings");
      return InspectValue{};
    }

    return existing->second;
  }

  // Type-check granularity: compares the setting's declared InspectFieldType against the type
  // the calling typed accessor expects, exactly -- the same single check is reused by Declare()'s
  // own setter closure (which always passes its own declared type, so it trivially matches), so
  // there is one consistent rule for every accessor rather than a looser variant-alternative check
  // for some types and an exact-type check for others.
  bool RawSetValue(const std::string& id, const InspectValue& value, InspectFieldType expectedType)
  {
    std::lock_guard<std::mutex> lock(SettingsMutex());

    InspectField* entry = FindEntryLocked(id);
    if (!entry)
    {
      EngineLog::WarningOnce("EngineSettings: Set* called for undeclared id '" + id + "'", "EngineSettings");
      return false;
    }

    if (entry->type != expectedType)
    {
      EngineLog::WarningOnce("EngineSettings: Set* type mismatch for '" + id + "'", "EngineSettings");
      return false;
    }

    Values()[id] = value;
    return true;
  }
}

InspectFieldPtr EngineSettings::Declare(InspectFieldPtr descriptor)
{
  if (!descriptor)
  {
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(SettingsMutex());

  std::vector<InspectFieldPtr>& entries = Entries();
  const auto existing = std::find_if(entries.begin(), entries.end(),
      [&descriptor](const InspectFieldPtr& entry) { return entry && entry->id == descriptor->id; });

  if (existing != entries.end())
  {
    if ((*existing)->displayName != descriptor->displayName ||
        (*existing)->groupName != descriptor->groupName ||
        (*existing)->type != descriptor->type)
    {
      EngineLog::WarningOnce(
          "EngineSettings: '" + descriptor->id + "' re-declared with different metadata; keeping original",
          "EngineSettings");
    }
    return *existing;
  }

  const std::string id = descriptor->id;
  Values()[id] = descriptor->initialValue;

  descriptor->getter = [id]() -> InspectValue { return RawGetValue(id); };
  descriptor->setter = [id, type = descriptor->type](const InspectValue& value) { RawSetValue(id, value, type); };

  entries.push_back(descriptor);
  ++GenerationCounter();
  return descriptor;
}

bool EngineSettings::GetBool(const std::string& id, bool fallback)
{
  const InspectValue value = RawGetValue(id);
  if (const bool* boolValue = std::get_if<bool>(&value))
  {
    return *boolValue;
  }
  return fallback;
}

void EngineSettings::SetBool(const std::string& id, bool value)
{
  RawSetValue(id, InspectValue(value), InspectFieldType::Boolean);
}

int EngineSettings::GetInt(const std::string& id, int fallback)
{
  const InspectValue value = RawGetValue(id);
  if (const double* numberValue = std::get_if<double>(&value))
  {
    return static_cast<int>(*numberValue);
  }
  return fallback;
}

void EngineSettings::SetInt(const std::string& id, int value)
{
  RawSetValue(id, InspectValue(static_cast<double>(value)), InspectFieldType::Number);
}

double EngineSettings::GetNumber(const std::string& id, double fallback)
{
  const InspectValue value = RawGetValue(id);
  if (const double* numberValue = std::get_if<double>(&value))
  {
    return *numberValue;
  }
  return fallback;
}

void EngineSettings::SetNumber(const std::string& id, double value)
{
  RawSetValue(id, InspectValue(value), InspectFieldType::Number);
}

std::string EngineSettings::GetText(const std::string& id, const std::string& fallback)
{
  const InspectValue value = RawGetValue(id);
  if (const std::string* textValue = std::get_if<std::string>(&value))
  {
    return *textValue;
  }
  return fallback;
}

void EngineSettings::SetText(const std::string& id, const std::string& value)
{
  RawSetValue(id, InspectValue(value), InspectFieldType::Text);
}

glm::vec3 EngineSettings::GetVector3(const std::string& id, const glm::vec3& fallback)
{
  const InspectValue value = RawGetValue(id);
  if (const glm::vec3* vectorValue = std::get_if<glm::vec3>(&value))
  {
    return *vectorValue;
  }
  return fallback;
}

void EngineSettings::SetVector3(const std::string& id, const glm::vec3& value)
{
  RawSetValue(id, InspectValue(value), InspectFieldType::Vector3);
}

std::vector<InspectFieldPtr> EngineSettings::Snapshot()
{
  std::lock_guard<std::mutex> lock(SettingsMutex());
  return Entries();
}

std::uint64_t EngineSettings::Generation()
{
  std::lock_guard<std::mutex> lock(SettingsMutex());
  return GenerationCounter();
}
