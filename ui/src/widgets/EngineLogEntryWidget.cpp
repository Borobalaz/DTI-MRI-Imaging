#include "widgets/EngineLogEntryWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
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

  auto *layout = new QHBoxLayout(this);
  const int marginH = ResolveCommonToken("provider-item-margin-h").toInt();
  const int marginV = ResolveCommonToken("provider-item-margin-v").toInt();
  layout->setContentsMargins(marginH, marginV, marginH, marginV);
  layout->setSpacing(ResolveCommonToken("provider-item-spacing").toInt());

  severityLabel = new QLabel(this);
  severityLabel->setObjectName("engineLogSeverityLabel");
  layout->addWidget(severityLabel);

  timestampLabel = new QLabel(this);
  timestampLabel->setObjectName("engineLogTimestampLabel");
  layout->addWidget(timestampLabel);

  textLabel = new QLabel(this);
  textLabel->setObjectName("engineLogTextLabel");
  textLabel->setWordWrap(true);
  textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(textLabel, 1);

  updateSeverityStyle();
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
