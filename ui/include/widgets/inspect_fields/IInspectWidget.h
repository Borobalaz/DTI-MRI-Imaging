#pragma once

#include <QHBoxLayout>
#include <QUrl>
#include <QStringList>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class IInspectWidget
{
public:
  virtual ~IInspectWidget() = default;

  virtual QString fieldId() const = 0;
  virtual QString displayName() const = 0;
  virtual QString groupName() const = 0;
  virtual bool isReadOnly() const = 0;
  virtual QStringList enumOptions() const { return {}; }
  virtual double minimum() const { return 0.0; }
  virtual double maximum() const { return 1.0; }
  virtual QVariantMap meta() const
  {
    QVariantMap map;
    map["fieldId"] = fieldId();
    map["displayName"] = displayName();
    map["groupName"] = groupName();
    map["readOnly"] = isReadOnly();
    map["minimum"] = minimum();
    map["maximum"] = maximum();
    map["enumOptions"] = enumOptions();
    return map;
  }

  virtual IInspectWidget *addToLayout(QHBoxLayout *layout) = 0;
  virtual void SetValue(const QVariant& value) = 0;
  virtual QVariant GetValue() const = 0;
};
