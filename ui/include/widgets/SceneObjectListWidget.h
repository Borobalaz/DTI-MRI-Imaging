#pragma once

#include <vector>

#include <QStringList>

#include <QFrame>

#include "qt-adapters/InspectObjectSummary.h"

class QScrollArea;
class QVBoxLayout;
class InspectProviderWidget;

class SceneObjectListWidget : public QFrame
{
  Q_OBJECT

public:
  explicit SceneObjectListWidget(QWidget *parent = nullptr);

  void setObjects(std::vector<InspectObjectSummary> objects);
  void setCurrentProviderName(const std::string &providerName, bool emitSignal = false);

  // Re-reads layout metrics (margin/spacing/min-width) from tokens/common.ini and re-applies
  //  them, so editing that file updates an already-running instance (called from
  //  WidgetsMainWindow::applyTheme() whenever a style source file change is detected).
  void refreshLayoutMetrics();

signals:
  void currentRowChanged(std::string providerName);
  void visibilityIconClicked(std::string providerName);

private:
  void clearRows();
  void updateRowSelection();

  QVBoxLayout *objectsLayout = nullptr;
  QScrollArea *scrollArea = nullptr;
  QWidget *listContainer = nullptr;
  QVBoxLayout *listLayout = nullptr;
  std::vector<InspectProviderWidget *> rows;
  std::string currentSelectedProviderName;
};
