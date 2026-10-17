#pragma once

#include <cstdint>
#include <vector>

#include <QFrame>

#include "Log/ILogService.h"

class QHBoxLayout;
class QScrollArea;
class QVBoxLayout;
class QToolButton;
class EngineLogEntryWidget;

// Read-only dock panel displaying the engine's log entries (Log/ILogService), appended to as new
//  entries arrive rather than rebuilt from scratch each time, so scroll position isn't lost.
class EngineLogWidget : public QFrame
{
  Q_OBJECT

public:
  explicit EngineLogWidget(QWidget *parent = nullptr);

  // Attaches the engine's log service and performs the initial sync. Subscribes to push-based
  //  "entries changed" notifications instead of per-frame polling.
  void SetLogService(ILogService *service);

  // Re-reads layout metrics (margin/spacing/min-width) from tokens/common.ini and re-applies
  //  them, so editing that file updates an already-running instance (called from
  //  WidgetsMainWindow::applyTheme() whenever a style source file change is detected).
  void refreshLayoutMetrics();

private:
  void handleEntriesChanged();
  void appendRow(const LogEntry &entry);
  void clearRows();
  bool isScrolledToBottom() const;
  bool isSeverityVisible(LogSeverity severity) const;
  void setSeverityFilterVisible(LogSeverity severity, bool visible);

  ILogService *logService = nullptr;
  std::uint64_t lastAppendedSequenceId = 0;

  bool showMessage = true;
  bool showWarning = true;
  bool showError = true;

  bool hasSelection = false;
  quint64 selectedSequenceId = 0;

  QVBoxLayout *panelLayout = nullptr;
  QHBoxLayout *headerLayout = nullptr;
  QToolButton *filterMessageButton = nullptr;
  QToolButton *filterWarningButton = nullptr;
  QToolButton *filterErrorButton = nullptr;
  QToolButton *clearButton = nullptr;
  QScrollArea *scrollArea = nullptr;
  QWidget *listContainer = nullptr;
  QVBoxLayout *listLayout = nullptr;
  std::vector<EngineLogEntryWidget *> rows;
};
