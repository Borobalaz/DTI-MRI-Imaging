#pragma once

#include <memory>
#include <string>

#include "GameObject.h"
#include "Shader.h"

class ModelLoader
{
public:
  static std::shared_ptr<GameObject> LoadGameObject(
    const std::string& modelPath,
    const std::shared_ptr<Shader>& shader,
    bool flipUv = false
  );
};
