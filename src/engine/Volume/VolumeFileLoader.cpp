#include "VolumeFileLoader.h"

std::optional<LoadedVolumeVariant> VolumeFileLoader::Load(const std::string& filePath)
{
  if (const auto scalarU8 = LoadTyped<uint8_t>(filePath))
  {
    return LoadedVolumeVariant(*scalarU8);
  }

  if (const auto scalarU16 = LoadTyped<uint16_t>(filePath))
  {
    return LoadedVolumeVariant(*scalarU16);
  }

  if (const auto scalarF32 = LoadTyped<float>(filePath))
  {
    return LoadedVolumeVariant(*scalarF32);
  }

  if (const auto matrixF32 = LoadTyped<glm::mat3>(filePath))
  {
    return LoadedVolumeVariant(*matrixF32);
  }

  return std::nullopt;
}
