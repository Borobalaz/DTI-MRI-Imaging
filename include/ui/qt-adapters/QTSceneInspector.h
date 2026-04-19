#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "ui/mediator/InspectField.h"

class InspectProvider;
class InspectField;

class QTSceneInspector : public QObject
{
  Q_OBJECT
public:
  explicit QTSceneInspector(QObject* parent = nullptr);

  QStringList objectNames() const;
  int selectedObjectIndex() const;
  void setSelectedObjectIndex(int index);

  QObjectList fields() const;
  int fieldRevision() const;

  void Update(const std::vector<InspectProvider*>& providers);
  void SetProviders(const std::vector<InspectProvider*>& providers);

  QVariantMap fieldMeta(const QString& fieldId) const;
  QVariant fieldValue(const QString& fieldId) const;
  bool setFieldValue(const QString& fieldId, const QVariant& value);

signals:
  void objectNamesChanged();
  void selectedObjectIndexChanged();
  void fieldsChanged();
  void fieldRevisionChanged();
  void fieldValueChanged(const QString& fieldId, const QVariant& value);

private:
  void RebuildFieldObjects();
  void SyncSnapshots();
  std::shared_ptr<InspectField> FindField(const QString& fieldId) const;

  std::vector<InspectProvider*> providers;  // references to the inspectable objects in scene
  QStringList providerNames;
  int selectedIndex = -1;
  int revision = 0;
  std::vector<std::shared_ptr<InspectField>> currentFields; // fields of the currently selected object
  QObjectList fieldObjects;
  QHash<QString, QVariant> fieldSnapshots;
  QTimer syncTimer;
};
