#pragma once

#include <string>

#include "render/texture/Texture.h"

struct SkyboxFaces
{
  std::string right;
  std::string left;
  std::string top;
  std::string bottom;
  std::string front;
  std::string back;
};

class TextureCube : public Texture
{
public:
  explicit TextureCube(const SkyboxFaces& faces);
  ~TextureCube() override;

  bool IsValid() const override;

private:
  void Bind(unsigned int unit) const override;
  unsigned int GetId() const override;

  bool UploadFace(unsigned int target,
                  const std::string& faceName,
                  const std::string& path,
                  int& expectedWidth,
                  int& expectedHeight);

  unsigned int id;
  bool valid;
};