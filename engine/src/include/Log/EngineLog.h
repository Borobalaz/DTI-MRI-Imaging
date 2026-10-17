#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "Log/LogEntry.h"

/**
 * @brief Thread-safe static/global log registry engine objects append diagnostic entries to.
 *  Mirrors StableTypeId.cpp's function-local-static + mutex pattern (engine/src/RenderCore/StableTypeId.cpp)
 *  rather than threading a logger reference through every constructor. Internal engine detail;
 *  only reachable through EngineLogService (and, from app_qt, through Engine::GetLogService()).
 */
class EngineLog
{
public:
  static void Add(LogSeverity severity, std::string_view text, std::string_view source = {});
  static void Message(std::string_view text, std::string_view source = {});
  static void Warning(std::string_view text, std::string_view source = {});
  static void Error(std::string_view text, std::string_view source = {});

  static std::vector<LogEntry> Snapshot(); // for EngineLogService only
  static std::uint64_t Generation();
};
