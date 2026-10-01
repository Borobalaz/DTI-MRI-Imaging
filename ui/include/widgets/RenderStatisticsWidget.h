#pragma once

#include <QFrame>

class QLabel;
class RenderStatistics;

class RenderStatisticsWidget : public QFrame
{
  Q_OBJECT

public:
  explicit RenderStatisticsWidget(QWidget *parent = nullptr);

  void setRenderStatistics(RenderStatistics *statistics);

private:
  void refresh();

  RenderStatistics *statistics = nullptr;
  QLabel *fpsValueLabel = nullptr;
  QLabel *averageFpsValueLabel = nullptr;
  QLabel *renderTimeValueLabel = nullptr;
  QLabel *averageRenderTimeValueLabel = nullptr;
};
