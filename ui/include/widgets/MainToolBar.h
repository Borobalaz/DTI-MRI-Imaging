#pragma once

#include <QList>
#include <QToolBar>

class QAction;
class QMenu;

class MainToolBar : public QToolBar
{
  Q_OBJECT

public:
  explicit MainToolBar(QWidget *parent = nullptr);

  // Inserts the given (already-checkable) panel visibility actions into the View menu, in order.
  void setViewActions(const QList<QAction *> &actions);

signals:
  void toggleThemeRequested();

private:
  QMenu *viewMenu = nullptr;
};
