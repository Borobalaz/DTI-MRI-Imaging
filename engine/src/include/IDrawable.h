#pragma once

#include "Renderer/RenderProxy.h"

class IDrawable
{
public:
  virtual ~IDrawable() = default;

  /**
   * @brief Build a proxy object for the renderer to consume. 
   *        The proxy should contain all information necessary for the renderer to draw the drawable.
   * 
   * @param renderProxy 
   */
  virtual void BuildRenderProxy(RenderProxy& renderProxy) const = 0;
  
  bool visible = true;
  void SetVisible(bool isVisible) { visible = isVisible; }

};