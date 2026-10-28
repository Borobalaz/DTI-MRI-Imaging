#pragma once

#include <memory>

#include <QSurfaceFormat>

#include "graphics/IGraphicsContextProvider.h"

class QOffscreenSurface;
class QOpenGLContext;

/**
 * @brief Supplies the engine's render thread with a Qt-created OpenGL context.
 *
 * Construct and destroy on the GUI thread, after the event loop is running: the
 *  QOffscreenSurface is created in the constructor because Qt requires surfaces to be created
 *  on the GUI thread. Every IGraphicsContextProvider method runs on the engine thread, so the
 *  QOpenGLContext is created, made current and destroyed there, and is never shared with
 *  another context.
 */
class QtGraphicsContextProvider : public IGraphicsContextProvider
{
public:
  QtGraphicsContextProvider();
  ~QtGraphicsContextProvider() override;

  QtGraphicsContextProvider(const QtGraphicsContextProvider&) = delete;
  QtGraphicsContextProvider& operator=(const QtGraphicsContextProvider&) = delete;

  bool CreateContext() override;
  bool MakeCurrent() override;
  void DoneCurrent() override;
  void* GetProcAddress(const char* name) override;
  void DestroyContext() override;

private:
  // Captured on the GUI thread so the engine thread never reads Qt's global default format.
  QSurfaceFormat surfaceFormat;
  std::unique_ptr<QOffscreenSurface> surface;
  std::unique_ptr<QOpenGLContext> context;

  // opengl32.dll handle (Windows only): wglGetProcAddress returns null for GL 1.1 entry points,
  //  which are only exported from opengl32.dll itself.
  void* openGl32Module = nullptr;
};
