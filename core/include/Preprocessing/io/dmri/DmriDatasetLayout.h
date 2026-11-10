#pragma once

#include <array>

#include "scene/volume/FodfVolume.h"

/**
 * @brief On-disk layout of a "dmri" saved dataset: manifest keys, payload paths and the fODF-map
 *        channel table, shared by DmriPreprocessingDatasetWriter and DmriPreprocessingDatasetReader
 *        so the two can never drift. The 16 DTI channels use DtiChannelTable under
 *        kChannelKeyPrefix/kChannelDirectory, exactly as in the legacy "dti"/"fodf" formats.
 */
namespace DmriDatasetLayout
{
  constexpr const char *kPreprocessorType = "dmri";
  constexpr int kFormatVersion = 1;

  constexpr const char *kVolumeExtension = ".vxa";

  // channels/<name>.vxa, required.
  constexpr const char *kChannelKeyPrefix = "channel.";
  constexpr const char *kChannelDirectory = "channels/";

  // fodf/maps/<name>.vxa, all 14 or none.
  constexpr const char *kFodfMapKeyPrefix = "fodfMap.";
  constexpr const char *kFodfMapDirectory = "fodf/maps/";

  constexpr const char *kTensorOdfShKey = "sh.tensorOdf";
  constexpr const char *kTensorOdfShPath = "sh/tensor_odf.shb";
  constexpr const char *kFodfShKey = "sh.fodf";
  constexpr const char *kFodfShPath = "sh/fodf.shb";
  constexpr const char *kShMaskKey = "sh.mask";
  constexpr const char *kShMaskPath = "sh/mask.vxa";

  constexpr const char *kResponseKey = "response";
  constexpr const char *kResponsePath = "response.json";

  constexpr const char *kSurfaceMeshKey = "mesh.surface";
  constexpr const char *kSurfaceMeshPath = "meshes/surface.meshb";
  constexpr const char *kDtiStreamlineMeshKey = "mesh.streamlinesDti";
  constexpr const char *kDtiStreamlineMeshPath = "meshes/streamlines_dti.meshb";
  constexpr const char *kFodfStreamlineMeshKey = "mesh.streamlinesFodf";
  constexpr const char *kFodfStreamlineMeshPath = "meshes/streamlines_fodf.meshb";

  constexpr const char *kSeedPointsKey = "tractography.seeds";
  constexpr const char *kSeedPointsPath = "tractography/seeds.json";

  // Always written: whether the fODF outputs exist, why not, and the absolute amplitude cutoff.
  constexpr const char *kFodfStatusKey = "fodf.status";
  constexpr const char *kFodfStatusPath = "fodf/status.json";

  struct FodfMapChannelEntry
  {
    const char *name;
    VolumeData FodfVolumeChannels::*member;
  };

  inline constexpr std::array<FodfMapChannelEntry, 14> kFodfMapChannelTable = {{
      {"AFD", &FodfVolumeChannels::AFD},
      {"NuFO", &FodfVolumeChannels::NuFO},
      {"Peak1X", &FodfVolumeChannels::Peak1X},
      {"Peak1Y", &FodfVolumeChannels::Peak1Y},
      {"Peak1Z", &FodfVolumeChannels::Peak1Z},
      {"Peak2X", &FodfVolumeChannels::Peak2X},
      {"Peak2Y", &FodfVolumeChannels::Peak2Y},
      {"Peak2Z", &FodfVolumeChannels::Peak2Z},
      {"Peak3X", &FodfVolumeChannels::Peak3X},
      {"Peak3Y", &FodfVolumeChannels::Peak3Y},
      {"Peak3Z", &FodfVolumeChannels::Peak3Z},
      {"Peak1Amplitude", &FodfVolumeChannels::Peak1Amplitude},
      {"Peak2Amplitude", &FodfVolumeChannels::Peak2Amplitude},
      {"Peak3Amplitude", &FodfVolumeChannels::Peak3Amplitude},
  }};
}
