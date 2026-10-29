#pragma once

#include <functional>

#include <QFrame>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QVariant>

class QVBoxLayout;
class QWidget;
class IInspectWidget;
class InspectGroupSection;

class InspectorWidget : public QFrame
{
  Q_OBJECT

public:
  explicit InspectorWidget(QWidget *parent = nullptr);

  void setFields(const QObjectList &fieldObjects);
  void refreshBoundEditors();

  // Placeholder hint shown (as QLabel#inspectorEmptyLabel) whenever setFields() adds no editors;
  //  takes effect on the next setFields() call. An empty text shows nothing.
  void setEmptyText(const QString &text);

  // Namespaces this panel's persisted group collapse state in QSettings
  //  (inspectorGroups/<key>/<groupName>/collapsed), so each panel remembers its own groups
  //  across sessions. With an empty key the state is kept in memory only.
  void setSettingsKey(const QString &key);

  // Re-reads layout metrics (margin/spacing/min-width) from tokens/common.ini and re-applies
  //  them, so editing that file updates an already-running instance (called from
  //  WidgetsMainWindow::applyTheme() whenever a style source file change is detected).
  void refreshLayoutMetrics();

private:
  struct EditorBinding
  {
    QPointer<QObject> field;
    std::function<void(const QVariant &)> updateEditor; // Update the editor widget to reflect the given field value
  };

  void clearInspector();
  InspectGroupSection *addGroupSection(const QString &groupName);
  void addFieldEditor(IInspectWidget *field, QVBoxLayout *targetLayout);
  bool isGroupCollapsed(const QString &groupTitle);
  void setGroupCollapsed(const QString &groupTitle, bool collapsed);
  QString groupSettingsPath(const QString &groupTitle) const;

  QVBoxLayout *inspectorPanelLayout = nullptr;
  QWidget *inspectorContent = nullptr;
  QVBoxLayout *inspectorLayout = nullptr;
  QList<EditorBinding> editorBindings;
  QString emptyText;
  QString settingsKey;
  // Collapse state per group title. Lives on the widget (not the sections) so it survives the
  //  full setFields() rebuilds triggered by provider-generation bumps.
  QHash<QString, bool> collapsedGroups;
  bool isApplyingEditorState = false;
};
