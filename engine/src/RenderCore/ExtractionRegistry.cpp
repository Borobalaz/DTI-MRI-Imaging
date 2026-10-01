#include "RenderCore/ExtractionRegistry.h"

#include <algorithm>

RegistrationToken ExtractionRegistry::RegisterExtractor(std::unique_ptr<IRenderExtractor> extractor,
                                                        ExtractorRegistrationInfo info)
{
  if (!extractor)
  {
    return 0;
  }

  Entry entry;
  entry.token = nextToken++;
  entry.extractor = std::move(extractor);
  entry.info = std::move(info);
  entry.insertionOrder = nextInsertionOrder++;
  entries.push_back(std::move(entry));
  return entries.back().token;
}

void ExtractionRegistry::ExtractFor(const RendererDescriptor& renderer,
                                    const SceneSnapshot& scene,
                                    RenderExtractionContext& context,
                                    RenderDataStore& output) const
{
  std::vector<const Entry*> selectedEntries;
  selectedEntries.reserve(entries.size());
  for (const Entry& entry : entries)
  {
    if (!entry.info.enabled || !entry.extractor || !entry.extractor->Supports(renderer))
    {
      continue;
    }

    selectedEntries.push_back(&entry);
  }

  std::stable_sort(selectedEntries.begin(), selectedEntries.end(),
    [](const Entry* left, const Entry* right)
    {
      if (left->info.phase != right->info.phase)
      {
        return static_cast<int>(left->info.phase) < static_cast<int>(right->info.phase);
      }

      if (left->info.priority != right->info.priority)
      {
        return left->info.priority > right->info.priority;
      }

      return left->insertionOrder < right->insertionOrder;
    });

  for (const Entry* entry : selectedEntries)
  {
    entry->extractor->Extract(scene, context, output);
  }
}
