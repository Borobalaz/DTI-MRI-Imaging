#include "widgets/inspect_fields/InspectVec3FieldWidget.h"

#include "widgets/inspect_fields/InspectSpinBox.h"

InspectVec3FieldWidget::InspectVec3FieldWidget(QWidget *parent)
  : QWidget(parent)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  xSpin = new InspectSpinBox(this);
  ySpin = new InspectSpinBox(this);
  zSpin = new InspectSpinBox(this);
  for (InspectSpinBox *spin : {xSpin, ySpin, zSpin})
  {
    spin->setBaseStep(0.1);
    // Keep the established 3-decimal display; it already covers the 0.01 Shift step.
    spin->setDecimals(3);
    spin->setRange(-1e6, 1e6);
    spin->setMinimumWidth(0);
    spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(spin);
    QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double)
    {
      if (valueChangedCallback) valueChangedCallback(GetValue());
    });
  }
}

InspectVec3FieldWidget::InspectVec3FieldWidget(QString fieldId, QString displayName,
                                               QString groupName, bool readOnly, QWidget *parent)
  : InspectVec3FieldWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  readOnlyValue = readOnly;
  setEnabled(!readOnlyValue);
}

IInspectWidget *InspectVec3FieldWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectVec3FieldWidget::fieldId() const { return fieldIdValue; }
QString InspectVec3FieldWidget::displayName() const { return displayNameValue; }
QString InspectVec3FieldWidget::groupName() const { return groupNameValue; }
bool InspectVec3FieldWidget::isReadOnly() const { return readOnlyValue; }

void InspectVec3FieldWidget::SetValue(const QVariant &value)
{
  QVariantList list = value.toList();
  if (list.size() < 3) list = {0.0, 0.0, 0.0};
  const QSignalBlocker bx(xSpin);
  const QSignalBlocker by(ySpin);
  const QSignalBlocker bz(zSpin);
  xSpin->setValue(list[0].toDouble());
  ySpin->setValue(list[1].toDouble());
  zSpin->setValue(list[2].toDouble());
}

QVariant InspectVec3FieldWidget::GetValue() const
{
  if (valueGetter) return valueGetter();
  return QVariantList{xSpin->value(), ySpin->value(), zSpin->value()};
}