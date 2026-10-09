#pragma once

#include <QFrame>

class QFormLayout;
class QLabel;
class RenderStatistics;

class RenderStatisticsWidget : public QFrame
{
  Q_OBJECT

public:
  explicit RenderStatisticsWidget(QWidget *parent = nullptr);

  void setRenderStatistics(RenderStatistics *statistics);

  // Re-reads layout metrics (margin/spacing/min-width) from tokens/common.ini and re-applies
  //  them, so editing that file updates an already-running instance (called from
  //  WidgetsMainWindow::applyTheme() whenever a style source file change is detected).
  void refreshLayoutMetrics();

private:
  void refresh();

  QFormLayout *formLayout = nullptr;
  RenderStatistics *statistics = nullptr;
  QLabel *fpsValueLabel = nullptr;
  QLabel *averageFpsValueLabel = nullptr;
  QLabel *renderTimeValueLabel = nullptr;
  QLabel *averageRenderTimeValueLabel = nullptr;
};
