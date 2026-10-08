#include "widgets/MainToolBar.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QToolButton>

MainToolBar::MainToolBar(QWidget *parent)
  : QToolBar(tr("Main Toolbar"), parent)
{
  setObjectName("mainToolBar");
  setMovable(false);
  setIconSize(QSize(20, 20));

  // File dropdown
  auto *fileMenu = new QMenu(tr("File"), this);
  fileMenu->addAction(tr("New Scene")); // dummy: no behavior wired up yet

  auto *fileButton = new QToolButton(this);
  fileButton->setText(tr("File"));
  fileButton->setMenu(fileMenu);
  fileButton->setPopupMode(QToolButton::InstantPopup);
  addWidget(fileButton);

  // View dropdown
  viewMenu = new QMenu(tr("View"), this);

  auto *viewButton = new QToolButton(this);
  viewButton->setText(tr("View"));
  viewButton->setMenu(viewMenu);
  viewButton->setPopupMode(QToolButton::InstantPopup);
  addWidget(viewButton);

  addSeparator();

  auto *toggleThemeAction = addAction(tr("Toggle Theme"));
  toggleThemeAction->setToolTip(tr("Switch between dark and light theme (Ctrl+Shift+T)"));
  QObject::connect(toggleThemeAction, &QAction::triggered, this, &MainToolBar::toggleThemeRequested);
}

void MainToolBar::setViewActions(const QList<QAction *> &actions)
{
  for (QAction *action : actions)
  {
    viewMenu->addAction(action);
  }
}
