#include "ui/qt-adapters/QTSceneInspector.h"

#include <sstream>

#include <QCoreApplication>
#include <QHash>
#include "ui/widgets/inspect_fields/InspectProvider.h"
#include "ui/widgets/inspect_fields/IInspectWidget.h"

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

  std::vector<std::shared_ptr<IInspectWidget>> refreshedFields = providers[static_cast<size_t>(selectedIndex)]->GetInspectFields();
  bool changed = refreshedFields.size() != currentFields.size();
  if (!changed)
  {
    for (size_t i = 0; i < refreshedFields.size(); ++i)
    {
      const std::shared_ptr<IInspectWidget>& refreshed = refreshedFields[i];
      const std::shared_ptr<IInspectWidget>& existing = currentFields[i];
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

  const std::shared_ptr<IInspectWidget> field = FindField(fieldId);
  if (field)
  {
    meta = field->meta();
  }

  return meta;
}

QVariant QTSceneInspector::fieldValue(const QString& fieldId) const
{
  const std::shared_ptr<IInspectWidget> field = FindField(fieldId);
  if (!field)
  {
    return QVariant();
  }

  return field->GetValue();
}

bool QTSceneInspector::setFieldValue(const QString& fieldId, const QVariant& value)
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

void QTSceneInspector::RebuildFieldObjects()
{
  fieldObjects.clear();
  fieldSnapshots.clear();
  for (const std::shared_ptr<IInspectWidget>& field : currentFields)
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
  for (const std::shared_ptr<IInspectWidget>& field : currentFields)
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

std::shared_ptr<IInspectWidget> QTSceneInspector::FindField(const QString& fieldId) const
{
  for (const std::shared_ptr<IInspectWidget>& field : currentFields)
  {
    if (field && field->fieldId() == fieldId)
    {
      return field;
    }
  }
  return nullptr;
}

