#include "qt-adapters/QtInspectFieldAdapter.h"

#include "widgets/inspect_fields/InspectColorFieldWidget.h"
#include "widgets/inspect_fields/InspectNumberFieldWidget.h"
#include "widgets/inspect_fields/InspectTextFieldWidget.h"
#include "widgets/inspect_fields/InspectVec3FieldWidget.h"
#include "widgets/inspect_fields/InspectCheckboxFieldWidget.h"
#include "widgets/inspect_fields/InspectActionFieldWidget.h"
#include "widgets/inspect_fields/InspectDropdownFieldWidget.h"
#include "widgets/inspect_fields/InspectFilePickerWidget.h"

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

QtInspectFieldAdapter::QtInspectFieldAdapter(InspectFieldPtr engineField)
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
    case InspectFieldType::Directory:
    {
      auto directory = std::make_shared<InspectFilePickerWidget>(id, name, group, QStringLiteral("Select folder"), QString(), field->readOnly, nullptr, /*pickDirectory=*/true);
      directory->valueChangedCallback = [this](const QVariant& value) { field->SetValue(ToInspectValue(value, field->type)); };
      widget = std::move(directory);
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

QString QtInspectFieldAdapter::fieldId() const { return QString::fromStdString(field->id); }
QString QtInspectFieldAdapter::displayName() const { return QString::fromStdString(field->displayName); }
QString QtInspectFieldAdapter::groupName() const { return QString::fromStdString(field->groupName); }
bool QtInspectFieldAdapter::isReadOnly() const { return field->readOnly; }
double QtInspectFieldAdapter::minimum() const { return field->minimum; }
double QtInspectFieldAdapter::maximum() const { return field->maximum; }
IInspectWidget* QtInspectFieldAdapter::addToLayout(QHBoxLayout* layout) { return widget->addToLayout(layout); }

void QtInspectFieldAdapter::SetValue(const QVariant& value)
{
  field->SetValue(ToInspectValue(value, field->type));
  widget->SetValue(value);
}

QVariant QtInspectFieldAdapter::GetValue() const { return ToVariant(field->GetValue()); }

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
