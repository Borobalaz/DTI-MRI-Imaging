#pragma once

#include <QWidget>

class QDockWidget;
class QLabel;
class QToolButton;

// Custom title bar installed via QDockWidget::setTitleBarWidget(). Qt's Fusion style ignores
// QSS font properties (and any QFont set on the QDockWidget itself) when natively painting
// QDockWidget::title via CE_DockWidgetTitle, so the title text can't be made bold/sized through
// the stylesheet. This widget replaces that native title bar with ordinary child widgets
// (QLabel/QToolButton), which paint through normal widget styling and are fully themeable.
class DockTitleBarWidget : public QWidget
{
  Q_OBJECT

public:
  explicit DockTitleBarWidget(QDockWidget *dockWidget, QWidget *parent = nullptr);

private:
  QDockWidget *dockWidget;
  QLabel *titleLabel;
  QToolButton *floatButton;
  QToolButton *closeButton;
};
