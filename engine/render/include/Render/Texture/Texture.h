#pragma once

class Texture
{
  friend class RenderBackend;

public:
  virtual ~Texture() = default;

  virtual bool IsValid() const = 0;

private:
  // GPU-only: reachable through RenderBackend::Bind / RenderBackend::Handle.
  virtual void Bind(unsigned int unit) const = 0;
  virtual unsigned int GetId() const = 0;
};
