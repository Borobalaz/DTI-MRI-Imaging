#include "ui/widgets/WidgetsMainWindow.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ui/qt-adapters/InspectQtAdapter.h"
#include "ui/widgets/DTIViewportWidget.h"
#include "ui/widgets/InspectorWidget.h"
#include "ui/widgets/SceneObjectListWidget.h"

/**
 * @brief Construct a new Widgets Main Window:: Widgets Main Window object
 * 
 * @param parent 
 */
WidgetsMainWindow::WidgetsMainWindow(QWidget *parent)
  : QMainWindow(parent)
{
  setupLayout();

  viewportWidget->setDwiPath("assets/volumes/dwi/HARDI150_hdbet_masked4d.nii.gz");
  viewportWidget->setBvalPath("assets/volumes/dwi/HARDI150.bval");
  viewportWidget->setBvecPath("assets/volumes/dwi/HARDI150.bvec");

  wireAdapterSignals();

  resize(1600, 900);
  setWindowTitle("DTI Visualizer (Widgets)");
}

/**
 * @brief Build the main window ui layout.
 * 
 */
void WidgetsMainWindow::setupLayout()
{
  // Root widget
  auto *root = new QWidget(this);
  auto *rootLayout = new QHBoxLayout(root);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(12);

  // Left panel: scene object list
  sceneObjectListWidget = new SceneObjectListWidget(root);

  // Scene viewport
  auto *viewportPanel = new QFrame(root);
  viewportPanel->setObjectName("viewportPanel");
  auto *viewportLayout = new QVBoxLayout(viewportPanel);
  viewportLayout->setContentsMargins(1, 1, 1, 1);

  viewportWidget = new DTIViewportWidget(viewportPanel);
  viewportLayout->addWidget(viewportWidget, 1);

  // Right panel: inspector
  inspectorWidget = new InspectorWidget(root);

  // Assemble root layout
  rootLayout->addWidget(sceneObjectListWidget);
  rootLayout->addWidget(viewportPanel, 1);
  rootLayout->addWidget(inspectorWidget);

  setCentralWidget(root);

  // Apply stylesheet for a dark theme
  setStyleSheet(
      "QMainWindow { background-color: #1b2635; }"
      "QFrame#objectsPanel, QFrame#inspectorPanel, QFrame#viewportPanel {"
      "  background-color: #131925;"
      "  border: 1px solid #243042;"
      "  border-radius: 12px;"
      "}"
      "QLabel#panelTitle { color: #9aa6b2; font-weight: 700; font-size: 18px; }"
      "QScrollArea#inspectorScrollArea { background-color: #131925; border: none; }"
      "QScrollArea#inspectorScrollArea > QWidget > QWidget { background-color: #131925; }"
      "QWidget#inspectorContent { background-color: #131925; }"
      "QListWidget#objectList { background: transparent; color: #d8e1ea; border: none; }"
      "QListWidget#objectList::item { padding: 8px; border-radius: 6px; }"
      "QListWidget#objectList::item:selected { background: #243042; }"
      "QLabel { color: #d8e1ea; }"
      "QLineEdit, QDoubleSpinBox, QComboBox {"
      "  background: #0f1723; color: #d8e1ea; border: 1px solid #243042; border-radius: 6px;"
      "}"
      "QCheckBox { color: #d8e1ea; }");
}

/**
 * @brief Wire signals between the InspectQtAdapter and the scene object list and inspector widgets, to synchronize state between them.
 * 
 */
void WidgetsMainWindow::wireAdapterSignals()
{
  InspectQtAdapter *const adapter = &viewportWidget->inspectAdapter();

  // When the selected object changes in the scene object list, update the adapter's selected index
  QObject::connect(sceneObjectListWidget, &SceneObjectListWidget::currentRowChanged, this, [this](int row)
  {
    viewportWidget->inspectAdapter().setSelectedObjectIndex(row);
  });

  // When the adapter's object names change (e.g. from scene updates), refresh the scene object list
  QObject::connect(adapter, &InspectQtAdapter::objectNamesChanged, this, [this]()
  {
    refreshObjectList();
  });

  // When the adapter's selected index changes (e.g. from viewport interaction), update the scene object list selection
  QObject::connect(adapter, &InspectQtAdapter::selectedObjectIndexChanged, this, [this]()
  {
    syncObjectSelection();
  });

  // When the adapter's fields change (e.g. from scene updates), update the inspector
  QObject::connect(adapter, &InspectQtAdapter::fieldsChanged, this, [this]()
  {
    inspectorWidget->setFields(viewportWidget->inspectAdapter().fields());
  });

  // When field values change (e.g. from viewport interaction), refresh the inspector editors
  QObject::connect(adapter, &InspectQtAdapter::fieldRevisionChanged, this, [this]()
  {
    inspectorWidget->refreshBoundEditors();
  });

  // Initial synchronization
  refreshObjectList();
  syncObjectSelection();
  inspectorWidget->setFields(viewportWidget->inspectAdapter().fields());
}

/**
 * @brief Refresh the scene object list with the latest object names from the adapter.
 */
void WidgetsMainWindow::refreshObjectList()
{
  const QStringList names = viewportWidget->inspectAdapter().objectNames();
  sceneObjectListWidget->setObjectNames(names);
}

/**
 * @brief Synchronize the object selection between the adapter and the scene object list.
 */
void WidgetsMainWindow::syncObjectSelection()
{
  const int index = viewportWidget->inspectAdapter().selectedObjectIndex();
  sceneObjectListWidget->setCurrentIndex(index, false);
}
