#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectVec3FieldWidget : public QWidget, public IInspectWidget
{
public:
  explicit InspectVec3FieldWidget(QWidget *parent = nullptr)
    : QWidget(parent)
  {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    xSpin = new QDoubleSpinBox(this);
    ySpin = new QDoubleSpinBox(this);
    zSpin = new QDoubleSpinBox(this);

    for (QDoubleSpinBox *spin : {xSpin, ySpin, zSpin})
    {
      spin->setDecimals(3);
      spin->setSingleStep(0.1);
      spin->setRange(-1e6, 1e6);
      spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
      spin->setMinimumWidth(0);
      spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
      layout->addWidget(spin);

      QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double)
      {
        if (valueChangedCallback)
        {
          valueChangedCallback(GetValue());
        }
      });
    }
  }

  InspectVec3FieldWidget(QString fieldId,
                         QString displayName,
                         QString groupName,
                         bool readOnly = false,
                         QWidget *parent = nullptr)
    : InspectVec3FieldWidget(parent)
  {
    fieldIdValue = std::move(fieldId);
    displayNameValue = std::move(displayName);
    groupNameValue = std::move(groupName);
    readOnlyValue = readOnly;
    setEnabled(!readOnlyValue);
  }

  IInspectWidget *addToLayout(QHBoxLayout *layout) override
  {
    layout->addWidget(this, 1);
    return this;
  }

  QString fieldId() const override { return fieldIdValue; }
  QString displayName() const override { return displayNameValue; }
  QString groupName() const override { return groupNameValue; }
  bool isReadOnly() const override { return readOnlyValue; }

  void SetValue(const QVariant &value) override
  {
    QVariantList list = value.toList();
    if (list.size() < 3)
    {
      list = {0.0, 0.0, 0.0};
    }

    const QSignalBlocker bx(xSpin);
    const QSignalBlocker by(ySpin);
    const QSignalBlocker bz(zSpin);
    xSpin->setValue(list[0].toDouble());
    ySpin->setValue(list[1].toDouble());
    zSpin->setValue(list[2].toDouble());
  }

  QVariant GetValue() const override
  {
    return QVariantList{xSpin->value(), ySpin->value(), zSpin->value()};
  }

  std::function<void(const QVariant &)> valueChangedCallback;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
  QDoubleSpinBox *xSpin = nullptr;
  QDoubleSpinBox *ySpin = nullptr;
  QDoubleSpinBox *zSpin = nullptr;
};
