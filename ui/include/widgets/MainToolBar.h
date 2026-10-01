#pragma once

#include <QToolBar>

class MainToolBar : public QToolBar
{
  Q_OBJECT

public:
  explicit MainToolBar(QWidget *parent = nullptr);

signals:
  void toggleThemeRequested();
  void objectListVisibilityToggled(bool visible);
  void statsVisibilityToggled(bool visible);
  void inspectorVisibilityToggled(bool visible);
};
