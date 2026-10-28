#pragma once

// Deferred deletion of GPU objects. Destructors of GPU-owning render resources
// (textures, shaders, geometry, instance buffers, framebuffers) may run on any
// thread -- e.g. the UI thread dropping the last shared_ptr to an inspectable
// provider -- so they enqueue their handles here instead of calling the graphics
// API, and the GL thread deletes them in FlushGpuDeletions().
//
// The queue is process-global. That assumes one GL context per process, the same
// assumption glad's global function table already makes.

enum class GpuResourceKind
{
  Texture,
  Buffer,
  VertexArray,
  Program,
  Framebuffer
};

// Any thread. Ignores handle 0, and drops the handle when the queue is not
// accepting (no live context to delete it in).
void EnqueueGpuDeletion(GpuResourceKind kind, unsigned int handle);

// GL thread only, with the context current: deletes everything queued so far.
void FlushGpuDeletions();

// Enabled when the graphics API is initialized, disabled at shutdown so late
// destructors (after the context is gone) don't grow the queue forever.
void SetGpuDeletionAccepting(bool accepting);
