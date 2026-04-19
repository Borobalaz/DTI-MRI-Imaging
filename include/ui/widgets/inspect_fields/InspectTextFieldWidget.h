#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectTextFieldWidget : public QLineEdit, public IInspectWidget
{
public:
  explicit InspectTextFieldWidget(QWidget *parent = nullptr)
    : QLineEdit(parent)
  {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    QObject::connect(this, &QLineEdit::editingFinished, this, [this]()
    {
      if (valueChangedCallback)
      {
        valueChangedCallback(text());
      }
    });
  }

  InspectTextFieldWidget(QString fieldId,
                         QString displayName,
                         QString groupName,
                         bool readOnly = false,
                         QWidget *parent = nullptr)
    : InspectTextFieldWidget(parent)
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
    setText(value.toString());
  }

  QVariant GetValue() const override
  {
    return text();
  }

  std::function<void(const QVariant &)> valueChangedCallback;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
