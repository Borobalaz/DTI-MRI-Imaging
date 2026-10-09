#include "widgets/InspectorWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <QObject>

#include "styles/StyleSheetComposer.h"
#include "widgets/inspect_fields/IInspectWidget.h"

InspectorWidget::InspectorWidget(QWidget *parent)
  : QFrame(parent)
{
  inspectorPanelLayout = new QVBoxLayout(this);
  refreshLayoutMetrics();

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setObjectName("inspectorScrollArea");
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  inspectorContent = new QWidget(scrollArea);
  inspectorContent->setObjectName("inspectorContent");
  inspectorLayout = new QVBoxLayout(inspectorContent);
  inspectorLayout->setContentsMargins(0, 0, 0, 0);
  inspectorLayout->addStretch(1);

  scrollArea->setWidget(inspectorContent);
  inspectorPanelLayout->addWidget(scrollArea, 1);
}

void InspectorWidget::refreshLayoutMetrics()
{
  setMinimumWidth(ResolveCommonToken("inspector-min-width").toInt());

  const int margin = ResolveCommonToken("panel-margin").toInt();
  inspectorPanelLayout->setContentsMargins(margin, margin, margin, margin);
  inspectorPanelLayout->setSpacing(ResolveCommonToken("panel-spacing").toInt());
}

void InspectorWidget::setFields(const QObjectList &fieldObjects)
{
  editorBindings.clear();
  clearInspector();

  QString currentGroup;
  for (QObject *fieldObject : fieldObjects)
  {
    auto *field = dynamic_cast<IInspectWidget *>(fieldObject);
    if (!field)
    {
      continue;
    }

    if (field->fieldId() == "visible" || field->fieldId() == "isVisible")
    {
      continue;
    }

    if (field->groupName() != currentGroup)
    {
      currentGroup = field->groupName();
      auto *groupLabel = new QLabel(currentGroup, inspectorContent);
      groupLabel->setObjectName("inspectorGroupLabel");
      // Color, font-weight and spacing all come from QLabel#inspectorGroupLabel in
      //  ui/styles/widgets/InspectorWidget.qss, so they stay themed and hot-reloadable.
      inspectorLayout->addWidget(groupLabel);
    }

    addFieldEditor(field);
  }

  inspectorLayout->addStretch(1);
  refreshBoundEditors();
}

void InspectorWidget::refreshBoundEditors()
{
  isApplyingEditorState = true;
  for (const EditorBinding &binding : editorBindings)
  {
    if (!binding.field || !binding.updateEditor)
    {
      continue;
    }

    auto *field = dynamic_cast<IInspectWidget *>(binding.field.data());
    if (field)
    {
      binding.updateEditor(field->GetValue());
    }
  }
  isApplyingEditorState = false;
}

void InspectorWidget::clearInspector()
{
  if (!inspectorLayout)
  {
    return;
  }

  while (QLayoutItem *item = inspectorLayout->takeAt(0))
  {
    if (QWidget *widget = item->widget())
    {
      widget->deleteLater();
    }
    delete item;
  }
}

void InspectorWidget::addFieldEditor(IInspectWidget *field)
{
  auto *row = new QWidget(inspectorContent);
  row->setObjectName("inspectorFieldRow");
  // Disabling the row (rather than just the editor) also dims the name label via the
  //  palette's disabled color group, matching read-only fields visually, not just functionally.
  row->setEnabled(!field->isReadOnly());

  auto *rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(ResolveCommonToken("field-row-margin-h").toInt(),
                                 ResolveCommonToken("field-row-margin-v").toInt(),
                                 ResolveCommonToken("field-row-margin-h").toInt(),
                                 ResolveCommonToken("field-row-margin-v").toInt());
  rowLayout->setSpacing(ResolveCommonToken("field-row-spacing").toInt());

  auto *nameLabel = new QLabel(field->displayName(), row);
  nameLabel->setObjectName("inspectorFieldLabel");
  const int labelWidth = ResolveCommonToken("field-label-width").toInt();
  nameLabel->setMinimumWidth(labelWidth);
  nameLabel->setMaximumWidth(labelWidth);
  nameLabel->setWordWrap(true);
  rowLayout->addWidget(nameLabel);

  IInspectWidget *editor = field->addToLayout(rowLayout);
  if (editor)
  {
    editorBindings.push_back({dynamic_cast<QObject *>(field), [editor](const QVariant &value)
    {
      editor->SetValue(value);
    }});
  }
  else
  {
    auto *unsupported = new QLabel("Unsupported field", row);
    unsupported->setWordWrap(true);
    rowLayout->addWidget(unsupported, 1);
  }

  inspectorLayout->addWidget(row);
}

