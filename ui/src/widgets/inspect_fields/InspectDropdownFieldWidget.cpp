#include "widgets/inspect_fields/InspectDropdownFieldWidget.h"

InspectDropdownFieldWidget::InspectDropdownFieldWidget(QWidget *parent)
  : QComboBox(parent)
{
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QObject::connect(this, &QComboBox::currentTextChanged, this, [this](const QString &text)
  {
    if (valueChangedCallback) valueChangedCallback(text);
  });
}

InspectDropdownFieldWidget::InspectDropdownFieldWidget(QString fieldId, QString displayName,
                                                       QString groupName, QStringList options,
                                                       bool readOnly, QWidget *parent)
  : InspectDropdownFieldWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  readOnlyValue = readOnly;
  SetOptions(options);
  setEnabled(!readOnlyValue);
}

void InspectDropdownFieldWidget::SetOptions(const QStringList &options)
{
  clear();
  addItems(options);
}

IInspectWidget *InspectDropdownFieldWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectDropdownFieldWidget::fieldId() const { return fieldIdValue; }
QString InspectDropdownFieldWidget::displayName() const { return displayNameValue; }
QString InspectDropdownFieldWidget::groupName() const { return groupNameValue; }
bool InspectDropdownFieldWidget::isReadOnly() const { return readOnlyValue; }

void InspectDropdownFieldWidget::SetValue(const QVariant &value)
{
  const QSignalBlocker blocker(this);
  const int index = findText(value.toString());
  if (index >= 0) setCurrentIndex(index);
}

QVariant InspectDropdownFieldWidget::GetValue() const { return currentText(); }