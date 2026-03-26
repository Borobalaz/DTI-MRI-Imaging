#include "Gui/widgets/FieldWidgets.h"

#include <glm/glm.hpp>
#include <imgui.h>

bool BoolFieldWidget::Render(const UiField& field, UiFieldValue& value) const
{
  bool currentValue = std::holds_alternative<bool>(value) ? std::get<bool>(value) : false;
  const bool changed = ImGui::Checkbox("##value", &currentValue);
  if (changed)
  {
    value = currentValue;
  }

  (void)field;
  return changed;
}

bool IntFieldWidget::Render(const UiField& field, UiFieldValue& value) const
{
  int currentValue = std::holds_alternative<int>(value) ? std::get<int>(value) : 0;
  const bool changed = ImGui::SliderInt("##value", &currentValue, field.minInt, field.maxInt);
  if (changed)
  {
    value = currentValue;
  }

  return changed;
}

bool FloatFieldWidget::Render(const UiField& field, UiFieldValue& value) const
{
  float currentValue = std::holds_alternative<float>(value) ? std::get<float>(value) : 0.0f;
  const bool changed = ImGui::SliderFloat("##value", &currentValue, field.minFloat, field.maxFloat);
  if (changed)
  {
    value = currentValue;
  }

  return changed;
}

bool Vec3FieldWidget::Render(const UiField& field, UiFieldValue& value) const
{
  glm::vec3 currentValue = std::holds_alternative<glm::vec3>(value) ? std::get<glm::vec3>(value) : glm::vec3(0.0f);
  const bool changed = ImGui::DragFloat3("##value", &currentValue.x, field.speed);
  if (changed)
  {
    value = currentValue;
  }

  return changed;
}

bool Color3FieldWidget::Render(const UiField& field, UiFieldValue& value) const
{
  glm::vec3 currentValue = std::holds_alternative<glm::vec3>(value) ? std::get<glm::vec3>(value) : glm::vec3(0.0f);
  const bool changed = ImGui::ColorEdit3("##value", &currentValue.x);
  if (changed)
  {
    value = currentValue;
  }

  (void)field;
  return changed;
}
