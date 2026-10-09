#pragma once

#include <memory>

#include <QDateTime>
#include <QMainWindow>
#include <QString>

#include "controllers/MainWindowShortcuts.h"

class QDockWidget;
class QCloseEvent;
class QTimer;

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
  void recordActiveStyleModTime();
  void pollActiveStyleForChanges();
  QString currentThemeName() const;

  void saveLayoutState();
  void restoreLayoutState();

  void refreshObjectList();
  void syncObjectSelection();

  bool useDarkTheme = true;
  std::unique_ptr<MainWindowShortcuts> shortcuts;
  QTimer *styleReloadTimer = nullptr;
  QDateTime lastStyleModTime;

  OpenGLViewportWidget *viewportWidget = nullptr;
  InspectorWidget *inspectorWidget = nullptr;
  RenderStatisticsWidget *renderStatisticsWidget = nullptr;

  SceneObjectListWidget *sceneObjectListWidget = nullptr;

  QDockWidget *objectsDock = nullptr;
  QDockWidget *statsDock = nullptr;
  QDockWidget *inspectorDock = nullptr;

  MainToolBar *toolBar = nullptr;
};
