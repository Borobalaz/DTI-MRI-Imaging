#include "ui/qt-adapters/QTSceneInspector.h"

#include <sstream>

#include <QCoreApplication>
#include <QHash>
#include "ui/mediator/InspectProvider.h"

QTSceneInspector::QTSceneInspector(QObject* parent)
  : QObject(parent)
{
  syncTimer.setInterval(33);
  syncTimer.setTimerType(Qt::PreciseTimer);
  connect(&syncTimer, &QTimer::timeout, this, [this]()
  {
    SyncSnapshots();
  });
  syncTimer.start();
}

QStringList QTSceneInspector::objectNames() const
{
  return providerNames;
}

int QTSceneInspector::selectedObjectIndex() const
{
  return selectedIndex;
}

void QTSceneInspector::setSelectedObjectIndex(int index)
{
  if (index == selectedIndex)
  {
    return;
  }

  if (index < 0 || index >= static_cast<int>(providers.size()))
  {
    selectedIndex = -1;
    currentFields.clear();
  }
  else
  {
    selectedIndex = index;
    currentFields = providers[static_cast<size_t>(selectedIndex)]->GetInspectFields();
  }

  RebuildFieldObjects();

  emit selectedObjectIndexChanged();
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

void QTSceneInspector::Update(const std::vector<InspectProvider*>& newProviders)
{
  bool sameProviders = newProviders.size() == providers.size();
  if (sameProviders)
  {
    for (size_t i = 0; i < newProviders.size(); ++i)
    {
      if (newProviders[i] != providers[i])
      {
        sameProviders = false;
        break;
      }
    }
  }

  if (!sameProviders)
  {
    SetProviders(newProviders);
    return;
  }

  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(providers.size()))
  {
    return;
  }

  std::vector<std::shared_ptr<InspectField>> refreshedFields = providers[static_cast<size_t>(selectedIndex)]->GetInspectFields();
  bool changed = refreshedFields.size() != currentFields.size();
  if (!changed)
  {
    for (size_t i = 0; i < refreshedFields.size(); ++i)
    {
      const std::shared_ptr<InspectField>& refreshed = refreshedFields[i];
      const std::shared_ptr<InspectField>& existing = currentFields[i];
      const QString refreshedId = refreshed ? refreshed->fieldId() : QString();
      const QString existingId = existing ? existing->fieldId() : QString();
      if (refreshedId != existingId)
      {
        changed = true;
        break;
      }
    }
  }

  if (!changed)
  {
    return;
  }

  currentFields = std::move(refreshedFields);
  RebuildFieldObjects();
  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
}

void QTSceneInspector::SetProviders(const std::vector<InspectProvider*>& newProviders)
{
  if (newProviders.size() == providers.size())
  {
    bool same = true;
    for (size_t i = 0; i < newProviders.size(); ++i)
    {
      if (newProviders[i] != providers[i])
      {
        same = false;
        break;
      }
    }

    if (same)
    {
      return;
    }
  }

  providers = newProviders;

  providerNames.clear();
  providerNames.reserve(static_cast<qsizetype>(providers.size()));
  for (InspectProvider* provider : providers)
  {
    providerNames.push_back(provider ? QString::fromStdString(provider->GetInspectDisplayName()) : QString());
  }

  emit objectNamesChanged();

  const int previousIndex = selectedIndex;
  if (providers.empty())
  {
    selectedIndex = -1;
    currentFields.clear();
  }
  else if (selectedIndex < 0 || selectedIndex >= static_cast<int>(providers.size()))
  {
    selectedIndex = 0;
    currentFields = providers.front()->GetInspectFields();
  }
  else
  {
    currentFields = providers[static_cast<size_t>(selectedIndex)]->GetInspectFields();
  }

  RebuildFieldObjects();

  if (previousIndex != selectedIndex)
  {
    emit selectedObjectIndexChanged();
  }

  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
}

QVariantMap QTSceneInspector::fieldMeta(const QString& fieldId) const
{
  QVariantMap meta;

  const std::shared_ptr<InspectField> field = FindField(fieldId);
  if (field)
  {
    meta = field->meta();
  }

  return meta;
}

QVariant QTSceneInspector::fieldValue(const QString& fieldId) const
{
  const std::shared_ptr<InspectField> field = FindField(fieldId);
  if (!field)
  {
    return QVariant();
  }

  return field->value();
}

bool QTSceneInspector::setFieldValue(const QString& fieldId, const QVariant& value)
{
  const std::shared_ptr<InspectField> field = FindField(fieldId);
  if (!field)
  {
    return false;
  }

  field->setValue(value);
  fieldSnapshots[field->fieldId()] = field->value();
  ++revision;
  emit fieldRevisionChanged();
  return true;
}

void QTSceneInspector::RebuildFieldObjects()
{
  fieldObjects.clear();
  fieldSnapshots.clear();
  for (const std::shared_ptr<InspectField>& field : currentFields)
  {
    if (!field)
    {
      continue;
    }

    if (QCoreApplication::instance() && field->thread() != QCoreApplication::instance()->thread())
    {
      field->moveToThread(QCoreApplication::instance()->thread());
    }

    fieldObjects.push_back(field.get());
    fieldSnapshots.insert(field->fieldId(), field->value());
  }
}

void QTSceneInspector::SyncSnapshots()
{
  for (const std::shared_ptr<InspectField>& field : currentFields)
  {
    if (!field)
    {
      continue;
    }

    const QVariant currentValue = field->value();
    const QVariant previousValue = fieldSnapshots.value(field->fieldId());
    if (currentValue != previousValue)
    {
      fieldSnapshots.insert(field->fieldId(), currentValue);
      field->notifyValueChanged();
      emit fieldValueChanged(field->fieldId(), currentValue);
      ++revision;
      emit fieldRevisionChanged();
    }
  }
}

std::shared_ptr<InspectField> QTSceneInspector::FindField(const QString& fieldId) const
{
  for (const std::shared_ptr<InspectField>& field : currentFields)
  {
    if (field && field->fieldId() == fieldId)
    {
      return field;
    }
  }
  return nullptr;
}
