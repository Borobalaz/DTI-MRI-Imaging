#pragma once

#include <functional>
#include <utility>

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QObject>
#include <QSignalBlocker>
#include <QPointer>
#include <QString>
#include <QVariant>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectNumberFieldWidget : public QObject, public IInspectWidget
{
  Q_OBJECT

public:
  using Getter = std::function<double()>;
  using Setter = std::function<void(double)>;

  InspectNumberFieldWidget(QString fieldId,
                           QString displayName,
                           QString groupName,
                           Getter getter,
                           Setter setter,
                           double minimumValue = 0.0,
                           double maximumValue = 1.0,
                           bool readOnly = false,
                           QObject *parent = nullptr)
    : QObject(parent),
      fieldIdValue(std::move(fieldId)),
      displayNameValue(std::move(displayName)),
      groupNameValue(std::move(groupName)),
      getter(std::move(getter)),
      setter(std::move(setter)),
      minimumValue(minimumValue),
      maximumValue(maximumValue),
      readOnlyValue(readOnly)
  {
  }

  InspectNumberFieldWidget(QString fieldId,
                           QString displayName,
                           QString groupName,
                           double minimumValue = 0.0,
                           double maximumValue = 1.0,
                           bool readOnly = false,
                           QObject *parent = nullptr)
    : InspectNumberFieldWidget(std::move(fieldId),
                               std::move(displayName),
                               std::move(groupName),
                               Getter{},
                               Setter{},
                               minimumValue,
                               maximumValue,
                               readOnly,
                               parent)
  {
  }

  QString fieldId() const override { return fieldIdValue; }
  QString displayName() const override { return displayNameValue; }
  QString groupName() const override { return groupNameValue; }
  bool isReadOnly() const override { return readOnlyValue; }
  double minimum() const override { return minimumValue; }
  double maximum() const override { return maximumValue; }

  QVariant value() const
  {
    return getter ? QVariant::fromValue(getter()) : QVariant(0.0);
  }

  QVariant GetValue() const override { return value(); }

  void SetValue(const QVariant &input) override
  {
    if (setter)
    {
      setter(input.toDouble());
    }
  }

  IInspectWidget *addToLayout(QHBoxLayout *layout) override
  {
    auto *editor = new QDoubleSpinBox;
    editor->setDecimals(4);
    editor->setSingleStep(0.01);
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

  ~InspectNumberFieldWidget() override = default;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  Getter getter;
  Setter setter;
  double minimumValue = 0.0;
  double maximumValue = 1.0;
  bool readOnlyValue = false;
  QPointer<QDoubleSpinBox> editorWidget;
};
