#pragma once

#include <functional>
#include <utility>

#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "widgets/inspect_fields/IInspectWidget.h"

class InspectVec3FieldWidget : public QWidget, public IInspectWidget
{
public:
  using Getter = std::function<QVariant()>;

  explicit InspectVec3FieldWidget(QWidget *parent = nullptr);
  InspectVec3FieldWidget(QString fieldId, QString displayName, QString groupName,
                         bool readOnly = false, QWidget *parent = nullptr);
  IInspectWidget *addToLayout(QHBoxLayout *layout) override;
  QString fieldId() const override;
  QString displayName() const override;
  QString groupName() const override;
  bool isReadOnly() const override;
  void SetValue(const QVariant &value) override;
  QVariant GetValue() const override;

  Getter valueGetter;
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
