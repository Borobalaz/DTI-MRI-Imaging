#pragma once

#include <string>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/MriTractographySettings.h"

/**
 * @brief Builds/parses the DTI dataset's opaque provenance JSON (source paths + tractography
 *        parameters). Shared by MriPreprocessingRunner and DtiVolumeScene's save/load actions
 *        so the JSON shape can't drift between the two call sites.
 */
std::string BuildDtiPreprocessingParametersJson(
    const MriPreprocessingRequest &request,
    const MriTractographySettings &settings,
    const std::string &matchedPresetName);

void ApplyDtiPreprocessingParametersJson(
    const std::string &parametersJson,
    MriPreprocessingRequest &requestOut,
    MriTractographySettings &settingsOut);
