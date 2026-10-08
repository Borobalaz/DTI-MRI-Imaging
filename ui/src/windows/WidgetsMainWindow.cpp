#include "windows/WidgetsMainWindow.h"

#include <QCloseEvent>
#include <QDockWidget>
#include <QFrame>
#include <QGuiApplication>
#include <QIcon>
#include <QScreen>
#include <QSettings>
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

  // Restore the user's last dock/geometry arrangement, if any
  restoreLayoutState();
}

void WidgetsMainWindow::closeEvent(QCloseEvent *event)
{
  saveLayoutState();
  QMainWindow::closeEvent(event);
}

void WidgetsMainWindow::saveLayoutState()
{
  QSettings settings;
  settings.setValue("mainWindow/geometry", saveGeometry());
  settings.setValue("mainWindow/state", saveState());
}

void WidgetsMainWindow::restoreLayoutState()
{
  const QSettings settings;

  const QVariant geometry = settings.value("mainWindow/geometry");
  if (geometry.isValid())
  {
    restoreGeometry(geometry.toByteArray());
  }

  // Guard against a stale/corrupt saved geometry leaving the window minimized, zero-sized,
  //  or positioned off every connected screen (invisible, with no on-screen way to recover it).
  bool onScreen = false;
  for (const QScreen *screen : QGuiApplication::screens())
  {
    if (screen->availableGeometry().intersects(frameGeometry()))
    {
      onScreen = true;
      break;
    }
  }

  if (isMinimized() || frameGeometry().width() < 200 || frameGeometry().height() < 150 || !onScreen)
  {
    setWindowState(windowState() & ~Qt::WindowMinimized);
    resize(1600, 900);
    if (const QScreen *primaryScreen = QGuiApplication::primaryScreen())
    {
      move(primaryScreen->availableGeometry().center() - rect().center());
    }
  }

  const QVariant state = settings.value("mainWindow/state");
  if (state.isValid())
  {
    restoreState(state.toByteArray());
  }
}

/**
 * @brief Build the main window ui layout.
 *  The viewport is the central widget; the scene object list, render statistics,
 *  and inspector panels are dockable QDockWidgets that can be freely moved, resized,
 *  floated, or tabbed by the user.
 *
 */
void WidgetsMainWindow::setupLayout()
{
  setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);

  // Scene viewport (central widget)
  auto *viewportPanel = new QFrame(this);
  viewportPanel->setObjectName("viewportPanel");
  auto *viewportLayout = new QVBoxLayout(viewportPanel);
  viewportLayout->setContentsMargins(1, 1, 1, 1);

  viewportWidget = new OpenGLViewportWidget(viewportPanel);
  viewportLayout->addWidget(viewportWidget, 1);

  setCentralWidget(viewportPanel);

  // Left dock: scene object list
  sceneObjectListWidget = new SceneObjectListWidget(this);
  objectsDock = new QDockWidget(tr("Objects"), this);
  objectsDock->setObjectName("objectsDock");
  objectsDock->setWidget(sceneObjectListWidget);

  // Left dock (stacked below objects): render statistics
  renderStatisticsWidget = new RenderStatisticsWidget(this);
  statsDock = new QDockWidget(tr("Render Statistics"), this);
  statsDock->setObjectName("renderStatsDock");
  statsDock->setWidget(renderStatisticsWidget);

  // Right dock: inspector
  inspectorWidget = new InspectorWidget(this);
  inspectorDock = new QDockWidget(tr("Inspector"), this);
  inspectorDock->setObjectName("inspectorDock");
  inspectorDock->setWidget(inspectorWidget);

  renderStatisticsWidget->setRenderStatistics(viewportWidget->renderStatistics());

  // Assemble the default dock arrangement
  addDockWidget(Qt::LeftDockWidgetArea, objectsDock);
  splitDockWidget(objectsDock, statsDock, Qt::Vertical);
  addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
}

void WidgetsMainWindow::setupToolBar()
{
  toolBar = new MainToolBar(this);

  QObject::connect(toolBar, &MainToolBar::toggleThemeRequested, this, [this]()
  {
    toggleTheme();
  });
  toolBar->setViewActions({objectsDock->toggleViewAction(), statsDock->toggleViewAction(), inspectorDock->toggleViewAction()});

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
