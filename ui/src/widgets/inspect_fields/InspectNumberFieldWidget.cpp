#include "widgets/inspect_fields/InspectNumberFieldWidget.h"

#include <algorithm>
#include <cmath>

InspectNumberFieldWidget::InspectNumberFieldWidget(QString fieldId,
                                                   QString displayName,
                                                   QString groupName,
                                                   Getter getterValue,
                                                   Setter setterValue,
                                                   double minimumValueValue,
                                                   double maximumValueValue,
                                                   double stepSizeValue,
                                                   bool readOnly,
                                                   QObject *parent)
  : QObject(parent),
    fieldIdValue(std::move(fieldId)),
    displayNameValue(std::move(displayName)),
    groupNameValue(std::move(groupName)),
    getter(std::move(getterValue)),
    setter(std::move(setterValue)),
    minimumValue(minimumValueValue),
    maximumValue(maximumValueValue),
    stepSize(std::max(stepSizeValue, 1e-12)),
    readOnlyValue(readOnly)
{
}

QString InspectNumberFieldWidget::fieldId() const { return fieldIdValue; }
QString InspectNumberFieldWidget::displayName() const { return displayNameValue; }
QString InspectNumberFieldWidget::groupName() const { return groupNameValue; }
bool InspectNumberFieldWidget::isReadOnly() const { return readOnlyValue; }
double InspectNumberFieldWidget::minimum() const { return minimumValue; }
double InspectNumberFieldWidget::maximum() const { return maximumValue; }
double InspectNumberFieldWidget::singleStep() const { return stepSize; }

QVariant InspectNumberFieldWidget::value() const
{
  return getter ? QVariant::fromValue(getter()) : QVariant(0.0);
}

QVariant InspectNumberFieldWidget::GetValue() const { return value(); }

void InspectNumberFieldWidget::SetValue(const QVariant &input)
{
  const double numericValue = input.toDouble();
  if (setter)
  {
    setter(numericValue);
  }

  if (editorWidget)
  {
    const QSignalBlocker blocker(editorWidget);
    editorWidget->setValue(numericValue);
  }
}

IInspectWidget *InspectNumberFieldWidget::addToLayout(QHBoxLayout *layout)
{
  auto *editor = new QDoubleSpinBox;
  editor->setDecimals(DecimalsForStep(stepSize));
  editor->setSingleStep(stepSize);
  editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
  editor->setMinimumWidth(0);
  editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  editor->setEnabled(!isReadOnly());

  if (minimumValue < maximumValue)
  {
    editor->setRange(minimumValue, maximumValue);
  }
  else
  {
    editor->setRange(-1e9, 1e9);
  }

  QObject::connect(editor, qOverload<double>(&QDoubleSpinBox::valueChanged), editor, [this](double v)
  {
    SetValue(v);
  });

  const QSignalBlocker blocker(editor);
  editor->setValue(value().toDouble());
  layout->addWidget(editor, 1);
  editorWidget = editor;
  return this;
}

int InspectNumberFieldWidget::DecimalsForStep(double step)
{
  if (!(step > 0.0)) return 2;
  const double digits = -std::log10(step);
  if (digits <= 0.0) return 0;
  return std::clamp(static_cast<int>(std::ceil(digits)), 0, 10);
}