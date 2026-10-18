#pragma once
#include <memory>
#include <vector>
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariant>
#include "Settings/ISettingsService.h"
#include "widgets/inspect_fields/IInspectWidget.h"

// Thin Qt adapter around ISettingsService: a selection-free subset of QTSceneInspector that
//  exposes flat, non-selection-based engine-wide settings (e.g. "Enable Postprocessing") as
//  Qt field widgets, polling for per-value-edit changes and reacting to the engine's
//  structural (registered/unregistered) push notification.
class QTEngineSettingsAdapter : public QObject
{
  Q_OBJECT
public:
  explicit QTEngineSettingsAdapter(QObject *parent = nullptr);
  void SetSettingsService(ISettingsService *service);
  QObjectList fields() const;
  int fieldRevision() const;

signals:
  void fieldsChanged();
  void fieldRevisionChanged();
  void fieldValueChanged(const QString &fieldId, const QVariant &value);

private:
  void HandleSettingsChanged();
  void RebuildFieldObjects();
  void SyncSnapshots();

  ISettingsService *settingsService = nullptr;
  int revision = 0;
  std::vector<std::shared_ptr<IInspectWidget>> currentFields;
  QObjectList fieldObjects;
  QHash<QString, QVariant> fieldSnapshots;
  QTimer syncTimer;
};
