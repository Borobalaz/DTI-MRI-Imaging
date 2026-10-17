#include "widgets/EngineLogWidget.h"

#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "styles/StyleSheetComposer.h"
#include "widgets/EngineLogEntryWidget.h"

EngineLogWidget::EngineLogWidget(QWidget *parent)
    : QFrame(parent)
{
  panelLayout = new QVBoxLayout(this);

  // Header row: severity filter toggles + a Clear button local to this widget (the shared
  //  DockTitleBarWidget stays generic, so widget-specific controls like this live in the
  //  widget's own header; the dock's own title bar already shows "Engine Log").
  auto *headerRow = new QWidget(this);
  headerLayout = new QHBoxLayout(headerRow);
  headerLayout->setContentsMargins(2, 2, 2, 2);

  filterMessageButton = new QToolButton(headerRow);
  filterMessageButton->setObjectName("engineLogFilterButton");
  filterMessageButton->setText(tr("Message"));
  filterMessageButton->setCheckable(true);
  filterMessageButton->setChecked(true);
  filterMessageButton->setAutoRaise(true);
  filterMessageButton->setProperty("severity", "message");
  headerLayout->addWidget(filterMessageButton);

  filterWarningButton = new QToolButton(headerRow);
  filterWarningButton->setObjectName("engineLogFilterButton");
  filterWarningButton->setText(tr("Warning"));
  filterWarningButton->setCheckable(true);
  filterWarningButton->setChecked(true);
  filterWarningButton->setAutoRaise(true);
  filterWarningButton->setProperty("severity", "warning");
  headerLayout->addWidget(filterWarningButton);

  filterErrorButton = new QToolButton(headerRow);
  filterErrorButton->setObjectName("engineLogFilterButton");
  filterErrorButton->setText(tr("Error"));
  filterErrorButton->setCheckable(true);
  filterErrorButton->setChecked(true);
  filterErrorButton->setAutoRaise(true);
  filterErrorButton->setProperty("severity", "error");
  headerLayout->addWidget(filterErrorButton);

  QObject::connect(filterMessageButton, &QToolButton::toggled, this, [this](bool checked)
  {
    setSeverityFilterVisible(LogSeverity::Message, checked);
  });
  QObject::connect(filterWarningButton, &QToolButton::toggled, this, [this](bool checked)
  {
    setSeverityFilterVisible(LogSeverity::Warning, checked);
  });
  QObject::connect(filterErrorButton, &QToolButton::toggled, this, [this](bool checked)
  {
    setSeverityFilterVisible(LogSeverity::Error, checked);
  });

  headerLayout->addStretch(1);

  clearButton = new QToolButton(headerRow);
  clearButton->setObjectName("engineLogClearButton");
  clearButton->setText(tr("Clear"));
  clearButton->setAutoRaise(true);
  QObject::connect(clearButton, &QToolButton::clicked, this, [this]()
  {
    clearRows();
  });
  headerLayout->addWidget(clearButton);

  panelLayout->addWidget(headerRow);

  scrollArea = new QScrollArea(this);
  scrollArea->setObjectName("engineLogScrollArea");
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  listContainer = new QWidget(scrollArea);
  listLayout = new QVBoxLayout(listContainer);
  listLayout->setContentsMargins(0, 0, 0, 0);
  listLayout->setSpacing(4);
  listLayout->addStretch(1);

  scrollArea->setWidget(listContainer);
  panelLayout->addWidget(scrollArea, 1);

  refreshLayoutMetrics();
}

void EngineLogWidget::refreshLayoutMetrics()
{
  const int margin = ResolveCommonToken("panel-margin").toInt();
  const int spacing = ResolveCommonToken("panel-spacing").toInt();
  panelLayout->setContentsMargins(margin, spacing, margin, margin);
  panelLayout->setSpacing(spacing);
  headerLayout->setSpacing(ResolveCommonToken("log-header-spacing").toInt());
}

