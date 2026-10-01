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

signals:
  void currentRowChanged(std::string providerName);
  void visibilityIconClicked(std::string providerName);

private:
  void clearRows();
  void updateRowSelection();

  QScrollArea *scrollArea = nullptr;
  QWidget *listContainer = nullptr;
  QVBoxLayout *listLayout = nullptr;
  std::vector<InspectProviderWidget *> rows;
  std::string currentSelectedProviderName;
};
