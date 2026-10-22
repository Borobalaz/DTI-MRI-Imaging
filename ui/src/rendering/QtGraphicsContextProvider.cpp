#include "rendering/QtGraphicsContextProvider.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QtGlobal>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

QtGraphicsContextProvider::QtGraphicsContextProvider()
  : surfaceFormat(QSurfaceFormat::defaultFormat())
{
  if (!QOpenGLContext::supportsThreadedOpenGL())
  {
    qWarning("QtGraphicsContextProvider: the platform reports no support for threaded OpenGL; "
             "rendering on the engine thread may fail.");
  }

  surface = std::make_unique<QOffscreenSurface>();
  surface->setFormat(surfaceFormat);
  surface->create();
  if (!surface->isValid())
  {
    qWarning("QtGraphicsContextProvider: failed to create the offscreen surface.");
  }
}

// Runs on the GUI thread after the engine thread has been joined, so the context is gone already.
QtGraphicsContextProvider::~QtGraphicsContextProvider() = default;

bool QtGraphicsContextProvider::CreateContext()
{
  if (!surface || !surface->isValid())
  {
    return false;
  }

  context = std::make_unique<QOpenGLContext>();
  context->setFormat(surfaceFormat);
  if (!context->create())
  {
    qWarning("QtGraphicsContextProvider: failed to create the OpenGL context.");
    context.reset();
    return false;
  }

#if defined(Q_OS_WIN)
  // Qt loads opengl32.dll while creating a desktop GL context; only look it up, don't add a reference.
  openGl32Module = reinterpret_cast<void*>(GetModuleHandleW(L"opengl32.dll"));
#endif

  return true;
}

bool QtGraphicsContextProvider::MakeCurrent()
{
  return context && context->makeCurrent(surface.get());
}

void QtGraphicsContextProvider::DoneCurrent()
{
  if (context)
  {
    context->doneCurrent();
  }
}

void* QtGraphicsContextProvider::GetProcAddress(const char* name)
{
#if defined(Q_OS_WIN)
  if (openGl32Module)
  {
    if (FARPROC procedure = ::GetProcAddress(static_cast<HMODULE>(openGl32Module), name))
    {
      return reinterpret_cast<void*>(procedure);
    }
  }
#endif

  if (!context)
  {
    return nullptr;
  }

  return reinterpret_cast<void*>(context->getProcAddress(name));
}

void QtGraphicsContextProvider::DestroyContext()
{
  context.reset();
  openGl32Module = nullptr;
}
