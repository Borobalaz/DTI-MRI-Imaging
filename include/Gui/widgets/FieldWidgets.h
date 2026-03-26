#pragma once

#include "Gui/widgets/IWidget.h"

class BoolFieldWidget : public IWidget
{
public:
  bool Render(const UiField& field, UiFieldValue& value) const override;
};

class IntFieldWidget : public IWidget
{
public:
  bool Render(const UiField& field, UiFieldValue& value) const override;
};

class FloatFieldWidget : public IWidget
{
public:
  bool Render(const UiField& field, UiFieldValue& value) const override;
};

class Vec3FieldWidget : public IWidget
{
public:
  bool Render(const UiField& field, UiFieldValue& value) const override;
};

class Color3FieldWidget : public IWidget
{
public:
  bool Render(const UiField& field, UiFieldValue& value) const override;
};
