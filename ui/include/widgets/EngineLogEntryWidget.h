#pragma once

#include <QFrame>

#include "Log/LogEntry.h"

class QLabel;

// A single read-only row in EngineLogWidget, showing one LogEntry's severity, text, and timestamp.
class EngineLogEntryWidget : public QFrame
{
  Q_OBJECT

public:
  explicit EngineLogEntryWidget(QWidget *parent = nullptr);

  void setEntry(const LogEntry &newEntry);

private:
  void updateSeverityStyle();

  LogEntry entry;
  QLabel *severityLabel = nullptr;
  QLabel *textLabel = nullptr;
  QLabel *timestampLabel = nullptr;
};
