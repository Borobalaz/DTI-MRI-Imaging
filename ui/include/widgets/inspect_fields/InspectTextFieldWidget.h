#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>

#include "widgets/inspect_fields/IInspectWidget.h"

class InspectTextFieldWidget : public QLineEdit, public IInspectWidget
{
public:
  explicit InspectTextFieldWidget(QWidget *parent = nullptr);
  InspectTextFieldWidget(QString fieldId, QString displayName, QString groupName,
                         bool readOnly = false, QWidget *parent = nullptr);
  IInspectWidget *addToLayout(QHBoxLayout *layout) override;
  QString fieldId() const override;
  QString displayName() const override;
  QString groupName() const override;
  bool isReadOnly() const override;
  void SetValue(const QVariant &value) override;
  QVariant GetValue() const override;

  std::function<void(const QVariant &)> valueChangedCallback;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
