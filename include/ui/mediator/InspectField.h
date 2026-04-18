#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>
#include <QObject>

/**
 * @brief A base class for inspectable fields in the inspector widget.
 * 
 */
class InspectField : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString fieldId READ fieldId CONSTANT)
  Q_PROPERTY(QString displayName READ displayName CONSTANT)
  Q_PROPERTY(QString groupName READ groupName CONSTANT)
  Q_PROPERTY(bool readOnly READ isReadOnly CONSTANT)
  Q_PROPERTY(QUrl editorUrl READ editorUrl CONSTANT)
  Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
  Q_PROPERTY(QStringList enumOptions READ enumOptions CONSTANT)
  Q_PROPERTY(double minimum READ minimum CONSTANT)
  Q_PROPERTY(double maximum READ maximum CONSTANT)

public:
  InspectField(QString fieldId,
               QString displayName,
               QString groupName,
               bool readOnly = false,
               QObject *parent = nullptr)
    : QObject(parent),
      fieldIdValue(std::move(fieldId)),
      displayNameValue(std::move(displayName)),
      groupNameValue(std::move(groupName)),
      readOnlyValue(readOnly)
  {
  }

  ~InspectField() override = default;

  QString fieldId() const { return fieldIdValue; }
  QString displayName() const { return displayNameValue; }
  QString groupName() const { return groupNameValue; }
  bool isReadOnly() const { return readOnlyValue; }

  virtual QUrl editorUrl() const = 0;
  virtual QVariant value() const = 0;
  virtual bool trySetValue(const QVariant &value) = 0;
  virtual QStringList enumOptions() const { return {}; }
  virtual double minimum() const { return 0.0; }
  virtual double maximum() const { return 1.0; }

  QVariantMap meta() const
  {
    QVariantMap map;
    map["fieldId"] = fieldIdValue;
    map["displayName"] = displayNameValue;
    map["groupName"] = groupNameValue;
    map["readOnly"] = readOnlyValue;
    map["minimum"] = minimum();
    map["maximum"] = maximum();
    map["enumOptions"] = enumOptions();
    map["editorUrl"] = editorUrl().toString();
    return map;
  }

public slots:
  void setValue(const QVariant &value)
  {
    if (!readOnlyValue && trySetValue(value))
    {
      emit valueChanged();
    }
  }

  void notifyValueChanged()
  {
    emit valueChanged();
  }

signals:
  void valueChanged();

protected:
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
