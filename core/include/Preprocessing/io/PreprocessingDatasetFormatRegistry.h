#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "Preprocessing/io/IPreprocessingDatasetReader.h"
#include "Preprocessing/io/IPreprocessingDatasetWriter.h"

/**
 * @brief Maps a preprocessorType string to the writer/reader that knows how to save/load that
 *        preprocessor's results. Adding a new preprocessor type never requires changing this
 *        class - only registering a new writer/reader (see PreprocessingDatasetFormats.cpp).
 */
class PreprocessingDatasetFormatRegistry
{
public:
  void RegisterWriter(std::shared_ptr<IPreprocessingDatasetWriter> writer);
  void RegisterReader(std::shared_ptr<IPreprocessingDatasetReader> reader);

  std::shared_ptr<IPreprocessingDatasetWriter> FindWriter(const std::string &preprocessorType) const;
  std::shared_ptr<IPreprocessingDatasetReader> FindReader(const std::string &preprocessorType) const;

private:
  std::unordered_map<std::string, std::shared_ptr<IPreprocessingDatasetWriter>> writersByType;
  std::unordered_map<std::string, std::shared_ptr<IPreprocessingDatasetReader>> readersByType;
};
