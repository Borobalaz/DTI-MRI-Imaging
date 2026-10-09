#pragma once

#include <functional>

#include <QFrame>
#include <QPointer>
#include <QVariant>

class QVBoxLayout;
class QWidget;
class IInspectWidget;

class InspectorWidget : public QFrame
{
  Q_OBJECT

public:
  explicit InspectorWidget(QWidget *parent = nullptr);

  void setFields(const QObjectList &fieldObjects);
  void refreshBoundEditors();

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
  void addFieldEditor(IInspectWidget *field);

  QVBoxLayout *inspectorPanelLayout = nullptr;
  QWidget *inspectorContent = nullptr;
  QVBoxLayout *inspectorLayout = nullptr;
  QList<EditorBinding> editorBindings;
  bool isApplyingEditorState = false;
};
