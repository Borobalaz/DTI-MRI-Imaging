#include "ui/widgets/InspectorWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "ui/mediator/InspectField.h"

namespace
{
  QVariantList ToVec3List(const QVariant &value)
  {
    QVariantList list = value.toList();
    if (list.size() < 3)
    {
      list = {0.0, 0.0, 0.0};
    }
    return list;
  }
}

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

/**
 * @brief Add fields to the layout and set up editor bindings for the given field objects.
 * 
 * @param fieldObjects 
 */
void InspectorWidget::setFields(const QObjectList &fieldObjects)
{
  editorBindings.clear();
  clearInspector();

  QString currentGroup;
  for (QObject *fieldObject : fieldObjects)
  {
    auto *field = qobject_cast<InspectField *>(fieldObject);
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

    binding.updateEditor(binding.field->value());
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

/**
 * @brief Put one field onto the inspector, 
 *  creating an appropriate editor widget based on the field's editorUrl 
 *  and setting up the binding to update the field when the editor value changes and vice versa.
 * 
 * @param field 
 */
void InspectorWidget::addFieldEditor(InspectField *field)
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

  const QString editorUrl = field->editorUrl().toString();

  if (editorUrl.contains("InspectCheckboxField.qml"))
  {
    auto *editor = new QCheckBox(row);
    editor->setEnabled(!field->isReadOnly());
    editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(editor, 1);

    QObject::connect(editor, &QCheckBox::toggled, this, [this, field](bool checked)
    {
      if (!isApplyingEditorState)
      {
        field->setValue(checked);
      }
    });

    editorBindings.push_back({field, [editor](const QVariant &value)
    {
      editor->setChecked(value.toBool());
    }});
  }
  else if (editorUrl.contains("InspectNumberField.qml"))
  {
    auto *editor = new QDoubleSpinBox(row);
    editor->setDecimals(4);
    editor->setSingleStep(0.01);
    editor->setEnabled(!field->isReadOnly());
    editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
    editor->setMinimumWidth(0);
    editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    const double minimum = field->minimum();
    const double maximum = field->maximum();
    if (minimum < maximum)
    {
      editor->setRange(minimum, maximum);
    }
    else
    {
      editor->setRange(-1e9, 1e9);
    }

    rowLayout->addWidget(editor, 1);

    QObject::connect(editor, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, field](double value)
    {
      if (!isApplyingEditorState)
      {
        field->setValue(value);
      }
    });

    editorBindings.push_back({field, [editor](const QVariant &value)
    {
      editor->setValue(value.toDouble());
    }});
  }
  else if (editorUrl.contains("InspectDropdownField.qml"))
  {
    auto *editor = new QComboBox(row);
    editor->setEnabled(!field->isReadOnly());
    editor->addItems(field->enumOptions());
    editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(editor, 1);

    QObject::connect(editor, &QComboBox::currentTextChanged, this, [this, field](const QString &text)
    {
      if (!isApplyingEditorState)
      {
        field->setValue(text);
      }
    });

    editorBindings.push_back({field, [editor](const QVariant &value)
    {
      const QString current = value.toString();
      const int index = editor->findText(current);
      if (index >= 0)
      {
        editor->setCurrentIndex(index);
      }
    }});
  }
  else if (editorUrl.contains("InspectTextField.qml"))
  {
    auto *editor = new QLineEdit(row);
    editor->setEnabled(!field->isReadOnly());
    editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rowLayout->addWidget(editor, 1);

    QObject::connect(editor, &QLineEdit::editingFinished, this, [this, field, editor]()
    {
      if (!isApplyingEditorState)
      {
        field->setValue(editor->text());
      }
    });

    editorBindings.push_back({field, [editor](const QVariant &value)
    {
      editor->setText(value.toString());
    }});
  }
  else if (editorUrl.contains("InspectVec3Field.qml") || editorUrl.contains("InspectColorField.qml"))
  {
    auto *editorContainer = new QWidget(row);
    editorContainer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(2);

    auto *xSpin = new QDoubleSpinBox(editorContainer);
    auto *ySpin = new QDoubleSpinBox(editorContainer);
    auto *zSpin = new QDoubleSpinBox(editorContainer);
    QList<QDoubleSpinBox *> spins = {xSpin, ySpin, zSpin};

    const bool isColorEditor = editorUrl.contains("InspectColorField.qml");
    for (QDoubleSpinBox *spin : spins)
    {
      spin->setDecimals(3);
      spin->setSingleStep(isColorEditor ? 0.01 : 0.1);
      spin->setRange(isColorEditor ? 0.0 : -1e6, isColorEditor ? 1.0 : 1e6);
      spin->setEnabled(!field->isReadOnly());
      spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
      spin->setMinimumWidth(0);
      spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
      editorLayout->addWidget(spin);
    }

    auto emitValue = [this, field, xSpin, ySpin, zSpin]()
    {
      if (isApplyingEditorState)
      {
        return;
      }

      field->setValue(QVariantList{xSpin->value(), ySpin->value(), zSpin->value()});
    };

    QObject::connect(xSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [emitValue](double)
    {
      emitValue();
    });
    QObject::connect(ySpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [emitValue](double)
    {
      emitValue();
    });
    QObject::connect(zSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [emitValue](double)
    {
      emitValue();
    });

    rowLayout->addWidget(editorContainer, 1);

    editorBindings.push_back({field, [xSpin, ySpin, zSpin](const QVariant &value)
    {
      const QVariantList list = ToVec3List(value);
      xSpin->setValue(list[0].toDouble());
      ySpin->setValue(list[1].toDouble());
      zSpin->setValue(list[2].toDouble());
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
