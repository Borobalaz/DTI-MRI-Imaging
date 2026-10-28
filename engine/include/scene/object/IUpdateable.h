#pragma once

#include <functional>
#include <utility>

class IUpdateable
{
public:
  virtual ~IUpdateable() = default;

  void SetUpdate(std::function<void(float)> updateFunction)
  {
    this->updateFunction = std::move(updateFunction);
  }

  virtual void Update(float deltaTime)
  {
    if (updateFunction)
    {
      updateFunction(deltaTime);
    }
  }

private:
  std::function<void(float)> updateFunction;
};
