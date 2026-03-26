#pragma once

#include "Gui/Inspectable.h"

class IWidget
{
public:
  virtual ~IWidget() = default;

  virtual bool Render(const UiField& field, UiFieldValue& value) const = 0;
};
