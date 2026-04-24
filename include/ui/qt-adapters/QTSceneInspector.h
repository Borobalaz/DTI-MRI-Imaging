#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

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
  bool selectObjectByRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);

  QObjectList fields() const;
  int fieldRevision() const;

  void Update(const std::vector<InspectProvider*>& providers);
  void SetProviders(const std::vector<InspectProvider*>& providers);
  const std::vector<InspectProvider*>& getProviders() const { return providers; }

  QVariantMap fieldMeta(const QString& fieldId) const;
  QVariant fieldValue(const QString& fieldId) const;
  bool setFieldValue(const QString& fieldId, const QVariant& value);
  bool hasVisibility(const std::string& providerName) const;
  bool isVisible(const std::string& providerName) const;
  bool setVisible(const std::string& providerName, bool visible);

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
  InspectProvider* FindProviderByName(const std::string& name) const;
  std::shared_ptr<IInspectWidget> FindVisibilityField(const std::vector<std::shared_ptr<IInspectWidget>>& fields) const;

  std::vector<InspectProvider*> providers;  // references to the inspectable objects in scene
  std::string selectedProviderName = "";
  int revision = 0;
  std::vector<std::shared_ptr<IInspectWidget>> currentFields; // fields of the currently selected object
  QObjectList fieldObjects;
  QHash<QString, QVariant> fieldSnapshots;
  QTimer syncTimer;
};

