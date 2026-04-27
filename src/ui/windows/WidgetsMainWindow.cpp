#include "ui/windows/WidgetsMainWindow.h"

#include <QFrame>
#include <QGridLayout>
#include <QVBoxLayout>

#include "ui/qt-adapters/QTSceneInspector.h"
#include "ui/controllers/MainWindowShortcuts.h"
#include "ui/styles/DarkThemeStyle.h"
#include "ui/styles/LightThemeStyle.h"
#include "ui/widgets/DTIViewportWidget.h"
#include "ui/widgets/InspectorWidget.h"
#include "ui/widgets/RenderStatisticsWidget.h"
#include "ui/widgets/SceneObjectListWidget.h"

/**
 * @brief Construct a new Widgets Main Window:: Widgets Main Window object
 * 
 * @param parent 
 */
WidgetsMainWindow::WidgetsMainWindow(QWidget *parent)
  : QMainWindow(parent)
{
  // Set up keyboard shortcuts
  shortcuts = std::make_unique<MainWindowShortcuts>(this);
  QObject::connect(shortcuts.get(), &MainWindowShortcuts::toggleThemeRequested, this, [this]()
  {
    toggleTheme();
  });

  // Build the main window layout
  setupLayout();

  //viewportWidget->setDwiPath("assets/volumes/dwi/HARDI150_hdbet_masked4d.nii.gz");
  //viewportWidget->setBvalPath("assets/volumes/dwi/HARDI150.bval");
  //viewportWidget->setBvecPath("assets/volumes/dwi/HARDI150.bvec");

  viewportWidget->setDwiPath("assets/volumes/dwi/macaque/sub-01_run-01_dwi_hdbet_masked4d.nii.gz");
  viewportWidget->setBvalPath("assets/volumes/dwi/macaque/sub-01_run-01_dwi.bval");
  viewportWidget->setBvecPath("assets/volumes/dwi/macaque/sub-01_run-01_dwi.bvec");


  // Wire signals between the QTSceneInspector, the scene object list 
  //  and inspector widgets, to synchronize state between them
  wireAdapterSignals();

  // Apply the initial theme
  applyTheme();

  // Set initial window size and title
  resize(1600, 900);
  setWindowTitle("DTI Visualizer (Widgets)");
}

/**
 * @brief Build the main window ui layout. 
 *  This is a GridLayout with 3 columns and 2 rows. 
 *  The left column has the scene object list and render statistics widgets, 
 *  the middle column has the viewport, and the right column has the inspector.
 * 
 */
void WidgetsMainWindow::setupLayout()
{
  // Root widget
  auto *root = new QWidget(this);
  auto *rootLayout = new QGridLayout(root);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(12);

  // Left panel: scene object list
  sceneObjectListWidget = new SceneObjectListWidget(root);

  // Left-bottom panel: render statistics
  renderStatisticsWidget = new RenderStatisticsWidget(root);

  // Scene viewport
  auto *viewportPanel = new QFrame(root);
  viewportPanel->setObjectName("viewportPanel");
  auto *viewportLayout = new QVBoxLayout(viewportPanel);
  viewportLayout->setContentsMargins(1, 1, 1, 1);

  viewportWidget = new DTIViewportWidget(viewportPanel);
  viewportLayout->addWidget(viewportWidget, 1);

  // Right panel: inspector
  inspectorWidget = new InspectorWidget(root);

  renderStatisticsWidget->setRenderStatistics(viewportWidget->renderStatistics());

  // Assemble root layout
  rootLayout->addWidget(sceneObjectListWidget, 0, 0);
  rootLayout->addWidget(renderStatisticsWidget, 1, 0);
  rootLayout->addWidget(viewportPanel, 0, 1, 2, 1);
  rootLayout->addWidget(inspectorWidget, 0, 2, 2, 1);
  rootLayout->setRowStretch(0, 1);
  rootLayout->setRowStretch(1, 0);
  rootLayout->setColumnStretch(0, 0);
  rootLayout->setColumnStretch(1, 1);
  rootLayout->setColumnStretch(2, 0);

  setCentralWidget(root);

}

void WidgetsMainWindow::applyTheme()
{
  if (useDarkTheme)
  {
    const DarkThemeStyle darkThemeStyle;
    setStyleSheet(darkThemeStyle.styleSheet());
    return;
  }

  const LightThemeStyle lightThemeStyle;
  setStyleSheet(lightThemeStyle.styleSheet());
}

void WidgetsMainWindow::toggleTheme()
{
  useDarkTheme = !useDarkTheme;
  applyTheme();
}

/**
 * @brief Wire signals between the QTSceneInspector and the scene object list and inspector widgets, 
 *  to synchronize state between them.
 * 
 */
void WidgetsMainWindow::wireAdapterSignals()
{
  QTSceneInspector *const adapter = &viewportWidget->inspectAdapter();

  // When the selected object changes in the scene object list, update the adapter selection.
  QObject::connect(sceneObjectListWidget, &SceneObjectListWidget::currentRowChanged, this, [this](const std::string &providerName)
  {
    viewportWidget->inspectAdapter().setSelectedObjectName(providerName);
  });

  QObject::connect(sceneObjectListWidget, &SceneObjectListWidget::visibilityIconClicked, this, [this](const std::string &providerName)
  {
    QTSceneInspector &adapter = viewportWidget->inspectAdapter();
    if (!adapter.hasVisibility(providerName))
    {
      return;
    }

    const bool currentVisibility = adapter.isVisible(providerName);
    if (adapter.setVisible(providerName, !currentVisibility))
    {
      refreshObjectList();
      syncObjectSelection();
    }
  });

  // When the adapter's object names change (e.g. from scene updates), refresh the scene object list
  QObject::connect(adapter, &QTSceneInspector::providersChanged, this, [this]()
  {
    refreshObjectList();
  });

  QObject::connect(adapter, &QTSceneInspector::visibilityStateChanged, this, [this]()
  {
    refreshObjectList();
    syncObjectSelection();
  });

  // When the adapter's selected index changes (e.g. from viewport interaction), update the scene object list selection
  QObject::connect(adapter, &QTSceneInspector::selectedProviderIndexChanged, this, [this]()
  {
    syncObjectSelection();
  });

  // When the adapter's fields change (e.g. from scene updates), update the inspector
  QObject::connect(adapter, &QTSceneInspector::fieldsChanged, this, [this]()
  {
    inspectorWidget->setFields(viewportWidget->inspectAdapter().fields());
  });

  // When field values change (e.g. from viewport interaction), refresh the inspector editors
  QObject::connect(adapter, &QTSceneInspector::fieldRevisionChanged, this, [this]()
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
  QTSceneInspector &adapter = viewportWidget->inspectAdapter();
  sceneObjectListWidget->setObjects(adapter.getProviders());
  sceneObjectListWidget->setCurrentProviderName(adapter.selectedObjectName(), false);
}

/**
 * @brief Synchronize the object selection between the adapter and the scene object list.
 */
void WidgetsMainWindow::syncObjectSelection()
{
  sceneObjectListWidget->setCurrentProviderName(viewportWidget->inspectAdapter().selectedObjectName(), false);
}
