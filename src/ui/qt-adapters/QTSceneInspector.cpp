#include "ui/qt-adapters/QTSceneInspector.h"

#include <sstream>

#include <QCoreApplication>
#include <QHash>
#include "ui/widgets/inspect_fields/InspectProvider.h"
#include "ui/widgets/inspect_fields/IInspectWidget.h"
#include "ui/widgets/inspect_fields/InspectCheckboxFieldWidget.h"

QTSceneInspector::QTSceneInspector(QObject *parent)
    : QObject(parent)
{
  syncTimer.setInterval(33);
  syncTimer.setTimerType(Qt::PreciseTimer);
  connect(&syncTimer, &QTimer::timeout, this, [this]()
          { SyncSnapshots(); });
  syncTimer.start();
}

std::string QTSceneInspector::selectedObjectName() const
{
  return selectedProviderName;
}

/**
 * @brief Set the selected object to be the one with the given name.
 *  If no object has the name, selection will be cleared.
 *
 * @param name
 */
void QTSceneInspector::setSelectedObjectName(const std::string &name)
{
  if (name == selectedProviderName)
  {
    return;
  }

  // Find the provider that has name matching the input name
  bool found = false;
  for (auto provider : providers)
  {
    if (provider && provider->GetInspectDisplayName() == name)
    {
      found = true;
      selectedProviderName = name;
      currentFields = provider->GetInspectFields();
      break;
    }
  }

  if (!found)
  {
    // Clear selection if no provider has the input name
    selectedProviderName = "";
    currentFields.clear();
  }

  // Build the field objects for the newly selected provider
  RebuildFieldObjects();

  // Change signal emit
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

/**
 * @brief Update the list of inspectable providers coming from the scene.
 * 
 * @param newProviders 
 */
void QTSceneInspector::Update(const std::vector<InspectProvider *> &newProviders)
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

  if (selectedProviderName.empty())
  {
    return;
  }

  InspectProvider *selectedProvider = FindProviderByName(selectedProviderName);
  if (!selectedProvider)
  {
    selectedProviderName.clear();
    currentFields.clear();
    RebuildFieldObjects();
    emit selectedProviderIndexChanged();
    emit fieldsChanged();
    ++revision;
    emit fieldRevisionChanged();
    return;
  }

  std::vector<std::shared_ptr<IInspectWidget>> refreshedFields = selectedProvider->GetInspectFields();
  bool changed = refreshedFields.size() != currentFields.size();
  if (!changed)
  {
    for (size_t i = 0; i < refreshedFields.size(); ++i)
    {
      const std::shared_ptr<IInspectWidget> &refreshed = refreshedFields[i];
      const std::shared_ptr<IInspectWidget> &existing = currentFields[i];
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

/**
 * @brief Set the list of inspectable providers coming from the scene, replacing the existing list. 
 *  This is used when the provider list has changed (e.g. from scene updates).
 * 
 * @param newProviders 
 */
void QTSceneInspector::SetProviders(const std::vector<InspectProvider *> &newProviders)
{
  const std::string previousSelection = selectedProviderName;
  providers = newProviders;

  emit providersChanged();

  if (providers.empty())
  {
    selectedProviderName.clear();
    currentFields.clear();
  }
  else
  {
    InspectProvider *selectedProvider = nullptr;
    if (!selectedProviderName.empty())
    {
      selectedProvider = FindProviderByName(selectedProviderName);
    }

    if (!selectedProvider)
    {
      selectedProvider = providers.front();
      selectedProviderName = selectedProvider ? selectedProvider->GetInspectDisplayName() : std::string();
    }

    currentFields = selectedProvider ? selectedProvider->GetInspectFields() : std::vector<std::shared_ptr<IInspectWidget>>{};
  }

  RebuildFieldObjects();

  if (previousSelection != selectedProviderName)
  {
    emit selectedProviderIndexChanged();
  }

  emit fieldsChanged();
  ++revision;
  emit fieldRevisionChanged();
  emit visibilityStateChanged();
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

bool QTSceneInspector::hasVisibility(const std::string &providerName) const
{
  InspectProvider *provider = FindProviderByName(providerName);
  if (!provider)
  {
    return false;
  }

  return provider->HasVisibility();
}

bool QTSceneInspector::isVisible(const std::string &providerName) const
{
  InspectProvider *provider = FindProviderByName(providerName);
  if (!provider)
  {
    return false;
  }

  return provider->IsVisible();
}

bool QTSceneInspector::setVisible(const std::string &providerName, bool visible)
{
  InspectProvider *provider = FindProviderByName(providerName);
  if (!provider)
  {
    return false;
  }

  if (!provider->HasVisibility())
  {
    return false;
  }

  const std::shared_ptr<IInspectWidget> visibilityField = FindVisibilityField(provider->GetInspectFields());
  if (!visibilityField)
  {
    return false;
  }

  // Programmatic SetValue on checkbox fields does not emit user callbacks,
  // so invoke the callback path explicitly to mutate the provider state.
  if (std::shared_ptr<InspectCheckboxFieldWidget> visibilityCheckbox =
          std::dynamic_pointer_cast<InspectCheckboxFieldWidget>(visibilityField))
  {
    if (visibilityCheckbox->valueChangedCallback)
    {
      visibilityCheckbox->valueChangedCallback(visible);
    }
  }

  visibilityField->SetValue(visible);

  if (selectedProviderName == providerName)
  {
    const std::shared_ptr<IInspectWidget> selectedField = FindField(visibilityField->fieldId());
    if (selectedField)
    {
      selectedField->SetValue(visible);
      fieldSnapshots[selectedField->fieldId()] = selectedField->GetValue();
    }
  }

  ++revision;
  emit fieldRevisionChanged();
  emit visibilityStateChanged();
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

InspectProvider *QTSceneInspector::FindProviderByName(const std::string &name) const
{
  for (InspectProvider *provider : providers)
  {
    if (provider && provider->GetInspectDisplayName() == name)
    {
      return provider;
    }
  }
  return nullptr;
}

std::shared_ptr<IInspectWidget> QTSceneInspector::FindVisibilityField(const std::vector<std::shared_ptr<IInspectWidget>> &fields) const
{
  for (const std::shared_ptr<IInspectWidget> &field : fields)
  {
    if (!field)
    {
      continue;
    }

    const QString id = field->fieldId();
    if (id == "visible" || id == "isVisible")
    {
      return field;
    }
  }
  return nullptr;
}
