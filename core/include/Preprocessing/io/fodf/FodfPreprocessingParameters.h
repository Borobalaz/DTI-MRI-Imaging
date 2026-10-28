#pragma once

#include <string>
#include <vector>

#include "Preprocessing/MriPreprocessingPipeline.h"
#include "Preprocessing/MriFodfSettings.h"

// Builds/parses the fODF dataset's opaque provenance JSON (source paths, CSD settings, chosen
// shell). Shared by DtiVolumeScene's save/load actions so the JSON shape cannot drift.
std::string BuildFodfPreprocessingParametersJson(
    const MriPreprocessingRequest& request,
    const MriFodfSettings& settings,
    float shellBValue,
    const std::vector<float>& ignoredShellBValues);

// Restores paths and CSD settings (through the clamping setters). The "shell" block is
// informational only and is not applied here.
void ApplyFodfPreprocessingParametersJson(
    const std::string& parametersJson,
    MriPreprocessingRequest& requestOut,
    MriFodfSettings& settingsOut);

// Best-effort read of "shell"/"ignoredBValues"; returns an empty vector when missing/malformed.
std::vector<float> ReadFodfIgnoredShellBValues(const std::string& parametersJson);
