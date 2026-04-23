#include "ui/widgets/SceneObjectListWidget.h"

#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ui/widgets/InspectProviderWidget.h"

SceneObjectListWidget::SceneObjectListWidget(QWidget *parent)
  : QFrame(parent)
{
  setObjectName("objectsPanel");
  setMinimumWidth(280);
  setMaximumWidth(280);

  auto *objectsLayout = new QVBoxLayout(this);
  objectsLayout->setContentsMargins(12, 12, 12, 12);
  objectsLayout->setSpacing(8);

  auto *objectsTitle = new QLabel("Objects", this);
  objectsTitle->setObjectName("panelTitle");
  objectsLayout->addWidget(objectsTitle);

  scrollArea = new QScrollArea(this);
  scrollArea->setObjectName("objectsScrollArea");
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  listContainer = new QWidget(scrollArea);
  listLayout = new QVBoxLayout(listContainer);
  listLayout->setContentsMargins(0, 0, 0, 0);
  listLayout->setSpacing(4);
  listLayout->addStretch(1);

  scrollArea->setWidget(listContainer);
  objectsLayout->addWidget(scrollArea, 1);
}

/**
 * @brief Set the list of scene objects to display in the widget, along with their visibility states.
 * 
 * @param providers 
 */
void SceneObjectListWidget::setObjects(std::vector<InspectProvider*> providers)
{
  clearRows();

  for(auto provider : providers)
  {
    auto *itemWidget = new InspectProviderWidget(listContainer);
    itemWidget->setProvider(provider);

    QObject::connect(itemWidget, &InspectProviderWidget::clicked, this, [this](std::string providerName)
    {
      currentSelectedProviderName = providerName;
      updateRowSelection();
      emit currentRowChanged(providerName);
    });

    QObject::connect(itemWidget, &InspectProviderWidget::visibilityClicked, this, [this](std::string providerName)
    {
      emit visibilityIconClicked(providerName);
    });

    rows.push_back(itemWidget);
    listLayout->insertWidget(listLayout->count() - 1, itemWidget);
  }
  updateRowSelection();
}

void SceneObjectListWidget::setCurrentProviderName(const std::string &providerName, bool emitSignal)
{
  if (providerName == currentSelectedProviderName)
  {
    return;
  }

  currentSelectedProviderName = providerName;
  updateRowSelection();

  if (emitSignal)
  {
    emit currentRowChanged(providerName);
  }
}

/**
 * @brief Clear the list of InspectProviderWidgets. 
 * 
 */
void SceneObjectListWidget::clearRows()
{
  for (InspectProviderWidget *row : rows)
  {
    if (row)
    {
      row->deleteLater();
    }
  }
  rows.clear();
}

/**
 * @brief If a provider is selected, update the corresponding row widget to show the selection state. 
 *  Otherwise, clear selection from all rows.
 * 
 */
void SceneObjectListWidget::updateRowSelection()
{
  for (size_t i = 0; i < rows.size(); ++i)
  {
    if (!rows[i])
    {
      continue;
    }

    rows[i]->setSelected(currentSelectedProviderName == rows[i]->getName());
  }
}
