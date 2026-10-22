#include "Log/EngineLog.h"

#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_set>

namespace
{
  constexpr std::size_t kRetentionCap = 1000; // ring-buffer cap - keeps Snapshot() cheap to copy wholesale

  std::mutex& LogMutex()
  {
    static std::mutex mutex;
    return mutex;
  }

  std::deque<LogEntry>& LogEntries()
  {
    static std::deque<LogEntry> entries;
    return entries;
  }

  std::unordered_set<std::string>& SeenOnceKeys()
  {
    static std::unordered_set<std::string> keys;
    return keys;
  }

  std::uint64_t& NextSequenceId()
  {
    static std::uint64_t nextSequenceId = 1;
    return nextSequenceId;
  }

  std::uint64_t& GenerationCounter()
  {
    static std::uint64_t generation = 0;
    return generation;
  }

  std::uint64_t CurrentTimestampMs()
  {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
  }
}

namespace
{
  // Assumes the caller already holds LogMutex() - factored out so AddOnce() can check-and-append
  // atomically under a single lock without double-locking the non-recursive mutex.
  void AppendLocked(LogSeverity severity, std::string_view text, std::string_view source)
  {
    LogEntry entry;
    entry.sequenceId = NextSequenceId()++;
    entry.severity = severity;
    entry.text = std::string(text);
    entry.source = std::string(source);
    entry.timestampMs = CurrentTimestampMs();

    std::deque<LogEntry>& entries = LogEntries();
    entries.push_back(std::move(entry));
    if (entries.size() > kRetentionCap)
    {
      entries.pop_front();
    }

    ++GenerationCounter();
  }
}

void EngineLog::Add(LogSeverity severity, std::string_view text, std::string_view source)
{
  std::lock_guard<std::mutex> lock(LogMutex());
  AppendLocked(severity, text, source);
}

void EngineLog::Message(std::string_view text, std::string_view source)
{
  Add(LogSeverity::Message, text, source);
}

void EngineLog::Warning(std::string_view text, std::string_view source)
{
  Add(LogSeverity::Warning, text, source);
}

void EngineLog::Error(std::string_view text, std::string_view source)
{
  Add(LogSeverity::Error, text, source);
}

void EngineLog::AddOnce(LogSeverity severity, std::string_view text, std::string_view source)
{
  std::lock_guard<std::mutex> lock(LogMutex());

  std::string key;
  key.reserve(source.size() + text.size() + 2);
  key += std::to_string(static_cast<int>(severity));
  key += '\x1f';
  key += source;
  key += '\x1f';
  key += text;

  std::unordered_set<std::string>& seenKeys = SeenOnceKeys();
  if (seenKeys.insert(std::move(key)).second)
  {
    AppendLocked(severity, text, source);
  }
}

void EngineLog::MessageOnce(std::string_view text, std::string_view source)
{
  AddOnce(LogSeverity::Message, text, source);
}

void EngineLog::WarningOnce(std::string_view text, std::string_view source)
{
  AddOnce(LogSeverity::Warning, text, source);
}

void EngineLog::ErrorOnce(std::string_view text, std::string_view source)
{
  AddOnce(LogSeverity::Error, text, source);
}

std::vector<LogEntry> EngineLog::Snapshot()
{
  std::lock_guard<std::mutex> lock(LogMutex());
  const std::deque<LogEntry>& entries = LogEntries();
  return std::vector<LogEntry>(entries.begin(), entries.end());
}

std::uint64_t EngineLog::Generation()
{
  std::lock_guard<std::mutex> lock(LogMutex());
  return GenerationCounter();
}
