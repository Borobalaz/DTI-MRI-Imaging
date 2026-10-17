#include "Log/EngineLog.h"

#include <chrono>
#include <deque>
#include <mutex>

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

void EngineLog::Add(LogSeverity severity, std::string_view text, std::string_view source)
{
  std::lock_guard<std::mutex> lock(LogMutex());

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
