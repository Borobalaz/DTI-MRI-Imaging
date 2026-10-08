#pragma once

#include <memory>

#include <QMainWindow>

#include "controllers/MainWindowShortcuts.h"

class QDockWidget;
class QCloseEvent;

class OpenGLViewportWidget;
class InspectorWidget;
class RenderStatisticsWidget;
class SceneObjectListWidget;
class MainToolBar;

class WidgetsMainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit WidgetsMainWindow(QWidget *parent = nullptr);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void setupLayout();
  void setupToolBar();
  void wireAdapterSignals();
  void applyTheme();
  void toggleTheme();
  void applyTitleBarTheme();

  void saveLayoutState();
  void restoreLayoutState();

  void refreshObjectList();
  void syncObjectSelection();

  bool useDarkTheme = true;
  std::unique_ptr<MainWindowShortcuts> shortcuts;

  OpenGLViewportWidget *viewportWidget = nullptr;
  InspectorWidget *inspectorWidget = nullptr;
  RenderStatisticsWidget *renderStatisticsWidget = nullptr;

  SceneObjectListWidget *sceneObjectListWidget = nullptr;

  QDockWidget *objectsDock = nullptr;
  QDockWidget *statsDock = nullptr;
  QDockWidget *inspectorDock = nullptr;

  MainToolBar *toolBar = nullptr;
};
