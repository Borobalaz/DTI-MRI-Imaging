#include "widgets/inspect_fields/InspectCheckboxFieldWidget.h"

InspectCheckboxFieldWidget::InspectCheckboxFieldWidget(QWidget *parent)
  : QCheckBox(parent)
{
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QObject::connect(this, &QCheckBox::toggled, this, [this](bool checked)
  {
    if (valueChangedCallback) valueChangedCallback(checked);
  });
}

InspectCheckboxFieldWidget::InspectCheckboxFieldWidget(QString fieldId, QString displayName,
                                                       QString groupName, bool readOnly, QWidget *parent)
  : InspectCheckboxFieldWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  readOnlyValue = readOnly;
  setEnabled(!readOnlyValue);
}

IInspectWidget *InspectCheckboxFieldWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectCheckboxFieldWidget::fieldId() const { return fieldIdValue; }
QString InspectCheckboxFieldWidget::displayName() const { return displayNameValue; }
QString InspectCheckboxFieldWidget::groupName() const { return groupNameValue; }
bool InspectCheckboxFieldWidget::isReadOnly() const { return readOnlyValue; }

void InspectCheckboxFieldWidget::SetValue(const QVariant &value)
{
  const QSignalBlocker blocker(this);
  setChecked(value.toBool());
}

QVariant InspectCheckboxFieldWidget::GetValue() const { return isChecked(); }