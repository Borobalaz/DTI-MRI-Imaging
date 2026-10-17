#include "widgets/EngineLogWidget.h"

#include <QHBoxLayout>
#include <QLabel>
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
  refreshLayoutMetrics();

  // Header row: title + a Clear button local to this widget (the shared DockTitleBarWidget
  //  stays generic, so widget-specific controls like this live in the widget's own header).
  auto *headerRow = new QWidget(this);
  auto *headerLayout = new QHBoxLayout(headerRow);
  headerLayout->setContentsMargins(0, 0, 0, 0);

  auto *titleLabel = new QLabel(tr("Log"), headerRow);
  titleLabel->setObjectName("engineLogHeaderLabel");
  headerLayout->addWidget(titleLabel, 1);

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
}

void EngineLogWidget::refreshLayoutMetrics()
{
  const int margin = ResolveCommonToken("panel-margin").toInt();
  panelLayout->setContentsMargins(margin, margin, margin, margin);
  panelLayout->setSpacing(ResolveCommonToken("panel-spacing").toInt());
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
