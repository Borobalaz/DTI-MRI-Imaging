#pragma once

#include "ui/styles/IThemeStyle.h"

class LightThemeStyle : public IThemeStyle
{
public:
  QString styleSheet() const override;
};
