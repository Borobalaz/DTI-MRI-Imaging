#include "ui/widgets/RenderStatisctisWidget.h"

#include <QFormLayout>
#include <QLabel>

#include "ui/widgets/RenderStatistics.h"

RenderStatisctisWidget::RenderStatisctisWidget(QWidget *parent)
  : QFrame(parent)
{
  setObjectName("renderStatsPanel");
  setMinimumWidth(280);
  setMaximumWidth(280);

  auto *layout = new QFormLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setHorizontalSpacing(12);
  layout->setVerticalSpacing(6);

  auto *titleLabel = new QLabel("Render Statistics", this);
  titleLabel->setObjectName("panelTitle");
  layout->addRow(titleLabel);

  fpsValueLabel = new QLabel("0.00", this);
  averageFpsValueLabel = new QLabel("0.00", this);
  renderTimeValueLabel = new QLabel("0.000 ms", this);
  averageRenderTimeValueLabel = new QLabel("0.000 ms", this);

  layout->addRow("FPS", fpsValueLabel);
  layout->addRow("Average FPS", averageFpsValueLabel);
  layout->addRow("Render Time", renderTimeValueLabel);
  layout->addRow("Avg Render Time", averageRenderTimeValueLabel);
}

void RenderStatisctisWidget::setRenderStatistics(RenderStatistics *newStatistics)
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

void RenderStatisctisWidget::refresh()
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
