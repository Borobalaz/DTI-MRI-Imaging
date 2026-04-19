#include "ui/widgets/InspectorWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <QObject>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

InspectorWidget::InspectorWidget(QWidget *parent)
  : QFrame(parent)
{
  setObjectName("inspectorPanel");
  setMinimumWidth(320);
  setMaximumWidth(320);

  auto *inspectorPanelLayout = new QVBoxLayout(this);
  inspectorPanelLayout->setContentsMargins(12, 12, 12, 12);
  inspectorPanelLayout->setSpacing(8);

  auto *inspectorTitle = new QLabel("Inspector", this);
  inspectorTitle->setObjectName("panelTitle");
  inspectorPanelLayout->addWidget(inspectorTitle);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setObjectName("inspectorScrollArea");
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  inspectorContent = new QWidget(scrollArea);
  inspectorContent->setObjectName("inspectorContent");
  inspectorLayout = new QVBoxLayout(inspectorContent);
  inspectorLayout->setContentsMargins(0, 0, 0, 0);
  inspectorLayout->setSpacing(10);
  inspectorLayout->addStretch(1);

  scrollArea->setWidget(inspectorContent);
  inspectorPanelLayout->addWidget(scrollArea, 1);
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

    if (field->groupName() != currentGroup)
    {
      currentGroup = field->groupName();
      auto *groupLabel = new QLabel(currentGroup, inspectorContent);
      QFont groupFont = groupLabel->font();
      groupFont.setBold(true);
      groupLabel->setFont(groupFont);
      groupLabel->setStyleSheet("color: #9aa6b2; margin-top: 8px;");
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
  auto *rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(8);

  auto *nameLabel = new QLabel(field->displayName(), row);
  nameLabel->setMinimumWidth(96);
  nameLabel->setMaximumWidth(96);
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

