#include "Preprocessing/io/PreprocessingDatasetFormatRegistry.h"

void PreprocessingDatasetFormatRegistry::RegisterWriter(std::shared_ptr<IPreprocessingDatasetWriter> writer)
{
  if (!writer)
  {
    return;
  }

  writersByType[writer->PreprocessorType()] = std::move(writer);
}

void PreprocessingDatasetFormatRegistry::RegisterReader(std::shared_ptr<IPreprocessingDatasetReader> reader)
{
  if (!reader)
  {
    return;
  }

  readersByType[reader->PreprocessorType()] = std::move(reader);
}

std::shared_ptr<IPreprocessingDatasetWriter> PreprocessingDatasetFormatRegistry::FindWriter(
    const std::string &preprocessorType) const
{
  const auto found = writersByType.find(preprocessorType);
  return found == writersByType.end() ? nullptr : found->second;
}

std::shared_ptr<IPreprocessingDatasetReader> PreprocessingDatasetFormatRegistry::FindReader(
    const std::string &preprocessorType) const
{
  const auto found = readersByType.find(preprocessorType);
  return found == readersByType.end() ? nullptr : found->second;
}
