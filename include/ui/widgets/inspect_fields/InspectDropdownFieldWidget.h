#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QComboBox>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectDropdownFieldWidget : public QComboBox, public IInspectWidget
{
public:
  explicit InspectDropdownFieldWidget(QWidget *parent = nullptr)
    : QComboBox(parent)
  {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QObject::connect(this, &QComboBox::currentTextChanged, this, [this](const QString &text)
    {
      if (valueChangedCallback)
      {
        valueChangedCallback(text);
      }
    });
  }

  InspectDropdownFieldWidget(QString fieldId,
                             QString displayName,
                             QString groupName,
                             QStringList options,
                             bool readOnly = false,
                             QWidget *parent = nullptr)
    : InspectDropdownFieldWidget(parent)
  {
    fieldIdValue = std::move(fieldId);
    displayNameValue = std::move(displayName);
    groupNameValue = std::move(groupName);
    readOnlyValue = readOnly;
    SetOptions(options);
    setEnabled(!readOnlyValue);
  }

  void SetOptions(const QStringList &options)
  {
    clear();
    addItems(options);
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
    const QString current = value.toString();
    const int index = findText(current);
    if (index >= 0)
    {
      setCurrentIndex(index);
    }
  }

  QVariant GetValue() const override
  {
    return currentText();
  }

  std::function<void(const QVariant &)> valueChangedCallback;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
