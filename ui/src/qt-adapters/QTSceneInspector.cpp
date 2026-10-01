#include "qt-adapters/QTSceneInspector.h"

#include <limits>
#include <sstream>

#include <QCoreApplication>
#include <QHash>
#include "widgets/inspect_fields/InspectColorFieldWidget.h"
#include "widgets/inspect_fields/InspectNumberFieldWidget.h"
#include "widgets/inspect_fields/InspectTextFieldWidget.h"
#include "widgets/inspect_fields/InspectVec3FieldWidget.h"
#include "widgets/inspect_fields/InspectProvider.h"
#include "widgets/inspect_fields/IInspectWidget.h"
#include "widgets/inspect_fields/InspectCheckboxFieldWidget.h"
#include "widgets/inspect_fields/InspectActionFieldWidget.h"
#include "widgets/inspect_fields/InspectDropdownFieldWidget.h"
#include "widgets/inspect_fields/InspectFilePickerWidget.h"

namespace
{
QVariant ToVariant(const InspectValue& value)
{
  if (const auto* boolean = std::get_if<bool>(&value)) return *boolean;
  if (const auto* integer = std::get_if<int>(&value)) return *integer;
  if (const auto* number = std::get_if<double>(&value)) return *number;
  if (const auto* text = std::get_if<std::string>(&value)) return QString::fromStdString(*text);
  if (const auto* vector = std::get_if<glm::vec3>(&value)) return QVariantList{vector->x, vector->y, vector->z};
  return {};
}

InspectValue ToInspectValue(const QVariant& value, InspectFieldType type)
{
  switch (type)
  {
    case InspectFieldType::Boolean: return value.toBool();
    case InspectFieldType::Number: return value.toDouble();
    case InspectFieldType::Color:
    case InspectFieldType::Vector3:
    {
      const QVariantList list = value.toList();
      if (list.size() >= 3)
        return glm::vec3(static_cast<float>(list[0].toDouble()), static_cast<float>(list[1].toDouble()), static_cast<float>(list[2].toDouble()));
      return glm::vec3(0.0f);
    }
    default: return value.toString().toStdString();
  }
}

class QtInspectFieldAdapter : public QObject, public IInspectWidget
{
public:
  explicit QtInspectFieldAdapter(InspectFieldPtr engineField)
    : field(std::move(engineField))
  {
    const QString id = QString::fromStdString(field->id);
    const QString name = QString::fromStdString(field->displayName);
    const QString group = QString::fromStdString(field->groupName);

    switch (field->type)
    {
      case InspectFieldType::Boolean:
      {
        auto checkbox = std::make_shared<InspectCheckboxFieldWidget>(id, name, group, field->readOnly);
        checkbox->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
        widget = std::move(checkbox);
        break;
      }
      case InspectFieldType::Number:
      {
        auto number = std::make_shared<InspectNumberFieldWidget>(id, name, group,
          [this]() { return std::get<double>(field->GetValue()); },
          [this](double value) { field->SetValue(value); },
          field->minimum, field->maximum, field->step, field->readOnly);
        widget = std::move(number);
        break;
      }
      case InspectFieldType::Dropdown:
      {
        QStringList options;
        for (const std::string& option : field->enumOptions)
          options.push_back(QString::fromStdString(option));
        auto dropdown = std::make_shared<InspectDropdownFieldWidget>(id, name, group, options, field->readOnly);
        dropdown->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
        widget = std::move(dropdown);
        break;
      }
      case InspectFieldType::File:
      {
        auto file = std::make_shared<InspectFilePickerWidget>(id, name, group, QStringLiteral("Select file"), QStringLiteral("All files (*.*)"), field->readOnly);
        file->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
        widget = std::move(file);
        break;
      }
      case InspectFieldType::Action:
      {
        auto action = std::make_shared<InspectActionFieldWidget>(id, name, group);
        action->actionCallback = [this]() { field->SetValue(std::monostate{}); };
        widget = std::move(action);
        break;
      }
      case InspectFieldType::Color:
      {
        auto color = std::make_shared<InspectColorFieldWidget>(id, name, group, field->readOnly);
        color->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
        widget = std::move(color);
        break;
      }
      case InspectFieldType::Vector3:
      {
        auto vector = std::make_shared<InspectVec3FieldWidget>(id, name, group, field->readOnly);
        vector->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
        widget = std::move(vector);
        break;
      }
      default:
      {
        auto text = std::make_shared<InspectTextFieldWidget>(id, name, group, field->readOnly);
        text->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
        widget = std::move(text);
        break;
      }
    }

    widget->SetValue(ToVariant(field->GetValue()));
  }

  QString fieldId() const override { return QString::fromStdString(field->id); }
  QString displayName() const override { return QString::fromStdString(field->displayName); }
  QString groupName() const override { return QString::fromStdString(field->groupName); }
  bool isReadOnly() const override { return field->readOnly; }
  double minimum() const override { return field->minimum; }
  double maximum() const override { return field->maximum; }
  IInspectWidget* addToLayout(QHBoxLayout* layout) override { return widget->addToLayout(layout); }
  void SetValue(const QVariant& value) override
  {
    field->SetValue(ToInspectValue(value, field->type));
    widget->SetValue(value);
  }
  QVariant GetValue() const override { return ToVariant(field->GetValue()); }

private:
  InspectFieldPtr field;
  std::shared_ptr<IInspectWidget> widget;
};

std::vector<std::shared_ptr<IInspectWidget>> AdaptFields(const std::vector<InspectFieldPtr>& fields)
{
  std::vector<std::shared_ptr<IInspectWidget>> adapted;
  adapted.reserve(fields.size());
  for (const InspectFieldPtr& field : fields)
  {
    if (field) adapted.push_back(std::make_shared<QtInspectFieldAdapter>(field));
  }
  return adapted;
}
}

QTSceneInspector::QTSceneInspector(QObject *parent)
    : QObject(parent)
{
  syncTimer.setInterval(33);
  syncTimer.setTimerType(Qt::PreciseTimer);
  connect(&syncTimer, &QTimer::timeout, this, [this]()
          { SyncSnapshots(); });
  syncTimer.start();
}

/**
 * @brief Attach the engine's inspection service and perform the initial provider sync.
 *  Subscribes to push-based provider-list-changed notifications instead of per-frame polling.
 *
 * @param service
 */
void QTSceneInspector::SetInspectionService(IInspectionService *service)
{
  session.SetService(service);

  if (service)
  {
    service->OnProvidersChanged([this]()
                                 { HandleProvidersChanged(); });
  }

  HandleProvidersChanged();
}

std::string QTSceneInspector::selectedObjectName() const
{
  return session.SelectedId();
}

/**
 * @brief Select the object with the given stable id. Clears selection if not found.
 *
 * @param id
 */
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

/**
 * @brief React to the engine's push notification that the provider list structurally changed.
 *  Re-validates the current selection and rebuilds the field list if needed.
 */
void QTSceneInspector::HandleProvidersChanged()
{
  const std::string previousSelection = session.SelectedId();

  emit providersChanged();

  const bool stillValid = !previousSelection.empty() && session.SelectById(previousSelection);
  if (!stillValid)
  {
    const std::vector<InspectObjectSummary> summaries = getProviders();
    if (!summaries.empty())
    {
      session.SelectById(summaries.front().id);
    }
    else
    {
      session.ClearSelection();
    }
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

/**
 * @brief Select an object in the scene by casting a ray from the given origin in the given direction.
 *        The closest intersecting object will be selected, via the engine's inspection service.
 *
 * @param rayOrigin
 * @param rayDirection
 * @return true
 * @return false
 */
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

