#pragma once
#include <QDialog>
class InspectorWidget;

// Non-modal floating window (not a dock) showing flat, non-selection-based engine-wide settings
//  (ISettingsService), e.g. "Enable Postprocessing". Owned as a singleton by WidgetsMainWindow,
//  created once and shown/raised on each MainToolBar::engineSettingsRequested() rather than recreated;
//  closing it (native X) just hides it (WA_DeleteOnClose stays false).
class EngineSettingsDialog : public QDialog
{
  Q_OBJECT
public:
  explicit EngineSettingsDialog(QWidget *parent = nullptr);
  InspectorWidget *settingsWidget() const { return inspector; }
  void refreshLayoutMetrics(); // forwards to the embedded InspectorWidget; call from WidgetsMainWindow::applyTheme()

private:
  InspectorWidget *inspector = nullptr;
};
