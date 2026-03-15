#pragma once

#include <string>

#include "Texture.h"

class Texture2D : public Texture
{
public:
  explicit Texture2D(const std::string& path, bool flipVertically = true);
  ~Texture2D() override;

  void Bind(unsigned int unit) const override;
  bool IsValid() const override;
  unsigned int GetId() const override;

private:
  unsigned int id;
  bool valid;
};
