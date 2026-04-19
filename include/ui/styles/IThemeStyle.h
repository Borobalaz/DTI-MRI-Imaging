#pragma once

#include <QString>

class IThemeStyle
{
public:
  virtual ~IThemeStyle() = default;

  virtual QString styleSheet() const = 0;
};
