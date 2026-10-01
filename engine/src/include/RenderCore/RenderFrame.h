#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "RenderCore/RenderDataStore.h"

struct RenderView
{
  std::string name;
};

struct FrameMetadata
{
  std::uint64_t frameIndex = 0;
};

struct RenderFrame
{
  RenderDataStore data;
  std::vector<RenderView> views;
  FrameMetadata metadata;
};
