#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QObject>
#include <QSignalBlocker>
#include <QPointer>
#include <QString>
#include <QVariant>

#include "widgets/inspect_fields/IInspectWidget.h"

class InspectNumberFieldWidget : public QObject, public IInspectWidget
{
  Q_OBJECT

public:
  using Getter = std::function<double()>;
  using Setter = std::function<void(double)>;

  InspectNumberFieldWidget(QString fieldId, QString displayName, QString groupName,
                           Getter getter, Setter setter, double minimumValue = 0.0,
                           double maximumValue = 1.0, double stepSize = 0.01,
                           bool readOnly = false, QObject *parent = nullptr);

  InspectNumberFieldWidget(QString fieldId,
                           QString displayName,
                           QString groupName,
                           double minimumValue = 0.0,
                           double maximumValue = 1.0,
                             double stepSize = 0.01,
                           bool readOnly = false,
                           QObject *parent = nullptr)
    : InspectNumberFieldWidget(std::move(fieldId), std::move(displayName), std::move(groupName),
                               Getter{}, Setter{}, minimumValue, maximumValue, stepSize, readOnly, parent) {}

  QString fieldId() const override;
  QString displayName() const override;
  QString groupName() const override;
  bool isReadOnly() const override;
  double minimum() const override;
  double maximum() const override;
  double singleStep() const;

  QVariant value() const;

  QVariant GetValue() const override;
  void SetValue(const QVariant &input) override;
  IInspectWidget *addToLayout(QHBoxLayout *layout) override;

  ~InspectNumberFieldWidget() override = default;

private:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  Getter getter;
  Setter setter;
  double minimumValue = 0.0;
  double maximumValue = 1.0;
  double stepSize = 0.01;
  bool readOnlyValue = false;
  QPointer<QDoubleSpinBox> editorWidget;

  static int DecimalsForStep(double step);
};
