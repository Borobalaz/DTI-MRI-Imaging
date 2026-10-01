#pragma once

#include <memory>
#include <string>
#include <vector>

#include "RenderCore/Extraction.h"

using RegistrationToken = std::size_t;

enum class ExtractionPhase
{
  Views = 0,
  Geometry = 1,
  Lighting = 2,
  Volumes = 3,
  Effects = 4,
  Debug = 5
};

struct ExtractorRegistrationInfo
{
  std::string debugName;
  ExtractionPhase phase = ExtractionPhase::Geometry;
  int priority = 0;
  bool enabled = true;
};

class ExtractionRegistry
{
public:
  RegistrationToken RegisterExtractor(std::unique_ptr<IRenderExtractor> extractor,
                                      ExtractorRegistrationInfo info);

  void ExtractFor(const RendererDescriptor& renderer,
                  const SceneSnapshot& scene,
                  RenderExtractionContext& context,
                  RenderDataStore& output) const;

private:
  struct Entry
  {
    RegistrationToken token = 0;
    std::unique_ptr<IRenderExtractor> extractor;
    ExtractorRegistrationInfo info;
    std::size_t insertionOrder = 0;
  };

  std::vector<Entry> entries;
  RegistrationToken nextToken = 1;
  std::size_t nextInsertionOrder = 0;
};
