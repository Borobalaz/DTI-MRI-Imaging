#pragma once

#include <string>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/MriFodfSettings.h"
#include "Preprocessing/MriFodfTractographySettings.h"
#include "Preprocessing/MriTractographySettings.h"

/**
 * @brief Builds/parses the "dmri" dataset's opaque provenance JSON. It is the DTI parameters JSON
 *        (source paths, "tractography") merged with the fODF one ("fodf", "shell") plus a
 *        "fodfTractography" block (relative cutoff, max angle and the absolute cutoff the run
 *        derived). Shared by MriPreprocessingRunner and MriScene's save/load actions so the
 *        JSON shape cannot drift.
 *
 * @param result supplies the chosen shell (fodfResponse.bValue), the ignored shells and the
 *        absolute amplitude cutoff; all zero/empty when the fODF group did not run
 */
std::string BuildDmriPreprocessingParametersJson(
    const MriPreprocessingRequest &request,
    const MriTractographySettings &tractographySettings,
    const MriFodfSettings &fodfSettings,
    const MriFodfTractographySettings &fodfTractographySettings,
    const MriPreprocessingResult &result);

// Restores paths and all three settings objects (through their clamping setters). Missing or
// malformed blocks leave the corresponding settings untouched.
void ApplyDmriPreprocessingParametersJson(
    const std::string &parametersJson,
    MriPreprocessingRequest &requestOut,
    MriTractographySettings &tractographySettingsOut,
    MriFodfSettings &fodfSettingsOut,
    MriFodfTractographySettings &fodfTractographySettingsOut);
