#include "qt-adapters/QTEngineSettingsAdapter.h"

#include <QCoreApplication>

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

void QTEngineSettingsAdapter::SetSettingsService(ISettingsService *service)
{
  settingsService = service;
  if (settingsService) settingsService->OnSettingsChanged([this]() { HandleSettingsChanged(); });
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
