#pragma once

#include <functional>

#include "ui/mediator/InspectField.h"

class InspectSliderField : public InspectField
{
  Q_OBJECT

public:
  using Getter = std::function<QVariant()>;
  using Setter = std::function<void(const QVariant&)>;

  InspectSliderField(QString fieldId,
                     QString displayName,
                     QString groupName,
                     double minimumValue,
                     double maximumValue,
                     double stepValue,
                     Getter getter,
                     Setter setter,
                     bool readOnly = false,
                     QObject *parent = nullptr)
    : InspectField(std::move(fieldId), std::move(displayName), std::move(groupName), readOnly, parent),
      minimumValue(minimumValue),
      maximumValue(maximumValue),
      stepValue(stepValue),
      getter(std::move(getter)),
      setter(std::move(setter))
  {
  }

  QUrl editorUrl() const override { return QUrl(QStringLiteral("qrc:/qml/InspectNumberField.qml")); }
  QVariant value() const override { return getter ? getter() : QVariant(0.0); }
  double minimum() const override { return minimumValue; }
  double maximum() const override { return maximumValue; }

  bool trySetValue(const QVariant &input) override
  {
    if (!setter)
    {
      return false;
    }

    setter(input);
    return true;
  }

private:
  double minimumValue = 0.0;
  double maximumValue = 1.0;
  double stepValue = 0.1;
  Getter getter;
  Setter setter;
};
