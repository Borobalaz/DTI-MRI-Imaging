#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include "Inspection/IInspectionService.h"
#include "qt-adapters/InspectionSession.h"
#include "qt-adapters/InspectObjectSummary.h"
#include "widgets/inspect_fields/IInspectWidget.h"

/**
 * @brief Thin Qt adapter around InspectionSession: owns the polling timer and converts
 *  engine-neutral InspectField/InspectValue data into Qt widgets/QVariant for the UI.
 */
class QTSceneInspector : public QObject
{
  Q_OBJECT
public:
  explicit QTSceneInspector(QObject* parent = nullptr);

  void SetInspectionService(IInspectionService* service);

  std::string selectedObjectName() const;
  void setSelectedObjectName(const std::string& id);
  bool selectObjectByRay(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);

  QObjectList fields() const;
  int fieldRevision() const;

  QVariantMap fieldMeta(const QString& fieldId) const;
  QVariant fieldValue(const QString& fieldId) const;
  bool setFieldValue(const QString& fieldId, const QVariant& value);
  bool hasVisibility(const std::string& providerId) const;
  bool isVisible(const std::string& providerId) const;
  bool setVisible(const std::string& providerId, bool visible);

  std::vector<InspectObjectSummary> getProviders() const;

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
  void HandleProvidersChanged();
  std::shared_ptr<IInspectWidget> FindField(const QString& fieldId) const;

  InspectionSession session;
  int revision = 0;
  std::vector<std::shared_ptr<IInspectWidget>> currentFields; // fields of the currently selected object
  QObjectList fieldObjects;
  QHash<QString, QVariant> fieldSnapshots;
  QTimer syncTimer;
};

