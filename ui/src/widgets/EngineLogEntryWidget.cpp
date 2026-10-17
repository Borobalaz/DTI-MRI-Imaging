#include "widgets/EngineLogEntryWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>

#include "styles/StyleSheetComposer.h"

namespace
{
  QString SeverityPropertyValue(LogSeverity severity)
  {
    switch (severity)
    {
      case LogSeverity::Warning: return QStringLiteral("warning");
      case LogSeverity::Error:   return QStringLiteral("error");
      case LogSeverity::Message:
      default:                  return QStringLiteral("message");
    }
  }

  QString SeverityDisplayText(LogSeverity severity)
  {
    switch (severity)
    {
      case LogSeverity::Warning: return QStringLiteral("Warning");
      case LogSeverity::Error:   return QStringLiteral("Error");
      case LogSeverity::Message:
      default:                  return QStringLiteral("Message");
    }
  }
}

EngineLogEntryWidget::EngineLogEntryWidget(QWidget *parent)
    : QFrame(parent)
{
  setObjectName("engineLogEntryItem");
  setFrameShape(QFrame::NoFrame);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumHeight(ResolveCommonToken("log-row-min-height").toInt());

  auto *layout = new QHBoxLayout(this);
  const int marginH = ResolveCommonToken("log-row-margin-h").toInt();
  const int marginV = ResolveCommonToken("log-row-margin-v").toInt();
  layout->setContentsMargins(marginH, marginV, marginH, marginV);
  layout->setSpacing(ResolveCommonToken("log-row-spacing").toInt());

  severityLabel = new QLabel(this);
  severityLabel->setObjectName("engineLogSeverityLabel");
  severityLabel->setFixedWidth(ResolveCommonToken("log-severity-column-width").toInt());
  severityLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  layout->addWidget(severityLabel);

  timestampLabel = new QLabel(this);
  timestampLabel->setObjectName("engineLogTimestampLabel");
  timestampLabel->setFixedWidth(ResolveCommonToken("log-timestamp-column-width").toInt());
  timestampLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  layout->addWidget(timestampLabel);

  textLabel = new QLabel(this);
  textLabel->setObjectName("engineLogTextLabel");
  textLabel->setWordWrap(true);
  textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
  textLabel->installEventFilter(this);
  layout->addWidget(textLabel, 1);

  updateSeverityStyle();
  updateSelectionStyle();
}

void EngineLogEntryWidget::setEntry(const LogEntry &newEntry)
{
  entry = newEntry;

  severityLabel->setText(SeverityDisplayText(entry.severity));
  timestampLabel->setText(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(entry.timestampMs)).toString("HH:mm:ss"));

  const QString text = QString::fromStdString(entry.text);
  textLabel->setText(entry.source.empty() ? text : QStringLiteral("[%1] %2").arg(QString::fromStdString(entry.source), text));

  updateSeverityStyle();
}

void EngineLogEntryWidget::updateSeverityStyle()
{
  setProperty("severity", SeverityPropertyValue(entry.severity));
  style()->unpolish(this);
  style()->polish(this);
  update();
}

void EngineLogEntryWidget::setSelected(bool isSelected)
{
  selected = isSelected;
  updateSelectionStyle();
}

void EngineLogEntryWidget::updateSelectionStyle()
{
  setProperty("selected", selected);
  style()->unpolish(this);
  style()->polish(this);
  update();
}

// Only a row holding a real entry (non-zero sequence id) can be selected/clicked.
void EngineLogEntryWidget::handleRowClicked()
{
  if (entry.sequenceId == 0)
  {
    return;
  }

  emit clicked(entry.sequenceId);
}

// On left click, select this row. Still catches clicks on the frame background and the
//  non-interactive severity/timestamp columns (textLabel's own clicks go through eventFilter()
//  below, since its text-selection flags intercept mouse presses before they'd reach here).
void EngineLogEntryWidget::mousePressEvent(QMouseEvent *event)
{
  if (event && event->button() == Qt::LeftButton)
  {
    handleRowClicked();
  }

  QFrame::mousePressEvent(event);
}

bool EngineLogEntryWidget::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == textLabel && event->type() == QEvent::MouseButtonPress)
  {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton)
    {
      handleRowClicked();
    }

    // Never consume the event - textLabel still needs it for its own text-selection handling.
    return false;
  }

  return QFrame::eventFilter(watched, event);
}
