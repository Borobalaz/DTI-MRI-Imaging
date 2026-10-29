#include "qt-adapters/QTSceneDashboardAdapter.h"

#include <QMetaObject>

#include "qt-adapters/QtInspectFieldAdapter.h"

QTSceneDashboardAdapter::QTSceneDashboardAdapter(QObject *parent)
    : QObject(parent)
{
  syncTimer.setInterval(33);
  syncTimer.setTimerType(Qt::PreciseTimer);
  connect(&syncTimer, &QTimer::timeout, this, [this]()
          { SyncSnapshots(); });
  syncTimer.start();
}

// Attaches the engine's scene dashboard service (or detaches it, for nullptr) and performs the initial sync.
void QTSceneDashboardAdapter::SetSceneDashboardService(ISceneDashboardService *service)
{
  if (sceneDashboardService && sceneDashboardService != service)
  {
    sceneDashboardService->OnFieldsChanged({});
  }

  sceneDashboardService = service;
  if (sceneDashboardService)
  {
    // Fires on the engine thread: queue the handling onto this object's (GUI) thread,
    //  coalescing bursts into a single pending call.
    sceneDashboardService->OnFieldsChanged([this]()
    {
      if (!fieldsChangedPending.exchange(true))
      {
        QMetaObject::invokeMethod(this, [this]() { HandleQueuedSceneDashboardChanged(); }, Qt::QueuedConnection);
      }
    });
  }
  HandleSceneDashboardChanged();
}

void QTSceneDashboardAdapter::StopSyncTimer()
{
  syncTimer.stop();
}

void QTSceneDashboardAdapter::HandleQueuedSceneDashboardChanged()
{
  fieldsChangedPending.store(false);

  // The service may have been detached while this call was queued.
  if (!sceneDashboardService)
  {
    return;
  }

  HandleSceneDashboardChanged();
}

void QTSceneDashboardAdapter::HandleSceneDashboardChanged()
{
  currentFields = AdaptFields(sceneDashboardService ? sceneDashboardService->GetFields() : std::vector<InspectFieldPtr>{});
  RebuildFieldObjects();
  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
}

QObjectList QTSceneDashboardAdapter::fields() const
{
  return fieldObjects;
}

int QTSceneDashboardAdapter::fieldRevision() const
{
  return revision;
}

void QTSceneDashboardAdapter::RebuildFieldObjects()
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

void QTSceneDashboardAdapter::SyncSnapshots()
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
