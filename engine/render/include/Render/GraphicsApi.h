#pragma once

// Graphics-API function-pointer resolver supplied by the windowing layer
// (signature-compatible with GLADloadproc).
using GraphicsProcLoader = void* (*)(const char*);

// Loads the graphics-API entry points for the current context. Must run once,
// with a current context, before any render resource is created.
bool InitializeGraphicsApi(GraphicsProcLoader loadProc);
