#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariant>
#include "dashboard/ISceneDashboardService.h"
#include "widgets/inspect_fields/IInspectWidget.h"

// Thin Qt adapter around ISceneDashboardService, shaped like QTEngineSettingsAdapter: exposes the
//  active scene's scene-wide controls (preprocessing, tractography, rotation, ...) as Qt field
//  widgets for the Scene Dashboard dock, polling for per-value-edit changes and reacting to the
//  engine's structural (field set changed) push notification.
class QTSceneDashboardAdapter : public QObject
{
  Q_OBJECT
public:
  explicit QTSceneDashboardAdapter(QObject *parent = nullptr);
  void SetSceneDashboardService(ISceneDashboardService *service);
  // Stops polling field values; used before detaching from a shutting-down engine.
  void StopSyncTimer();
  QObjectList fields() const;
  int fieldRevision() const;

signals:
  void fieldsChanged();
  void fieldRevisionChanged();
  void fieldValueChanged(const QString &fieldId, const QVariant &value);

private:
  void HandleSceneDashboardChanged();
  void HandleQueuedSceneDashboardChanged();
  void RebuildFieldObjects();
  void SyncSnapshots();

  ISceneDashboardService *sceneDashboardService = nullptr;
  std::atomic<bool> fieldsChangedPending{false}; // set on the engine thread, cleared on the GUI thread
  int revision = 0;
  std::vector<std::shared_ptr<IInspectWidget>> currentFields;
  QObjectList fieldObjects;
  QHash<QString, QVariant> fieldSnapshots;
  QTimer syncTimer;
};
