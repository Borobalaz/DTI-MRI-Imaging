#pragma once

#include <memory>

#include <QMainWindow>

#include "ui/controllers/MainWindowShortcuts.h"

class DTIViewportWidget;
class InspectorWidget;
class RenderStatisticsWidget;
class SceneObjectListWidget;

class WidgetsMainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit WidgetsMainWindow(QWidget *parent = nullptr);

private:
  void setupLayout();
  void wireAdapterSignals();
  void applyTheme();
  void toggleTheme();

  void refreshObjectList();
  void syncObjectSelection();

  bool useDarkTheme = true;
  std::unique_ptr<MainWindowShortcuts> shortcuts;

  DTIViewportWidget *viewportWidget = nullptr;
  InspectorWidget *inspectorWidget = nullptr;
  RenderStatisticsWidget *renderStatisticsWidget = nullptr;

  SceneObjectListWidget *sceneObjectListWidget = nullptr;
};
