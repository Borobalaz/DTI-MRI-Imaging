#pragma once

#include <array>

#include "scene/volume/DTIVolume.h"

/**
 * @brief Shared mapping of each DTIVolumeChannels member to its on-disk channel name. Used
 *        identically by DtiPreprocessingDatasetWriter and DtiPreprocessingDatasetReader so the
 *        saved-dataset channel layout can never drift between the two.
 */
struct DtiChannelEntry
{
  const char *name;
  VolumeData DTIVolumeChannels::*member;
};

inline const std::array<DtiChannelEntry, 16> &GetDtiChannelTable()
{
  static const std::array<DtiChannelEntry, 16> table = {{
      {"Dxx", &DTIVolumeChannels::Dxx}, {"Dyy", &DTIVolumeChannels::Dyy}, {"Dzz", &DTIVolumeChannels::Dzz},
      {"Dxy", &DTIVolumeChannels::Dxy}, {"Dxz", &DTIVolumeChannels::Dxz}, {"Dyz", &DTIVolumeChannels::Dyz},
      {"EVx", &DTIVolumeChannels::EVx}, {"EVy", &DTIVolumeChannels::EVy}, {"EVz", &DTIVolumeChannels::EVz},
      {"L1", &DTIVolumeChannels::L1}, {"L2", &DTIVolumeChannels::L2}, {"L3", &DTIVolumeChannels::L3},
      {"FA", &DTIVolumeChannels::FA}, {"MD", &DTIVolumeChannels::MD},
      {"AD", &DTIVolumeChannels::AD}, {"RD", &DTIVolumeChannels::RD},
  }};
  return table;
}
