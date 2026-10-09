#include "widgets/RenderStatisticsWidget.h"

#include <QFormLayout>
#include <QLabel>

#include "state/RenderStatistics.h"
#include "styles/StyleSheetComposer.h"

RenderStatisticsWidget::RenderStatisticsWidget(QWidget *parent)
  : QFrame(parent)
{
  formLayout = new QFormLayout(this);
  refreshLayoutMetrics();

  fpsValueLabel = new QLabel("0.00", this);
  averageFpsValueLabel = new QLabel("0.00", this);
  renderTimeValueLabel = new QLabel("0.000 ms", this);
  averageRenderTimeValueLabel = new QLabel("0.000 ms", this);

  formLayout->addRow("FPS", fpsValueLabel);
  formLayout->addRow("Average FPS", averageFpsValueLabel);
  formLayout->addRow("Render Time", renderTimeValueLabel);
  formLayout->addRow("Avg Render Time", averageRenderTimeValueLabel);
}

void RenderStatisticsWidget::refreshLayoutMetrics()
{
  setMinimumWidth(ResolveCommonToken("stats-min-width").toInt());

  const int margin = ResolveCommonToken("stats-margin").toInt();
  formLayout->setContentsMargins(margin, margin, margin, margin);
  formLayout->setHorizontalSpacing(ResolveCommonToken("stats-spacing-h").toInt());
  formLayout->setVerticalSpacing(ResolveCommonToken("stats-spacing-v").toInt());
}

void RenderStatisticsWidget::setRenderStatistics(RenderStatistics *newStatistics)
{
  if (statistics == newStatistics)
  {
    return;
  }

  if (statistics)
  {
    QObject::disconnect(statistics, nullptr, this, nullptr);
  }

  statistics = newStatistics;

  if (statistics)
  {
    QObject::connect(statistics, &RenderStatistics::statisticsChanged, this, [this]()
    {
      refresh();
    });
  }

  refresh();
}

void RenderStatisticsWidget::refresh()
{
  if (!statistics)
  {
    fpsValueLabel->setText("-");
    averageFpsValueLabel->setText("-");
    renderTimeValueLabel->setText("-");
    averageRenderTimeValueLabel->setText("-");
    return;
  }

  fpsValueLabel->setText(QString::number(statistics->fps(), 'f', 2));
  averageFpsValueLabel->setText(QString::number(statistics->averageFps(), 'f', 2));
  renderTimeValueLabel->setText(QString::number(statistics->renderTime(), 'f', 3) + " ms");
  averageRenderTimeValueLabel->setText(QString::number(statistics->averageRenderTime(), 'f', 3) + " ms");
}
