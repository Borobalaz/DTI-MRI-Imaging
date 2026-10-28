#pragma once

// Graphics-API function-pointer resolver supplied by the windowing layer
// (signature-compatible with GLADloadproc).
using GraphicsProcLoader = void* (*)(const char*);

// Loads the graphics-API entry points for the current context. Must run once,
// with a current context, before any render resource is created.
bool InitializeGraphicsApi(GraphicsProcLoader loadProc);

// GL thread, context current: deletes the GPU objects whose owners were destroyed
// since the last call (render-resource destructors only queue their handles, since
// they may run on any thread). Call once per frame, before drawing.
void CollectGraphicsGarbage();

// GL thread, context current, after every render resource owned by the caller has
// been destroyed: flushes the remaining garbage and stops queueing deletions, since
// handles freed after this point have no context left to be deleted in.
void ShutdownGraphicsApi();
