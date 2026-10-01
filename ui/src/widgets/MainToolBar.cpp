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
  auto *viewMenu = new QMenu(tr("View"), this);

  auto *toggleObjectListAction = viewMenu->addAction(tr("Objects"));
  toggleObjectListAction->setCheckable(true);
  toggleObjectListAction->setChecked(true);
  QObject::connect(toggleObjectListAction, &QAction::toggled, this, &MainToolBar::objectListVisibilityToggled);

  auto *toggleStatsAction = viewMenu->addAction(tr("Stats"));
  toggleStatsAction->setCheckable(true);
  toggleStatsAction->setChecked(true);
  QObject::connect(toggleStatsAction, &QAction::toggled, this, &MainToolBar::statsVisibilityToggled);

  auto *toggleInspectorAction = viewMenu->addAction(tr("Inspector"));
  toggleInspectorAction->setCheckable(true);
  toggleInspectorAction->setChecked(true);
  QObject::connect(toggleInspectorAction, &QAction::toggled, this, &MainToolBar::inspectorVisibilityToggled);

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
