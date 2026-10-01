#pragma once

#include "styles/IThemeStyle.h"

class LightThemeStyle : public IThemeStyle
{
public:
  QString styleSheet() const override;
};
