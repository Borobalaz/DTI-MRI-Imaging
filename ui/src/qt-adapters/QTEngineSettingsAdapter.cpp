#include "qt-adapters/QTEngineSettingsAdapter.h"

#include <QMetaObject>

#include "qt-adapters/QtInspectFieldAdapter.h"

QTEngineSettingsAdapter::QTEngineSettingsAdapter(QObject *parent)
    : QObject(parent)
{
  syncTimer.setInterval(33);
  syncTimer.setTimerType(Qt::PreciseTimer);
  connect(&syncTimer, &QTimer::timeout, this, [this]()
          { SyncSnapshots(); });
  syncTimer.start();
}

// Attaches the engine's settings service (or detaches it, for nullptr) and performs the initial sync.
void QTEngineSettingsAdapter::SetSettingsService(ISettingsService *service)
{
  if (settingsService && settingsService != service)
  {
    settingsService->OnSettingsChanged({});
  }

  settingsService = service;
  if (settingsService)
  {
    // Fires on the engine thread: queue the handling onto this object's (GUI) thread,
    //  coalescing bursts into a single pending call.
    settingsService->OnSettingsChanged([this]()
    {
      if (!settingsChangedPending.exchange(true))
      {
        QMetaObject::invokeMethod(this, [this]() { HandleQueuedSettingsChanged(); }, Qt::QueuedConnection);
      }
    });
  }
  HandleSettingsChanged();
}

void QTEngineSettingsAdapter::StopSyncTimer()
{
  syncTimer.stop();
}

void QTEngineSettingsAdapter::HandleQueuedSettingsChanged()
{
  settingsChangedPending.store(false);

  // The service may have been detached while this call was queued.
  if (!settingsService)
  {
    return;
  }

  HandleSettingsChanged();
}

void QTEngineSettingsAdapter::HandleSettingsChanged()
{
  currentFields = AdaptFields(settingsService ? settingsService->GetSettings() : std::vector<InspectFieldPtr>{});
  RebuildFieldObjects();
  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
}

QObjectList QTEngineSettingsAdapter::fields() const
{
  return fieldObjects;
}

int QTEngineSettingsAdapter::fieldRevision() const
{
  return revision;
}

void QTEngineSettingsAdapter::RebuildFieldObjects()
{
  fieldObjects.clear();
  fieldSnapshots.clear();
  for (const std::shared_ptr<IInspectWidget> &field : currentFields)
  {
    if (!field)
    {
      continue;
    }

    auto *obj = dynamic_cast<QObject *>(field.get());
    if (obj)
    {
      fieldObjects.push_back(obj);
      fieldSnapshots.insert(field->fieldId(), field->GetValue());
    }
  }
}

void QTEngineSettingsAdapter::SyncSnapshots()
{
  for (const std::shared_ptr<IInspectWidget> &field : currentFields)
  {
    if (!field)
    {
      continue;
    }

    const QVariant currentValue = field->GetValue();
    const QVariant previousValue = fieldSnapshots.value(field->fieldId());
    if (currentValue != previousValue)
    {
      fieldSnapshots.insert(field->fieldId(), currentValue);
      emit fieldValueChanged(field->fieldId(), currentValue);
      ++revision;
      emit fieldRevisionChanged();
    }
  }
}
