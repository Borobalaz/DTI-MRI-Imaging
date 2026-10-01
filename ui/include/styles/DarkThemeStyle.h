#pragma once

#include "styles/IThemeStyle.h"

class DarkThemeStyle : public IThemeStyle
{
public:
  QString styleSheet() const override;
};
