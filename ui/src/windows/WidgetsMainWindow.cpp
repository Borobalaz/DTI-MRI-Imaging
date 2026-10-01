#include "windows/WidgetsMainWindow.h"

#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#include <dwmapi.h>
#include <windows.h>
#endif

#include "qt-adapters/QTSceneInspector.h"
#include "controllers/MainWindowShortcuts.h"
#include "styles/DarkThemeStyle.h"
#include "styles/LightThemeStyle.h"
#include "widgets/OpenGLViewportWidget.h"
#include "widgets/InspectorWidget.h"
#include "widgets/MainToolBar.h"
#include "widgets/RenderStatisticsWidget.h"
#include "widgets/SceneObjectListWidget.h"

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

  // Build the toolbar
  setupToolBar();

  // Wire signals between the QTSceneInspector, the scene object list 
  //  and inspector widgets, to synchronize state between them
  wireAdapterSignals();

  // Apply the initial theme
  applyTheme();

  // Set initial window size, title and icon
  resize(1600, 900);
  setWindowTitle("3D Engine");
  setWindowIcon(QIcon(":/icons/app-icon.svg"));
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

  viewportWidget = new OpenGLViewportWidget(viewportPanel);
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

void WidgetsMainWindow::setupToolBar()
{
  toolBar = new MainToolBar(this);

  QObject::connect(toolBar, &MainToolBar::toggleThemeRequested, this, [this]()
  {
    toggleTheme();
  });
  QObject::connect(toolBar, &MainToolBar::objectListVisibilityToggled, sceneObjectListWidget, &QWidget::setVisible);
  QObject::connect(toolBar, &MainToolBar::statsVisibilityToggled, renderStatisticsWidget, &QWidget::setVisible);
  QObject::connect(toolBar, &MainToolBar::inspectorVisibilityToggled, inspectorWidget, &QWidget::setVisible);

  addToolBar(Qt::TopToolBarArea, toolBar);
}

void WidgetsMainWindow::applyTheme()
{
  viewportWidget->SetFillColor(useDarkTheme ? glm::vec3(0.0f) : glm::vec3(1.0f));

  if (useDarkTheme)
  {
    const DarkThemeStyle darkThemeStyle;
    setStyleSheet(darkThemeStyle.styleSheet());
  }
  else
  {
    const LightThemeStyle lightThemeStyle;
    setStyleSheet(lightThemeStyle.styleSheet());
  }

  applyTitleBarTheme();
}

void WidgetsMainWindow::applyTitleBarTheme()
{
#if defined(Q_OS_WIN)
  // Color the native titlebar to match the theme (Windows 11 22H2+, DWMWA_CAPTION_COLOR/DWMWA_TEXT_COLOR).
  constexpr DWORD DwmwaUseImmersiveDarkMode = 20;
  constexpr DWORD DwmwaCaptionColor = 35;
  constexpr DWORD DwmwaTextColor = 36;

  const HWND hwnd = reinterpret_cast<HWND>(winId());

  const BOOL useDarkMode = useDarkTheme ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, DwmwaUseImmersiveDarkMode, &useDarkMode, sizeof(useDarkMode));

  const COLORREF captionColor = useDarkTheme ? RGB(0x1b, 0x26, 0x35) : RGB(0xee, 0xf3, 0xf8);
  const COLORREF textColor = useDarkTheme ? RGB(0xd8, 0xe1, 0xea) : RGB(0x2a, 0x3b, 0x4f);
  DwmSetWindowAttribute(hwnd, DwmwaCaptionColor, &captionColor, sizeof(captionColor));
  DwmSetWindowAttribute(hwnd, DwmwaTextColor, &textColor, sizeof(textColor));
#endif
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
