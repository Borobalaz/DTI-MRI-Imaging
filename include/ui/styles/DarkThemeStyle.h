#pragma once

#include "ui/styles/IThemeStyle.h"

class DarkThemeStyle : public IThemeStyle
{
public:
  QString styleSheet() const override;
};
