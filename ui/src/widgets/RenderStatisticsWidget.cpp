#include "widgets/RenderStatisticsWidget.h"

#include <QFormLayout>
#include <QLabel>
#include <QTimer>

#include "state/RenderStatistics.h"
#include "styles/StyleSheetComposer.h"

namespace
{
  // RenderStatistics::statisticsChanged fires once per rendered frame, which can be
  //  hundreds/thousands of times a second with an uncapped render loop. Reacting to it
  //  directly would run QLabel::setText() (and the QSS repaint + dock layout invalidation
  //  it triggers) on the GUI thread inside that per-frame call stack, competing with
  //  rendering itself. Polling on a fixed-rate timer instead decouples the display refresh
  //  rate from the render loop's rate.
  constexpr int kRefreshIntervalMs = 100;
}

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

  refreshTimer = new QTimer(this);
  refreshTimer->setInterval(kRefreshIntervalMs);
  QObject::connect(refreshTimer, &QTimer::timeout, this, &RenderStatisticsWidget::refresh);
  refreshTimer->start();
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
  statistics = newStatistics;
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
