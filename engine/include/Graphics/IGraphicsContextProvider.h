#pragma once

/**
 * @brief Host-supplied source of the graphics (OpenGL) context the engine renders with in
 *  threaded mode (Engine::Start). Keeps the engine free of any windowing toolkit.
 *
 *  Threading contract: every method is invoked on the engine thread, never on the host's
 *  thread, and none of them may block waiting on the host UI thread (Engine::Start blocks the
 *  caller until initialization finishes, so doing so would deadlock). Anything that must be
 *  created on the host UI thread (e.g. an offscreen surface) has to exist before Start().
 *
 *  Call order: CreateContext -> MakeCurrent -> GetProcAddress (during API loading) -> ...
 *  -> DoneCurrent -> DestroyContext. The engine never shares the context with another one.
 */
class IGraphicsContextProvider
{
public:
  virtual ~IGraphicsContextProvider() = default;

  // Creates the context (not yet current). Returns false on failure.
  virtual bool CreateContext() = 0;

  // Makes the context current on the calling (engine) thread. Returns false on failure.
  virtual bool MakeCurrent() = 0;

  // Releases the context from the calling (engine) thread.
  virtual void DoneCurrent() = 0;

  // Resolves a graphics-API entry point by name; nullptr when unavailable.
  // Called only while the context is current.
  virtual void* GetProcAddress(const char* name) = 0;

  // Destroys the context. Called after DoneCurrent().
  virtual void DestroyContext() = 0;
};
