#pragma once

#include <cstdint>
#include <string>

enum class LogSeverity { Message, Warning, Error };

struct LogEntry
{
  std::uint64_t sequenceId = 0;   // monotonic, assigned at Add() time — lets consumers append-only
  LogSeverity   severity = LogSeverity::Message;
  std::string   text;
  std::string   source;           // optional free-form tag, e.g. "VolumePass"; may be empty
  std::uint64_t timestampMs = 0;  // ms since Unix epoch; Qt formats via QDateTime::fromMSecsSinceEpoch
};
