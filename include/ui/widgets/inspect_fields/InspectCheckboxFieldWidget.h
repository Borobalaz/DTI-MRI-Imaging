#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectCheckboxFieldWidget : public QCheckBox, public IInspectWidget
{
public:
  explicit InspectCheckboxFieldWidget(QWidget *parent = nullptr)
    : QCheckBox(parent)
  {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QObject::connect(this, &QCheckBox::toggled, this, [this](bool checked)
    {
      if (valueChangedCallback)
      {
        valueChangedCallback(checked);
      }
    });
  }

  InspectCheckboxFieldWidget(QString fieldId,
                             QString displayName,
                             QString groupName,
                             bool readOnly = false,
                             QWidget *parent = nullptr)
    : InspectCheckboxFieldWidget(parent)
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
    const QSignalBlocker blocker(this);
    setChecked(value.toBool());
  }

  QVariant GetValue() const override
  {
    return isChecked();
  }

  std::function<void(const QVariant &)> valueChangedCallback;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
