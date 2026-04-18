#pragma once

#include <functional>

#include <QFrame>
#include <QPointer>
#include <QVariant>

class QVBoxLayout;
class QWidget;
class InspectField;

class InspectorWidget : public QFrame
{
  Q_OBJECT

public:
  explicit InspectorWidget(QWidget *parent = nullptr);

  void setFields(const QObjectList &fieldObjects);
  void refreshBoundEditors();

private:
  struct EditorBinding
  {
    QPointer<InspectField> field; 
    std::function<void(const QVariant &)> updateEditor; // Update the editor widget to reflect the given field value
  };

  void clearInspector();
  void addFieldEditor(InspectField *field);

  QWidget *inspectorContent = nullptr;
  QVBoxLayout *inspectorLayout = nullptr;
  QList<EditorBinding> editorBindings;
  bool isApplyingEditorState = false;
};
