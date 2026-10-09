#include "widgets/SceneObjectListWidget.h"

#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "styles/StyleSheetComposer.h"
#include "widgets/InspectProviderWidget.h"

SceneObjectListWidget::SceneObjectListWidget(QWidget *parent)
  : QFrame(parent)
{
  objectsLayout = new QVBoxLayout(this);
  refreshLayoutMetrics();

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

void SceneObjectListWidget::refreshLayoutMetrics()
{
  setMinimumWidth(ResolveCommonToken("object-list-min-width").toInt());

  const int margin = ResolveCommonToken("panel-margin").toInt();
  objectsLayout->setContentsMargins(margin, margin, margin, margin);
  objectsLayout->setSpacing(ResolveCommonToken("panel-spacing").toInt());
}

/**
 * @brief Set the list of scene objects to display in the widget, along with their visibility states.
 * 
 * @param objects 
 */
void SceneObjectListWidget::setObjects(std::vector<InspectObjectSummary> objects)
{
  clearRows();

  for (const InspectObjectSummary &object : objects)
  {
    auto *itemWidget = new InspectProviderWidget(listContainer);
    itemWidget->setObject(object);

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