void EngineLogWidget::SetLogService(ILogService *service)
{
  logService = service;

  if (logService)
  {
    logService->OnEntriesChanged([this]()
    {
      handleEntriesChanged();
    });
  }

  handleEntriesChanged();
}

// Appends any entries newer than lastAppendedSequenceId. Since Engine::Update() runs on the
//  GUI thread (OpenGLViewportWidget::paintGL()), this can call back into Qt directly - no
//  queued connection needed (same assumption QTSceneInspector already relies on).
void EngineLogWidget::handleEntriesChanged()
{
  if (!logService)
  {
    return;
  }

  const std::vector<LogEntry> entries = logService->GetEntries();
  for (const LogEntry &entry : entries)
  {
    if (entry.sequenceId > lastAppendedSequenceId)
    {
      appendRow(entry);
      lastAppendedSequenceId = entry.sequenceId;
    }
  }
}

void EngineLogWidget::appendRow(const LogEntry &entry)
{
  const bool wasAtBottom = isScrolledToBottom();

  auto *rowWidget = new EngineLogEntryWidget(listContainer);
  rowWidget->setEntry(entry);
  rowWidget->setVisible(isSeverityVisible(entry.severity));

  QObject::connect(rowWidget, &EngineLogEntryWidget::clicked, this, [this](quint64 id)
  {
    hasSelection = true;
    selectedSequenceId = id;

    for (EngineLogEntryWidget *row : rows)
    {
      if (!row)
      {
        continue;
      }

      row->setSelected(row->sequenceId() == selectedSequenceId);
    }
  });

  rows.push_back(rowWidget);
  listLayout->insertWidget(listLayout->count() - 1, rowWidget);

  if (wasAtBottom && scrollArea)
  {
    // The scroll area's range only grows once the new row's layout is processed, so defer the
    //  scroll-to-bottom until after that - scheduling it now (synchronously) would still see
    //  the old, smaller range.
    QTimer::singleShot(0, this, [this]()
    {
      if (QScrollBar *verticalScrollBar = scrollArea->verticalScrollBar())
      {
        verticalScrollBar->setValue(verticalScrollBar->maximum());
      }
    });
  }
}

// Clears displayed rows only - does not touch lastAppendedSequenceId or call into ILogService,
//  so entries already shown stay cleared and only genuinely new entries reappear afterward.
void EngineLogWidget::clearRows()
{
  for (EngineLogEntryWidget *row : rows)
  {
    if (row)
    {
      row->deleteLater();
    }
  }
  rows.clear();

  hasSelection = false;
  selectedSequenceId = 0;
}

bool EngineLogWidget::isScrolledToBottom() const
{
  if (!scrollArea || !scrollArea->verticalScrollBar())
  {
    return true;
  }

  const QScrollBar *verticalScrollBar = scrollArea->verticalScrollBar();
  return verticalScrollBar->value() >= verticalScrollBar->maximum();
}

bool EngineLogWidget::isSeverityVisible(LogSeverity severity) const
{
  switch (severity)
  {
    case LogSeverity::Message: return showMessage;
    case LogSeverity::Warning: return showWarning;
    case LogSeverity::Error:   return showError;
  }
  return true;
}

// Updates the filter state for one severity and re-applies visibility to every already-displayed
//  row of that severity - QScrollArea preserves its (clamped) scroll position automatically when
//  content visibility changes, so no extra scroll handling is needed here.
void EngineLogWidget::setSeverityFilterVisible(LogSeverity severity, bool visible)
{
  switch (severity)
  {
    case LogSeverity::Message: showMessage = visible; break;
    case LogSeverity::Warning: showWarning = visible; break;
    case LogSeverity::Error:   showError = visible; break;
  }

  for (EngineLogEntryWidget *row : rows)
  {
    if (row && row->severity() == severity)
    {
      row->setVisible(isSeverityVisible(severity));
    }
  }
}
