#pragma once

#include <QFrame>

#include "Log/LogEntry.h"

class QEvent;
class QLabel;
class QMouseEvent;
class QObject;

// A single row in EngineLogWidget, showing one LogEntry's severity, text, and timestamp.
// Selectable/clickable (mirrors InspectProviderWidget's selection idiom) and its text is
//  mouse/keyboard-selectable for copy.
class EngineLogEntryWidget : public QFrame
{
  Q_OBJECT

public:
  explicit EngineLogEntryWidget(QWidget *parent = nullptr);

  void setEntry(const LogEntry &newEntry);
  void setSelected(bool isSelected);

  quint64 sequenceId() const { return entry.sequenceId; }
  LogSeverity severity() const { return entry.severity; }

signals:
  void clicked(quint64 sequenceId);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void updateSeverityStyle();
  void updateSelectionStyle();
  void handleRowClicked();

  LogEntry entry;
  bool selected = false;
  QLabel *severityLabel = nullptr;
  QLabel *textLabel = nullptr;
  QLabel *timestampLabel = nullptr;
};
