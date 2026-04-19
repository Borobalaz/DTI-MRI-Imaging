#pragma once

#include <QFrame>

class QLabel;
class RenderStatistics;

class RenderStatisctisWidget : public QFrame
{
  Q_OBJECT

public:
  explicit RenderStatisctisWidget(QWidget *parent = nullptr);

  void setRenderStatistics(RenderStatistics *statistics);

private:
  void refresh();

  RenderStatistics *statistics = nullptr;
  QLabel *fpsValueLabel = nullptr;
  QLabel *averageFpsValueLabel = nullptr;
  QLabel *renderTimeValueLabel = nullptr;
  QLabel *averageRenderTimeValueLabel = nullptr;
};
