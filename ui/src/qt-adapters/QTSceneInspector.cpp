#include "qt-adapters/QTSceneInspector.h"

#include <limits>
#include <sstream>

#include <QCoreApplication>
#include <QHash>
#include <QMetaObject>

#include "qt-adapters/QtInspectFieldAdapter.h"

QTSceneInspector::QTSceneInspector(QObject *parent)
    : QObject(parent)
{
  syncTimer.setInterval(33);
  syncTimer.setTimerType(Qt::PreciseTimer);
  connect(&syncTimer, &QTimer::timeout, this, [this]()
          { SyncSnapshots(); });
  syncTimer.start();
}

// Attaches the engine's inspection service (or detaches it, for nullptr) and performs the
//  initial provider sync. Subscribes to push-based provider-list-changed notifications instead
//  of per-frame polling.
void QTSceneInspector::SetInspectionService(IInspectionService *service)
{
  if (IInspectionService *previousService = session.Service(); previousService && previousService != service)
  {
    previousService->OnProvidersChanged({});
  }

  session.SetService(service);

  if (service)
  {
    // Fires on the engine thread: queue the handling onto this object's (GUI) thread,
    //  coalescing bursts into a single pending call.
    service->OnProvidersChanged([this]()
                                 {
                                   if (!providersChangedPending.exchange(true))
                                   {
                                     QMetaObject::invokeMethod(this, [this]()
                                                               { HandleQueuedProvidersChanged(); }, Qt::QueuedConnection);
                                   }
                                 });
  }

  HandleProvidersChanged();
}

void QTSceneInspector::StopSyncTimer()
{
  syncTimer.stop();
}

void QTSceneInspector::HandleQueuedProvidersChanged()
{
  providersChangedPending.store(false);

  // The service may have been detached while this call was queued.
  if (!session.Service())
  {
    return;
  }

  HandleProvidersChanged();
}

std::string QTSceneInspector::selectedObjectName() const
{
  return session.SelectedId();
}

// Selects the object with the given stable id. Clears selection if not found.
void QTSceneInspector::setSelectedObjectName(const std::string &id)
{
  if (id == session.SelectedId())
  {
    return;
  }

  session.SelectById(id);
  currentFields = AdaptFields(session.CurrentFields());
  RebuildFieldObjects();

  emit selectedProviderIndexChanged();
  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
}

QObjectList QTSceneInspector::fields() const
{
  return fieldObjects;
}

int QTSceneInspector::fieldRevision() const
{
  return revision;
}

// Reacts to the engine's push notification that the provider list structurally changed:
//  re-validates the current selection (clearing it if its provider vanished) and rebuilds the
//  field list if needed.
void QTSceneInspector::HandleProvidersChanged()
{
  const std::string previousSelection = session.SelectedId();

  emit providersChanged();

  const bool stillValid = !previousSelection.empty() && session.SelectById(previousSelection);
  if (!stillValid)
  {
    session.ClearSelection();
  }

  currentFields = AdaptFields(session.CurrentFields());
  RebuildFieldObjects();

  if (previousSelection != session.SelectedId())
  {
    emit selectedProviderIndexChanged();
  }

  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
  emit visibilityStateChanged();
}

std::vector<InspectObjectSummary> QTSceneInspector::getProviders() const
{
  return session.ProviderSummaries();
}

QVariantMap QTSceneInspector::fieldMeta(const QString &fieldId) const
{
  QVariantMap meta;

  const std::shared_ptr<IInspectWidget> field = FindField(fieldId);
  if (field)
  {
    meta = field->meta();
  }

  return meta;
}

QVariant QTSceneInspector::fieldValue(const QString &fieldId) const
{
  const std::shared_ptr<IInspectWidget> field = FindField(fieldId);
  if (!field)
  {
    return QVariant();
  }

  return field->GetValue();
}

bool QTSceneInspector::setFieldValue(const QString &fieldId, const QVariant &value)
{
  const std::shared_ptr<IInspectWidget> field = FindField(fieldId);
  if (!field)
  {
    return false;
  }

  field->SetValue(value);
  fieldSnapshots[field->fieldId()] = field->GetValue();
  ++revision;
  emit fieldRevisionChanged();
  return true;
}

bool QTSceneInspector::hasVisibility(const std::string &providerId) const
{
  return session.SupportsVisibility(providerId);
}

bool QTSceneInspector::isVisible(const std::string &providerId) const
{
  return session.IsVisible(providerId);
}

bool QTSceneInspector::setVisible(const std::string &providerId, bool visible)
{
  if (!session.SetVisible(providerId, visible))
  {
    return false;
  }

  emit fieldRevisionChanged();
  emit visibilityStateChanged();
  return true;
}

// Selects an object in the scene by casting a ray from the given origin in the given direction.
//  The closest intersecting object is selected, via the engine's inspection service. Returns
//  true if something was hit.
bool QTSceneInspector::selectObjectByRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection)
{
  const std::string previousSelection = session.SelectedId();
  if (!session.SelectByRay(rayOrigin, rayDirection))
  {
    return false;
  }

  currentFields = AdaptFields(session.CurrentFields());
  RebuildFieldObjects();

  if (session.SelectedId() != previousSelection)
  {
    emit selectedProviderIndexChanged();
  }
  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
  return true;
}

void QTSceneInspector::RebuildFieldObjects()
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
    if (QCoreApplication::instance() && obj && obj->thread() != QCoreApplication::instance()->thread())
    {
      obj->moveToThread(QCoreApplication::instance()->thread());
    }

    if (obj)
    {
      fieldObjects.push_back(obj);
      fieldSnapshots.insert(field->fieldId(), field->GetValue());
    }
  }
}

void QTSceneInspector::SyncSnapshots()
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

std::shared_ptr<IInspectWidget> QTSceneInspector::FindField(const QString &fieldId) const
{
  for (const std::shared_ptr<IInspectWidget> &field : currentFields)
  {
    if (field && field->fieldId() == fieldId)
    {
      return field;
    }
  }
  return nullptr;
}

