#include "OpenGL/GpuDeletionQueue.h"

#include <mutex>
#include <utility>
#include <vector>

#include <glad/glad.h>

namespace
{
  struct PendingGpuDeletion
  {
    GpuResourceKind kind;
    unsigned int handle;
  };

  struct GpuDeletionQueueState
  {
    std::mutex mutex;
    std::vector<PendingGpuDeletion> pending;
    bool accepting = true;
  };

  GpuDeletionQueueState& QueueState()
  {
    static GpuDeletionQueueState state;
    return state;
  }

  void DeleteGpuObject(const PendingGpuDeletion& deletion)
  {
    const GLuint handle = deletion.handle;
    switch (deletion.kind)
    {
      case GpuResourceKind::Texture:
        glDeleteTextures(1, &handle);
        break;
      case GpuResourceKind::Buffer:
        glDeleteBuffers(1, &handle);
        break;
      case GpuResourceKind::VertexArray:
        glDeleteVertexArrays(1, &handle);
        break;
      case GpuResourceKind::Program:
        glDeleteProgram(handle);
        break;
      case GpuResourceKind::Framebuffer:
        glDeleteFramebuffers(1, &handle);
        break;
    }
  }
}

void EnqueueGpuDeletion(GpuResourceKind kind, unsigned int handle)
{
  if (handle == 0)
  {
    return;
  }

  GpuDeletionQueueState& state = QueueState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (state.accepting)
  {
    state.pending.push_back({kind, handle});
  }
}

void FlushGpuDeletions()
{
  std::vector<PendingGpuDeletion> deletions;
  {
    GpuDeletionQueueState& state = QueueState();
    std::lock_guard<std::mutex> lock(state.mutex);
    deletions.swap(state.pending);
  }

  for (const PendingGpuDeletion& deletion : deletions)
  {
    DeleteGpuObject(deletion);
  }
}

void SetGpuDeletionAccepting(bool accepting)
{
  GpuDeletionQueueState& state = QueueState();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.accepting = accepting;
  if (!accepting)
  {
    state.pending.clear();
  }
}
