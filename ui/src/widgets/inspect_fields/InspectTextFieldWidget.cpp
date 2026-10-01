#include "widgets/inspect_fields/InspectTextFieldWidget.h"

InspectTextFieldWidget::InspectTextFieldWidget(QWidget *parent)
  : QLineEdit(parent)
{
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QObject::connect(this, &QLineEdit::editingFinished, this, [this]()
  {
    if (valueChangedCallback) valueChangedCallback(text());
  });
}

InspectTextFieldWidget::InspectTextFieldWidget(QString fieldId, QString displayName,
                                               QString groupName, bool readOnly, QWidget *parent)
  : InspectTextFieldWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  readOnlyValue = readOnly;
  setEnabled(!readOnlyValue);
}

IInspectWidget *InspectTextFieldWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectTextFieldWidget::fieldId() const { return fieldIdValue; }
QString InspectTextFieldWidget::displayName() const { return displayNameValue; }
QString InspectTextFieldWidget::groupName() const { return groupNameValue; }
bool InspectTextFieldWidget::isReadOnly() const { return readOnlyValue; }

void InspectTextFieldWidget::SetValue(const QVariant &value)
{
  const QSignalBlocker blocker(this);
  setText(value.toString());
}

QVariant InspectTextFieldWidget::GetValue() const { return text(); }