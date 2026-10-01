#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QComboBox>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "widgets/inspect_fields/IInspectWidget.h"

class InspectDropdownFieldWidget : public QComboBox, public IInspectWidget
{
public:
  explicit InspectDropdownFieldWidget(QWidget *parent = nullptr);
  InspectDropdownFieldWidget(QString fieldId, QString displayName, QString groupName,
                             QStringList options, bool readOnly = false, QWidget *parent = nullptr);
  void SetOptions(const QStringList &options);
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
