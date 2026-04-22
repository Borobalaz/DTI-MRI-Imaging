#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "ui/widgets/inspect_fields/IInspectWidget.h"

class InspectProvider;

class QTSceneInspector : public QObject
{
  Q_OBJECT
public:
  explicit QTSceneInspector(QObject* parent = nullptr);

  std::string selectedObjectName() const;
  void setSelectedObjectName(const std::string& name);

  QObjectList fields() const;
  int fieldRevision() const;

  void Update(const std::vector<InspectProvider*>& providers);
  void SetProviders(const std::vector<InspectProvider*>& providers);

  QVariantMap fieldMeta(const QString& fieldId) const;
  QVariant fieldValue(const QString& fieldId) const;
  bool setFieldValue(const QString& fieldId, const QVariant& value);
  bool hasVisibility(int index) const;
  bool isVisible(int index) const;
  bool setVisible(int index, bool visible);

signals:
  void providersChanged();
  void selectedProviderIndexChanged();
  void fieldsChanged();
  void fieldRevisionChanged();
  void fieldValueChanged(const QString& fieldId, const QVariant& value);
  void visibilityStateChanged();

private:
  void RebuildFieldObjects();
  void SyncSnapshots();
  std::shared_ptr<IInspectWidget> FindField(const QString& fieldId) const;

  std::vector<InspectProvider*> providers;  // references to the inspectable objects in scene
  std::string selectedProviderName = "";
  int revision = 0;
  std::vector<std::shared_ptr<IInspectWidget>> currentFields; // fields of the currently selected object
  QObjectList fieldObjects;
  QHash<QString, QVariant> fieldSnapshots;
  QTimer syncTimer;
};

