#include "MriScene.h"
#include "render/Shader.h"
#include "scene/skybox/Skybox.h"
#include "scene/light/PointLight.h"
#include "scene/light/DirectionalLight.h"
#include "scene/camera/PerspectiveCamera.h"
#include "render/Material.h"
#include "scene/object/Mesh.h"
#include "scene/object/GameObject.h"
#include "scene/geometry/SphereGeometry.h"
#include "Preprocessing/io/PreprocessingDatasetFormats.h"
#include "Preprocessing/io/PreprocessingDatasetMetadataIO.h"
#include "Preprocessing/io/dmri/DmriDatasetLayout.h"
#include "Preprocessing/io/dmri/DmriPreprocessingParameters.h"
#include "Preprocessing/io/dti/DtiPreprocessingParameters.h"
#include "Preprocessing/io/fodf/FodfPreprocessingParameters.h"
#include "foundation/log/EngineLog.h"

#include <algorithm>
#include <any>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
  // Staging folder (inside the chosen output folder, so the final renames stay on one volume)
  // that a save writes into before its files are moved into place.
  const char *const kSaveStagingFolderName = ".dti-save-staging";

  constexpr const char *kLogSource = "MriScene";

  // Legacy read-only dataset formats ("dmri" is DmriDatasetLayout::kPreprocessorType).
  constexpr const char *kLegacyDtiPreprocessorType = "dti";
  constexpr const char *kLegacyFodfPreprocessorType = "fodf";

  // Processed object ids; they double as the object-list names.
  constexpr const char *kDtiVolumeId = "DTI Volume";
  constexpr const char *kFodfVolumeId = "fODF Volume";
  constexpr const char *kBrainSurfaceId = "Brain Surface";
  constexpr const char *kDtiStreamlinesId = "DTI Streamlines";
  constexpr const char *kFodfStreamlinesId = "fODF Streamlines";
  constexpr const char *kSeedPointsId = "Seed Points";
  constexpr const char *kTensorOdfGlyphsId = "Tensor ODF Glyphs";
  constexpr const char *kFodfGlyphsId = "fODF Glyphs";

  constexpr const char *kRepresentationGroup = "Representation";
  constexpr const char *kDtiRepresentationLabel = "DTI";
  constexpr const char *kFodfRepresentationLabel = "fODF";
  constexpr const char *kNoFodfDataReason = "the loaded dataset contains no fODF data";

  const char *DescribeJobKind(DatasetJobKind kind)
  {
    switch (kind)
    {
    case DatasetJobKind::Run:
      return "preprocessing";
    case DatasetJobKind::Load:
      return "dataset load";
    case DatasetJobKind::Save:
      return "dataset save";
    }
    return "dataset job";
  }

  ProgressSnapshot MakeInitialProgress(DatasetJobKind kind)
  {
    ProgressSnapshot progress;
    switch (kind)
    {
    case DatasetJobKind::Run:
      progress.label = "Starting preprocessing";
      break;
    case DatasetJobKind::Load:
      progress.label = "Loading saved dataset";
      break;
    case DatasetJobKind::Save:
      progress.label = "Saving dataset";
      break;
    }
    return progress;
  }

  ProgressSnapshot ToProgressSnapshot(const MriPreprocessingProgress &stageProgress)
  {
    ProgressSnapshot progress;
    progress.label = stageProgress.stageName;
    progress.step = static_cast<int>(stageProgress.stageIndex) + 1;
    progress.stepCount = static_cast<int>(stageProgress.stageCount);
    progress.fraction = stageProgress.stageCount > 0
                            ? (static_cast<float>(stageProgress.stageIndex) + stageProgress.stageFraction) /
                                  static_cast<float>(stageProgress.stageCount)
                            : 0.0f;
    return progress;
  }

  // "<stage> (step k/n, NN%)", where NN is the overall completion; label only for unstepped work.
  std::string FormatProgress(const ProgressSnapshot &progress)
  {
    if (progress.stepCount <= 0)
    {
      return progress.label;
    }

    std::ostringstream text;
    text << progress.label << " (step " << progress.step << "/" << progress.stepCount << ", "
         << static_cast<int>(std::round(progress.fraction * 100.0f)) << "%)";
    return text.str();
  }

  double MillisecondsSince(std::chrono::steady_clock::time_point start)
  {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  }

  void LogApplyDuration(std::chrono::steady_clock::time_point applyStart)
  {
    EngineLog::Message("DTI apply: " + std::to_string(std::llround(MillisecondsSince(applyStart))) +
                           " ms (engine thread)",
                       "MriScene");
  }

  // A copy of a live settings object (or defaults when there is none), so a job or an applied
  // request is unaffected by later inspector edits.
  template <typename Settings>
  std::shared_ptr<Settings> CopySettings(const std::shared_ptr<Settings> &settings)
  {
    return settings ? std::make_shared<Settings>(*settings) : std::make_shared<Settings>();
  }

  // Attaches copies of the three live settings objects to request.
  void SnapshotSettingsInto(MriPreprocessingRequest &request,
                            const std::shared_ptr<MriTractographySettings> &tractographySettings,
                            const std::shared_ptr<MriFodfSettings> &fodfSettings,
                            const std::shared_ptr<MriFodfTractographySettings> &fodfTractographySettings)
  {
    request.tractographySettings = CopySettings(tractographySettings);
    request.fodfSettings = CopySettings(fodfSettings);
    request.fodfTractographySettings = CopySettings(fodfTractographySettings);
  }

  void PrintPreprocessingReport(const MriPreprocessingReport &report)
  {
    std::cout << "Loaded: " << report.sourceVolumePath << std::endl;
    std::cout << "Executed stages:\n";
    for (const auto &stage : report.executedStages)
    {
      std::cout << "    - " << stage << "\n";
    }

    if (!report.warnings.empty())
    {
      std::cout << "  Warnings:\n";
      for (const auto &warning : report.warnings)
      {
        std::cout << "    [!] " << warning << "\n";
      }
    }
  }

  // ---- Worker-thread job bodies: pure CPU / file I/O, never GL. ----

  // Packs result's fODF maps, if any. A packing failure only costs the fODF volume: it becomes a
  // warning and result.fodfMaps is dropped, so a later save does not persist maps that cannot be
  // shown. Cancellation still propagates.
  std::optional<FodfVolumeTextureData> TryPackFodfVolumeTextures(MriPreprocessingResult &result,
                                                                 const CancellationToken &cancellation)
  {
    if (!result.fodfMaps)
    {
      return std::nullopt;
    }

    try
    {
      return PackFodfVolumeTextures(*result.fodfMaps, cancellation);
    }
    catch (const OperationCancelled &)
    {
      throw;
    }
    catch (const std::exception &error)
    {
      const std::string warning = std::string("fODF volume unavailable: packing the fODF maps failed: ") + error.what();
      EngineLog::Warning(warning, kLogSource);
      result.report.warnings.push_back(warning);
      result.fodfMaps.reset();
      return std::nullopt;
    }
  }

  // Packs the DTI channels and the fODF maps of outcome.result on the worker.
  void PackOutcomeVolumeTextures(DatasetJobOutcome &outcome, const CancellationToken &cancellation)
  {
    outcome.volumeTextures = PackDTIVolumeTextures(outcome.result->dtiChannels, cancellation);
    outcome.fodfVolumeTextures = TryPackFodfVolumeTextures(*outcome.result, cancellation);
  }

  DatasetJobOutcome RunPreprocessingJob(const MriToDmriPreprocessor &preprocessor,
                                        const MriPreprocessingRequest &request,
                                        SharedProgress &progress,
                                        const CancellationToken &cancellation)
  {
    MriPreprocessingExecutionOptions options;
    options.cancellation = cancellation;
    options.onProgress = [&progress](const MriPreprocessingProgress &stageProgress)
    { progress.Publish(ToProgressSnapshot(stageProgress)); };

    DatasetJobOutcome outcome;
    outcome.kind = DatasetJobKind::Run;
    outcome.appliedRequest = request;
    outcome.result = preprocessor.Process(request, options);

    ProgressSnapshot packingProgress;
    packingProgress.label = "Preparing volume textures";
    packingProgress.fraction = 1.0f;
    progress.Publish(packingProgress);
    PackOutcomeVolumeTextures(outcome, cancellation);
    return outcome;
  }

  DatasetJobOutcome LoadSavedDatasetJob(const std::string &metadataJsonPath, const CancellationToken &cancellation)
  {
    const std::optional<PreprocessingDatasetMetadata> metadata =
        PreprocessingDatasetMetadataIO::LoadFromFile(metadataJsonPath);
    if (!metadata.has_value())
    {
      throw std::runtime_error("Failed to read dataset manifest: " + metadataJsonPath);
    }

    const std::shared_ptr<IPreprocessingDatasetReader> reader =
        GetDefaultPreprocessingDatasetFormatRegistry().FindReader(metadata->preprocessorType);
    if (!reader)
    {
      throw std::runtime_error("No dataset reader registered for preprocessor type '" + metadata->preprocessorType + "'.");
    }

    cancellation.ThrowIfCancellationRequested();

    const std::string datasetDirectory = std::filesystem::path(metadataJsonPath).parent_path().string();
    std::any resultAny = reader->Read(*metadata, datasetDirectory);
    MriPreprocessingResult *result = std::any_cast<MriPreprocessingResult>(&resultAny);
    if (!result)
    {
      throw std::runtime_error("Saved dataset's reader returned an unexpected result type.");
    }

    cancellation.ThrowIfCancellationRequested();

    DatasetJobOutcome outcome;
    outcome.kind = DatasetJobKind::Load;
    outcome.result = std::move(*result);
    outcome.loadedPreprocessorType = metadata->preprocessorType;
    outcome.loadedParametersJson = metadata->parametersJson;
    PackOutcomeVolumeTextures(outcome, cancellation);
    return outcome;
  }

  // Moves one written dataset file from the staging folder to its final location, replacing an
  // existing file of the same name (as an in-place save would have).
  void MoveStagedDatasetFile(const std::filesystem::path &stagingDirectory,
                             const std::filesystem::path &outputDirectory,
                             const std::string &relativePath)
  {
    const std::filesystem::path target = outputDirectory / relativePath;
    std::filesystem::create_directories(target.parent_path());
    std::filesystem::remove(target);
    std::filesystem::rename(stagingDirectory / relativePath, target);
  }

  // Always writes the unified "dmri" format, whichever format the result was loaded from.
  DatasetJobOutcome SaveDatasetJob(const MriPreprocessingResult &result,
                                   const MriPreprocessingRequest &appliedRequest,
                                   const std::string &outputDirectory)
  {
    const std::string parametersJson = BuildDmriPreprocessingParametersJson(
        appliedRequest,
        *CopySettings(appliedRequest.tractographySettings),
        *CopySettings(appliedRequest.fodfSettings),
        *CopySettings(appliedRequest.fodfTractographySettings),
        result);

    const std::string preprocessorType = DmriDatasetLayout::kPreprocessorType;
    const std::shared_ptr<IPreprocessingDatasetWriter> writer =
        GetDefaultPreprocessingDatasetFormatRegistry().FindWriter(preprocessorType);
    if (!writer)
    {
      throw std::runtime_error("No dataset writer registered for preprocessor type '" + preprocessorType + "'.");
    }

    // Write everything into a staging folder first so a failed save leaves no partial dataset
    // behind, then rename the finished files into place - metadata.json last, so a manifest
    // only ever appears next to a complete payload.
    const std::filesystem::path outputPath(outputDirectory);
    const std::filesystem::path stagingPath = outputPath / kSaveStagingFolderName;
    try
    {
      std::filesystem::remove_all(stagingPath);
      const PreprocessingDatasetMetadata metadata =
          writer->Write(std::any(result), stagingPath.string(), /*outputBasename=*/"", parametersJson);

      for (const PreprocessingDatasetFileEntry &entry : metadata.files)
      {
        MoveStagedDatasetFile(stagingPath, outputPath, entry.relativePath);
      }
      MoveStagedDatasetFile(stagingPath, outputPath, "metadata.json");
      std::filesystem::remove_all(stagingPath);
    }
    catch (...)
    {
      // Best-effort cleanup only; the original error is what gets reported.
      std::error_code cleanupError;
      std::filesystem::remove_all(stagingPath, cleanupError);
      throw;
    }

    DatasetJobOutcome outcome;
    outcome.kind = DatasetJobKind::Save;
    return outcome;
  }
}

/**
 * @brief Construct a new Dti Volume Scene:: Dti Volume Scene object
 *
 */
MriScene::MriScene()
    : dtiVolume(nullptr),
      preprocessor(std::make_shared<const MriToDmriPreprocessor>()),
      jobProgress(std::make_shared<SharedProgress>())
{
  ClearGameObjects();
  ClearVolumes();

  std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>(
    "Directional Light 2",
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(1.0f, 1.0f, 1.0f),
    glm::vec3(-1.0f, -1.0f, -1.0f));
  AddLight(directionalLight);

  // Registered once: the type-filtered clears in ClearProcessedScene never remove it, and re-adding
  // it per apply would duplicate it. Listed by Scene::RebuildInspectProviders (EngineRuntime calls it
  // after construction).
  sliceController = std::make_shared<SliceController>();
  AddDrawable(sliceController);
  AddUpdateable(sliceController); // geometry rebuild on plane-version change
  // Seeded so the first tick does not cause a pointless provider-generation bump.
  publishedOffsetLimit = sliceController->GetOffsetLimit();
  publishedOffsetStep = sliceController->GetOffsetStep();

  tractographySettingsInspectable = std::make_shared<MriTractographySettings>();
  currentRequest.tractographySettings = tractographySettingsInspectable;
  fodfSettingsInspectable = std::make_shared<MriFodfSettings>();
  currentRequest.fodfSettings = fodfSettingsInspectable;
  fodfTractographySettingsInspectable = std::make_shared<MriFodfTractographySettings>();
  currentRequest.fodfTractographySettings = fodfTractographySettingsInspectable;
  currentRequest.dwiVolumePath = "assets/volumes/dwi/human/HARDI150_hdbet_masked4d.nii.gz";
  currentRequest.bvalPath = "assets/volumes/dwi/human/HARDI150.bval";
  currentRequest.bvecPath = "assets/volumes/dwi/human/HARDI150.bvec";
}

/**
 * @brief Cancel any running dataset jobs and wait for their worker threads to exit.
 *
 */
MriScene::~MriScene()
{
  // Signal both before joining either, so they wind down concurrently.
  if (activeJob)
  {
    activeJob->Cancel();
  }
  if (retiringJob)
  {
    retiringJob->Cancel();
  }

  activeJob.reset();
  retiringJob.reset();
}

/**
 * @brief Queue an asynchronous preprocessing run of the given DTI dataset; the result is
 *        applied by a later Update().
 *
 * @param dwiVolumePath The path to the DWI volume
 * @param bvalPath The path to the b-values file
 * @param bvecPath The path to the b-vectors file
 */
void MriScene::LoadDataset(
    const std::string &dwiVolumePath,
    const std::string &bvalPath,
    const std::string &bvecPath)
{
  currentRequest.tractographySettings = tractographySettingsInspectable;
  currentRequest.fodfSettings = fodfSettingsInspectable;
  currentRequest.fodfTractographySettings = fodfTractographySettingsInspectable;
  currentRequest.dwiVolumePath = dwiVolumePath;
  currentRequest.bvalPath = bvalPath;
  currentRequest.bvecPath = bvecPath;

  ReloadDataset();
}

void MriScene::ReloadDataset()
{
  datasetReloadRequested = true;
}

void MriScene::ClearProcessedScene()
{
  dtiVolume.reset();
  fodfVolume.reset();
  brainSurfaceObject.reset();
  dtiStreamlineObject.reset();
  fodfStreamlineObject.reset();
  seedPointsObject.reset();
  tensorOdfGlyphField.reset();
  fodfGlyphField.reset();
  // sliceController survives these type-filtered clears. If this ever calls ClearDrawables() or
  // ClearUpdateables(), re-add sliceController right after them.
  ClearGameObjects();
  ClearVolumes();
  ClearGlyphFields();
}

namespace
{
  // Every processed object is shown with the same base rotation (dataset Z-up into the scene's Y-up).
  const glm::vec3 kProcessedObjectRotation(-90.0f / 180.0f * glm::pi<float>(), 0.0f, 0.0f);

  // Nearest-neighbor seed spacing in the same normalized object space as seedPoints (mirrors
  // StreamlineTracing::VoxelToObjectSpace's normalization), so the sphere radius
  // scales with dataset resolution and the seed stride setting instead of a fixed constant -
  // keeping neighboring markers from touching at any setting.
  float ComputeSeedPointRadius(const VolumeMetadata &seedMetadata, const MriPreprocessingRequest &appliedRequest)
  {
    const glm::ivec3 seedDims = seedMetadata.dimensions;
    const glm::vec3 seedSpacing = seedMetadata.spacing;
    const glm::vec3 seedExtent = glm::vec3(std::max(seedDims.x - 1, 1),
                                           std::max(seedDims.y - 1, 1),
                                           std::max(seedDims.z - 1, 1)) *
                                 seedSpacing;
    const float seedMaxExtent = std::max({seedExtent.x, seedExtent.y, seedExtent.z});
    // Stride the seeds were actually traced with, not the (possibly since edited) live setting.
    const int seedStride = std::max(appliedRequest.tractographySettings ? appliedRequest.tractographySettings->GetSeedStride() : 1, 1);
    const float seedMinAxisSpacing = std::min({seedSpacing.x, seedSpacing.y, seedSpacing.z});
    const float seedSpacingObjectSpace = seedMaxExtent > 1e-6f
                                              ? (static_cast<float>(seedStride) * seedMinAxisSpacing) / seedMaxExtent
                                              : 0.01f;
    return std::max(seedSpacingObjectSpace * 0.35f, 1e-4f);
  }

  // (Re)builds a cached mesh shader that is missing or failed to build, and registers it for hot reload.
  void EnsureMeshShader(Scene &scene,
                        std::shared_ptr<Shader> &cachedShader,
                        const std::string &shaderId,
                        const std::string &vertexPath,
                        const std::string &fragmentPath)
  {
    if (cachedShader && cachedShader->IsValid())
    {
      return;
    }

    cachedShader = std::make_shared<Shader>(shaderId, vertexPath, fragmentPath);
    scene.RegisterShader(shaderId, cachedShader);
  }
}

/**
 * @brief Compile (or recover) the shaders every apply reuses and register them for hot reload.
 *        GL thread only. Shaders that failed to build are retried here, because hot reload skips
 *        invalid programs - so fixing a broken shader file takes effect on the next apply.
 */
void MriScene::EnsureShaderCache()
{
  if (shaderCache.volumeRenderModes.size() < 5)
  {
    shaderCache.volumeRenderModes = DTIVolume::CreateRenderModes();
    for (const DTIVolume::RenderMode &mode : shaderCache.volumeRenderModes)
    {
      RegisterShader("DTIVolume/" + mode.label, mode.shader);
    }
  }

  if (shaderCache.fodfVolumeRenderModes.size() < 2)
  {
    shaderCache.fodfVolumeRenderModes = FodfVolume::CreateRenderModes();
    for (const FodfVolume::RenderMode &mode : shaderCache.fodfVolumeRenderModes)
    {
      RegisterShader("FodfVolume/" + mode.label, mode.shader);
    }
  }

  EnsureMeshShader(*this, shaderCache.brainSurface, "dti_brain_surface_shader",
                   "shaders/dti_mesh/dti_mesh_vertex.glsl", "shaders/dti_mesh/dti_mesh_fragment.glsl");
  EnsureMeshShader(*this, shaderCache.streamline, "dti_streamline_shader",
                   "shaders/streamlines/streamline_vertex.glsl", "shaders/streamlines/streamline_fragment.glsl");
  EnsureMeshShader(*this, shaderCache.seedPoint, "dti_seed_point_shader",
                   "shaders/dti_mesh/dti_mesh_instanced_vertex.glsl", "shaders/dti_mesh/dti_mesh_fragment.glsl");
  EnsureMeshShader(*this, shaderCache.shGlyph, "dti_sh_glyph_shader",
                   "shaders/sh_glyph/sh_glyph_vertex.glsl", "shaders/sh_glyph/sh_glyph_fragment.glsl");
  EnsureMeshShader(*this, shaderCache.slicePlaneUnlit, "dti_unlit_shader",
                   "shaders/unlit/unlit_vertex.glsl", "shaders/unlit/unlit_fragment.glsl");
}

/**
 * @brief Build the fODF volume from pre-packed textures with the cached render modes. Returns null
 *        (with a warning) when no fODF render mode shader built, so the rest of the scene still applies.
 */
std::shared_ptr<FodfVolume> MriScene::CreateFodfVolume(FodfVolumeTextureData fodfVolumeTextures,
                                                             int renderModeIndex,
                                                             int channelIndex) const
{
  if (shaderCache.fodfVolumeRenderModes.empty())
  {
    EngineLog::Warning("fODF volume skipped: no fODF render mode shader built", kLogSource);
    return nullptr;
  }

  std::shared_ptr<FodfVolume> volume = std::make_shared<FodfVolume>(
      kFodfVolumeId, std::move(fodfVolumeTextures), shaderCache.fodfVolumeRenderModes);
  volume->SetRotation(kProcessedObjectRotation);
  volume->SetSelectedRenderModeIndex(renderModeIndex);
  volume->SetSelectedChannelIndex(channelIndex);
  volume->SetSlicePlane(sliceController->GetPlane());
  return volume;
}

/**
 * @brief Wrap a streamline tube mesh (DTI or fODF) in a GameObject using the shared streamline
 *        shader. Null when there is no mesh.
 */
std::shared_ptr<GameObject> MriScene::CreateStreamlineObject(const std::string &id,
                                                                   const std::shared_ptr<Mesh> &mesh) const
{
  if (!mesh)
  {
    return nullptr;
  }

  mesh->SetMaterial(std::make_shared<Material>(shaderCache.streamline));
  std::shared_ptr<GameObject> streamlineObject = std::make_shared<GameObject>(id);
  streamlineObject->AddMesh(mesh);
  streamlineObject->SetRotation(kProcessedObjectRotation);
  return streamlineObject;
}

/**
 * @brief Create an SH glyph field (tensor ODF or fODF) on the shared glyph shader and slice plane.
 *        Null when there is no SH volume. The glyph instance data builds on its own worker, starting
 *        in its first Update, so it sees the plane re-clamped by SetDatasetGeometry.
 */
std::shared_ptr<SHGlyphField> MriScene::CreateGlyphField(const std::string &id,
                                                               const std::shared_ptr<const SHVolume> &volume,
                                                               SHGlyphDisplayMode displayMode) const
{
  if (!volume)
  {
    return nullptr;
  }

  std::shared_ptr<SHGlyphField> glyphField = std::make_shared<SHGlyphField>(
      id, volume, shaderCache.shGlyph, SHGlyphBuildSettings{3, 0.3f, 3, displayMode});
  glyphField->SetRotation(kProcessedObjectRotation);
  glyphField->SetSlicePlane(sliceController->GetPlane());
  return glyphField;
}

/**
 * @brief Swap the processed scene content for a new result. Everything is built into locals
 *        first, so a throw (e.g. no usable DTI render mode) leaves the previous scene untouched.
 *        The volume textures arrive pre-packed and upload lazily at first draw, and the shaders
 *        come from the cache, so this makes no heavy GL or CPU calls itself. Each object keeps its
 *        own display settings across reruns and loads (DTI/fODF render mode and channel, each
 *        glyph field's Display mode); the Representation then decides which objects are shown.
 */
void MriScene::ApplyPreprocessingResult(MriPreprocessingResult result,
                                              DTIVolumeTextureData volumeTextures,
                                              std::optional<FodfVolumeTextureData> fodfVolumeTextures,
                                              const MriPreprocessingRequest &appliedRequest)
{
  const int previousRenderMode = dtiVolume ? dtiVolume->GetSelectedRenderModeIndex() : 0;
  const int previousChannel = dtiVolume ? dtiVolume->GetSelectedChannelIndex() : DTIVolume::kDefaultChannelIndex;
  const int previousFodfRenderMode = fodfVolume ? fodfVolume->GetSelectedRenderModeIndex() : 0;
  const int previousFodfChannel = fodfVolume ? fodfVolume->GetSelectedChannelIndex() : FodfVolume::kDefaultChannelIndex;
  // The glyph Display modes persist across reruns and loads; the other glyph settings reset.
  const SHGlyphDisplayMode previousTensorOdfGlyphDisplayMode =
      tensorOdfGlyphField ? tensorOdfGlyphField->GetDisplayMode() : SHGlyphDisplayMode::Slice;
  const SHGlyphDisplayMode previousFodfGlyphDisplayMode =
      fodfGlyphField ? fodfGlyphField->GetDisplayMode() : SHGlyphDisplayMode::Slice;
  EnsureShaderCache();

  // Read before volumeTextures is moved into the DTIVolume.
  const glm::vec3 datasetExtents = DatasetObjectExtents(volumeTextures.dimensions, volumeTextures.spacing);
  const float datasetVoxelStep = DatasetVoxelStep(volumeTextures.dimensions, volumeTextures.spacing);

  // ---- Build into locals; the scene is not touched yet ----
  std::shared_ptr<DTIVolume> newDtiVolume = std::make_shared<DTIVolume>(
      kDtiVolumeId, std::move(volumeTextures), shaderCache.volumeRenderModes);
  newDtiVolume->SetRotation(kProcessedObjectRotation);
  newDtiVolume->SetSelectedRenderModeIndex(previousRenderMode);
  newDtiVolume->SetSelectedChannelIndex(previousChannel);
  newDtiVolume->SetSlicePlane(sliceController->GetPlane());

  std::shared_ptr<FodfVolume> newFodfVolume =
      fodfVolumeTextures ? CreateFodfVolume(std::move(*fodfVolumeTextures), previousFodfRenderMode, previousFodfChannel)
                         : nullptr;

  // FA surface mesh
  std::shared_ptr<GameObject> newBrainSurfaceObject;
  if (result.surfaceMesh)
  {
    std::shared_ptr<Material> meshMaterial = std::make_shared<Material>(shaderCache.brainSurface);
    meshMaterial->SetBaseColor(glm::vec3(0.62f, 0.62f, 0.62f));
    meshMaterial->SetRoughness(0.32f);
    meshMaterial->SetMetallic(0.0f);

    result.surfaceMesh->SetMaterial(meshMaterial);
    newBrainSurfaceObject = std::make_shared<GameObject>(kBrainSurfaceId);
    newBrainSurfaceObject->AddMesh(result.surfaceMesh);
    newBrainSurfaceObject->SetRotation(kProcessedObjectRotation);
  }

  std::shared_ptr<GameObject> newDtiStreamlineObject = CreateStreamlineObject(kDtiStreamlinesId, result.streamlineMesh);
  std::shared_ptr<GameObject> newFodfStreamlineObject =
      CreateStreamlineObject(kFodfStreamlinesId, result.fodfStreamlineMesh);

  // Tractography seed points (shared by the DTI and fODF trackers), rendered as small instanced
  // red spheres - one GPU draw call for every seed regardless of count. Demonstrates GameObject
  // GPU instancing and doubles as a way to visually verify seed placement/density as
  // tractography settings change.
  std::shared_ptr<GameObject> newSeedPointsObject;
  if (!result.seedPoints.empty())
  {
    std::shared_ptr<Material> seedPointMaterial = std::make_shared<Material>(shaderCache.seedPoint);
    seedPointMaterial->SetBaseColor(glm::vec3(1.0f, 0.05f, 0.05f));
    seedPointMaterial->SetRoughness(0.5f);
    seedPointMaterial->SetMetallic(0.0f);

    const float seedPointRadius = ComputeSeedPointRadius(result.dtiChannels.FA.GetMetadata(), appliedRequest);
    std::shared_ptr<Geometry> seedPointGeometry = std::make_shared<SphereGeometry>(seedPointRadius, 12, 8);
    std::shared_ptr<Mesh> seedPointMesh = std::make_shared<Mesh>(seedPointGeometry, seedPointMaterial);

    newSeedPointsObject = std::make_shared<GameObject>(kSeedPointsId);
    newSeedPointsObject->AddMesh(seedPointMesh);
    // The instanced path applies this GameObject's own transform as a shared outer/group
    // transform on top of every instance, same as the volume/surface/streamlines above - so
    // this rotation (and Update()'s continuous spin below) affects the whole batch without
    // needing to touch the per-instance matrices.
    newSeedPointsObject->SetRotation(kProcessedObjectRotation);

    // Per-instance matrices are translation-only; the shared rotation above is applied once as
    // a group, not baked into each seed.
    std::vector<glm::mat4> seedTransforms;
    seedTransforms.reserve(result.seedPoints.size());
    for (const glm::vec3 &seedPosition : result.seedPoints)
    {
      seedTransforms.push_back(glm::translate(glm::mat4(1.0f), seedPosition));
    }
    newSeedPointsObject->SetInstanceTransforms(seedTransforms);
  }

  // One glyph field per SH volume the result carries. Legacy "dti" datasets carry neither.
  std::shared_ptr<SHGlyphField> newTensorOdfGlyphField =
      CreateGlyphField(kTensorOdfGlyphsId, result.shVolume, previousTensorOdfGlyphDisplayMode);
  std::shared_ptr<SHGlyphField> newFodfGlyphField =
      CreateGlyphField(kFodfGlyphsId, result.fodfShVolume, previousFodfGlyphDisplayMode);
  if (!newTensorOdfGlyphField && !newFodfGlyphField)
  {
    EngineLog::Message("Dataset has no spherical-harmonics data; SH glyphs unavailable", kLogSource);
  }

  std::string newFodfUnavailableReason = result.fodfUnavailableReason;
  if (newFodfUnavailableReason.empty() && !newFodfVolume && !newFodfStreamlineObject && !newFodfGlyphField)
  {
    newFodfUnavailableReason = kNoFodfDataReason;
  }

  // ---- Commit ----
  // Cache the result (moved, not copied) and the request/settings it was produced with, so
  // "Save current dataset" can persist it later without re-running preprocessing. The objects
  // built above already hold their own shared_ptr copies of its meshes.
  lastPreprocessingResult = std::make_shared<const MriPreprocessingResult>(std::move(result));
  lastAppliedRequest = appliedRequest;
  fodfUnavailableReason = std::move(newFodfUnavailableReason);

  ClearProcessedScene();

  dtiVolume = std::move(newDtiVolume);
  fodfVolume = std::move(newFodfVolume);
  brainSurfaceObject = std::move(newBrainSurfaceObject);
  dtiStreamlineObject = std::move(newDtiStreamlineObject);
  fodfStreamlineObject = std::move(newFodfStreamlineObject);
  seedPointsObject = std::move(newSeedPointsObject);
  tensorOdfGlyphField = std::move(newTensorOdfGlyphField);
  fodfGlyphField = std::move(newFodfGlyphField);

  sliceController->SetDatasetGeometry(datasetExtents, datasetVoxelStep); // keeps normal/offset/preset, re-clamps
  sliceController->SetShader(shaderCache.slicePlaneUnlit);              // no-op unless the cache rebuilt it
  sliceController->SetRotation(kProcessedObjectRotation);               // same base rotation as the new objects

  // Added in object-list order.
  AddVolume(dtiVolume);
  if (fodfVolume)
  {
    AddVolume(fodfVolume);
  }
  if (brainSurfaceObject)
  {
    AddGameObject(brainSurfaceObject);
  }
  if (dtiStreamlineObject)
  {
    AddGameObject(dtiStreamlineObject);
  }
  if (fodfStreamlineObject)
  {
    AddGameObject(fodfStreamlineObject);
  }
  if (seedPointsObject)
  {
    AddGameObject(seedPointsObject);
  }
  if (tensorOdfGlyphField)
  {
    AddGlyphField(tensorOdfGlyphField);
  }
  if (fodfGlyphField)
  {
    AddGlyphField(fodfGlyphField);
  }

  ApplyRepresentationVisibility();
  representationVisibilityDirty = false;
  RebuildInspectProviders();
}

bool MriScene::IsFodfAvailable() const
{
  return fodfUnavailableReason.empty();
}

MriScene::Representation MriScene::GetEffectiveRepresentation() const
{
  return selectedRepresentation == Representation::Fodf && IsFodfAvailable() ? Representation::Fodf
                                                                              : Representation::Dti;
}

void MriScene::ApplyRepresentationVisibility()
{
  const bool showFodf = GetEffectiveRepresentation() == Representation::Fodf;
  const auto showPair = [showFodf](InspectProvider *dtiSide, InspectProvider *fodfSide)
  {
    InspectProvider *preferred = showFodf ? fodfSide : dtiSide;
    InspectProvider *other = showFodf ? dtiSide : fodfSide;
    if (preferred)
    {
      preferred->SetVisible(true);
      if (other)
      {
        other->SetVisible(false);
      }
    }
    else if (other)
    {
      other->SetVisible(true); // partial pair (legacy dataset): keep the side that exists visible
    }
  };

  showPair(dtiVolume.get(), fodfVolume.get());
  showPair(dtiStreamlineObject.get(), fodfStreamlineObject.get());
  showPair(tensorOdfGlyphField.get(), fodfGlyphField.get());
}

/**
 * @brief The "Representation" dashboard group: the DTI/fODF switch, the reason fODF is unavailable
 *        (when it is) and the scene slice-plane controls, which SliceController reports under its
 *        own group and are regrouped here.
 */
std::vector<InspectFieldPtr> MriScene::GetRepresentationDashboardFields()
{
  std::vector<InspectFieldPtr> fields;

  const auto effectiveLabel = [this]() -> std::string
  {
    return GetEffectiveRepresentation() == Representation::Fodf ? kFodfRepresentationLabel
                                                                : kDtiRepresentationLabel;
  };
  auto representationField = MakeInspectField(
      "representation", "Representation", kRepresentationGroup, InspectFieldType::Dropdown,
      effectiveLabel(),
      [effectiveLabel]() -> InspectValue
      { return effectiveLabel(); },
      [this](const InspectValue &value)
      {
        const auto *label = std::get_if<std::string>(&value);
        if (!label)
          return;
        if (*label == kFodfRepresentationLabel)
          selectedRepresentation = Representation::Fodf;
        else if (*label == kDtiRepresentationLabel)
          selectedRepresentation = Representation::Dti;
        else
          return;
        // Even re-selecting the current value re-applies it, overriding manual eye toggles.
        representationVisibilityDirty = true;
      });
  representationField->enumOptions = {kDtiRepresentationLabel, kFodfRepresentationLabel};
  representationField->readOnly = !IsFodfAvailable();
  fields.push_back(representationField);

  if (!IsFodfAvailable())
  {
    auto fodfStatusField = MakeInspectField(
        "representationFodfStatus", "fODF", kRepresentationGroup, InspectFieldType::Text,
        "fODF unavailable: " + fodfUnavailableReason);
    fodfStatusField->readOnly = true;
    fields.push_back(fodfStatusField);
  }

  for (const InspectFieldPtr &sliceField : sliceController->GetDashboardFields())
  {
    sliceField->groupName = kRepresentationGroup;
    fields.push_back(sliceField);
  }

  return fields;
}

void MriScene::ApplyLoadedParameters(const std::string &preprocessorType, const std::string &parametersJson)
{
  if (preprocessorType == DmriDatasetLayout::kPreprocessorType)
  {
    ApplyDmriPreprocessingParametersJson(parametersJson, currentRequest, *tractographySettingsInspectable,
                                         *fodfSettingsInspectable, *fodfTractographySettingsInspectable);
  }
  else if (preprocessorType == kLegacyFodfPreprocessorType)
  {
    ApplyFodfPreprocessingParametersJson(parametersJson, currentRequest, *fodfSettingsInspectable);
  }
  else if (preprocessorType == kLegacyDtiPreprocessorType)
  {
    ApplyDtiPreprocessingParametersJson(parametersJson, currentRequest, *tractographySettingsInspectable);
  }
  else
  {
    EngineLog::Warning("Loaded dataset type '" + preprocessorType + "' has no known parameters; none applied",
                       kLogSource);
  }
}

/**
 * @brief Collect a finished job (if any) and hand its outcome to ApplyJobOutcome. Reaps a
 *        finished superseded job without looking at its outcome.
 */
void MriScene::PollJobs()
{
  if (retiringJob && retiringJob->IsFinished())
  {
    try
    {
      retiringJob->TakeResult();
    }
    catch (...)
    {
      // Deliberately ignored: this job was superseded, so neither its result nor how it
      // ended (cancelled or failed) is relevant any more.
    }
    retiringJob.reset();
  }

  if (!activeJob || !activeJob->IsFinished())
  {
    return;
  }

  DatasetJobOutcome outcome;
  outcome.kind = activeJobKind;
  try
  {
    outcome = activeJob->TakeResult();
  }
  catch (const OperationCancelled &)
  {
    outcome.cancelled = true;
  }
  catch (const std::exception &ex)
  {
    outcome.error = ex.what();
  }
  catch (...)
  {
    outcome.error = "Unknown error";
  }

  // A job cancelled after its last cancellation check may still have produced a result;
  // it was superseded or cancelled by the user, so it must not be applied.
  outcome.cancelled = outcome.cancelled || activeJobCancelled;

  activeJob.reset();
  activeJobCancelled = false;
  statusNotice.clear();

  ApplyJobOutcome(std::move(outcome));
}

/**
 * @brief Queue a dataset job. Run/Load are latest-wins (cancelling a running Run/Load) and
 *        wait behind a running Save; a Save is refused while anything else is in flight.
 */
void MriScene::RequestJob(DatasetJobKind kind)
{
  if (kind == DatasetJobKind::Save)
  {
    if (activeJob || pendingJob)
    {
      statusNotice = "Busy: save refused";
      std::cout << "DTI dataset save refused: another dataset job is running\n";
      return;
    }

    pendingJob = kind;
    return;
  }

  if (activeJob && activeJobKind != DatasetJobKind::Save)
  {
    RetireActiveJob();
  }

  pendingJob = kind;
}

/**
 * @brief "Cancel preprocessing": stops a running or queued Run/Load. A Save is never cancelled.
 */
void MriScene::CancelPreprocessingJobs()
{
  if (pendingJob && *pendingJob != DatasetJobKind::Save)
  {
    pendingJob.reset();
    statusText = "Cancelled";
  }

  if (activeJob && activeJobKind != DatasetJobKind::Save)
  {
    RetireActiveJob();
    statusText = "Cancelled";
  }
}

/**
 * @brief Signal the active job to stop and, if the retiring slot is free, park it there so a
 *        new job can start right away without waiting for (or joining) the old worker.
 */
void MriScene::RetireActiveJob()
{
  activeJob->Cancel();

  if (retiringJob)
  {
    // Slot still taken by an earlier superseded job: keep this one active until it stops,
    // which bounds the number of concurrently winding-down workers to two.
    activeJobCancelled = true;
    return;
  }

  retiringJob = std::move(activeJob);
  activeJobCancelled = false;
}

void MriScene::StartPendingJob()
{
  if (activeJob || !pendingJob)
  {
    return;
  }

  const DatasetJobKind kind = *pendingJob;
  pendingJob.reset();
  statusNotice.clear();

  // A fresh mailbox per job, so a winding-down job can't overwrite the new job's progress.
  jobProgress = std::make_shared<SharedProgress>();
  jobProgress->Publish(MakeInitialProgress(kind));

  switch (kind)
  {
  case DatasetJobKind::Run:
    activeJob = CreateRunJob();
    break;
  case DatasetJobKind::Load:
    activeJob = CreateLoadJob();
    break;
  case DatasetJobKind::Save:
    activeJob = CreateSaveJob();
    break;
  }

  activeJobKind = kind;
  activeJobCancelled = false;
}

/**
 * @brief Start a unified DTI + fODF preprocessing run on a snapshot of the current request and a
 *        copy of the tractography, fODF and fODF tractography settings, so later inspector edits
 *        can't race with the worker.
 */
std::unique_ptr<MriScene::DatasetJob> MriScene::CreateRunJob()
{
  MriPreprocessingRequest request = currentRequest;
  SnapshotSettingsInto(request, tractographySettingsInspectable, fodfSettingsInspectable,
                       fodfTractographySettingsInspectable);

  const std::shared_ptr<const MriToDmriPreprocessor> runPreprocessor = preprocessor;
  const std::shared_ptr<SharedProgress> progress = jobProgress;
  return std::make_unique<DatasetJob>(
      [runPreprocessor, request, progress](const CancellationToken &cancellation)
      { return RunPreprocessingJob(*runPreprocessor, request, *progress, cancellation); });
}

/**
 * @brief Start reading a saved dataset; its provenance parameters are applied to the scene
 *        only once the read succeeded (see ApplyJobOutcome).
 */
std::unique_ptr<MriScene::DatasetJob> MriScene::CreateLoadJob()
{
  if (pendingLoadMetadataPath.empty())
  {
    FailJob(DatasetJobKind::Load, "No saved dataset manifest was selected.");
    return nullptr;
  }

  const std::string metadataJsonPath = pendingLoadMetadataPath;
  return std::make_unique<DatasetJob>(
      [metadataJsonPath](const CancellationToken &cancellation)
      { return LoadSavedDatasetJob(metadataJsonPath, cancellation); });
}

/**
 * @brief Start persisting the most recently applied result together with the request and
 *        settings it was actually produced with, without re-running preprocessing.
 */
std::unique_ptr<MriScene::DatasetJob> MriScene::CreateSaveJob()
{
  if (!lastPreprocessingResult)
  {
    FailJob(DatasetJobKind::Save, "No preprocessed dataset is loaded yet.");
    return nullptr;
  }

  if (pendingSaveOutputDirectory.empty())
  {
    FailJob(DatasetJobKind::Save, "Save output folder is empty.");
    return nullptr;
  }

  // The captured result stays referenced by this task until it is destroyed on this thread,
  // so the worker never releases the last reference to the GPU-backed meshes it reads.
  const std::shared_ptr<const MriPreprocessingResult> result = lastPreprocessingResult;
  const MriPreprocessingRequest appliedRequest = lastAppliedRequest;
  const std::string outputDirectory = pendingSaveOutputDirectory;
  return std::make_unique<DatasetJob>(
      [result, appliedRequest, outputDirectory](const CancellationToken &)
      { return SaveDatasetJob(*result, appliedRequest, outputDirectory); });
}

/**
 * @brief Apply a finished job's outcome on the Update() (GL) thread: build the scene from a
 *        Run/Load result, or record a save/failure/cancellation in the status.
 */
void MriScene::ApplyJobOutcome(DatasetJobOutcome outcome)
{
  if (outcome.cancelled)
  {
    statusText = "Cancelled";
    std::cout << "DTI " << DescribeJobKind(outcome.kind) << " cancelled\n";
    return;
  }

  if (!outcome.error.empty())
  {
    FailJob(outcome.kind, outcome.error);
    return;
  }

  try
  {
    switch (outcome.kind)
    {
    case DatasetJobKind::Run:
      if (!outcome.result || outcome.result->report.sourceVolumePath.empty())
      {
        FailJob(outcome.kind, "No suitable volume found in dataset");
        return;
      }
      if (!outcome.volumeTextures)
      {
        FailJob(outcome.kind, "Volume textures were not prepared.");
        return;
      }
      PrintPreprocessingReport(outcome.result->report);
      lastLoadError.clear();
      {
        const auto applyStart = std::chrono::steady_clock::now();
        ApplyPreprocessingResult(std::move(*outcome.result), std::move(*outcome.volumeTextures),
                                 std::move(outcome.fodfVolumeTextures), outcome.appliedRequest);
        LogApplyDuration(applyStart);
      }
      break;

    case DatasetJobKind::Load:
    {
      if (!outcome.result)
      {
        FailJob(outcome.kind, "Saved dataset load produced no result.");
        return;
      }
      if (!outcome.volumeTextures)
      {
        FailJob(outcome.kind, "Volume textures were not prepared.");
        return;
      }
      ApplyLoadedParameters(outcome.loadedPreprocessorType, outcome.loadedParametersJson);
      // The applied request gets copies of the (just updated) settings, like a run's snapshot.
      MriPreprocessingRequest appliedRequest = currentRequest;
      SnapshotSettingsInto(appliedRequest, tractographySettingsInspectable, fodfSettingsInspectable,
                           fodfTractographySettingsInspectable);
      lastLoadError.clear();
      const auto applyStart = std::chrono::steady_clock::now();
      ApplyPreprocessingResult(std::move(*outcome.result), std::move(*outcome.volumeTextures),
                               std::move(outcome.fodfVolumeTextures), appliedRequest);
      LogApplyDuration(applyStart);
      break;
    }

    case DatasetJobKind::Save:
      lastSaveError.clear();
      std::cout << "DTI dataset saved\n";
      break;
    }

    statusText = "Ready";
  }
  catch (const std::exception &ex)
  {
    FailJob(outcome.kind, ex.what());
  }
}

void MriScene::FailJob(DatasetJobKind kind, const std::string &error)
{
  if (kind == DatasetJobKind::Save)
  {
    lastSaveError = error;
  }
  else
  {
    lastLoadError = error;
  }

  statusText = "Failed: " + error;
  std::cerr << "DTI " << DescribeJobKind(kind) << " failed: " << error << std::endl;
}

void MriScene::RefreshStatusText()
{
  if (!activeJob)
  {
    return;
  }

  statusText = activeJobCancelled ? std::string("Cancelling...") : FormatProgress(jobProgress->Read());
}

std::string MriScene::GetPreprocessingStatus() const
{
  return statusNotice.empty() ? statusText : statusText + " - " + statusNotice;
}

/**
 * @brief Update the scene: collect finished dataset jobs, start requested ones, and apply a
 *  slow rotation to the brain surface and streamlines for better visualization of the 3D structure.
 *
 * @param deltaTime
 */
void MriScene::Update(float deltaTime)
{
  // Compile the reusable shaders up front (GL thread), so the first apply doesn't pay for it.
  if (!shaderCacheWarmed)
  {
    shaderCacheWarmed = true;
    EnsureShaderCache();
  }

  // Job outcomes are applied here rather than on the worker because ApplyPreprocessingResult
  // swaps scene state owned by this thread and may (re)compile cached shaders.
  PollJobs();

  // A Representation switch overrides the eye toggles; the object list only refreshes its eye
  // icons when the provider generation bumps.
  if (representationVisibilityDirty)
  {
    representationVisibilityDirty = false;
    ApplyRepresentationVisibility();
    RebuildInspectProviders();
  }

  if (cancelRequested)
  {
    cancelRequested = false;
    CancelPreprocessingJobs();
  }

  if (datasetReloadRequested)
  {
    datasetReloadRequested = false;
    RequestJob(DatasetJobKind::Run);
  }

  if (datasetSaveRequested)
  {
    datasetSaveRequested = false;
    RequestJob(DatasetJobKind::Save);
  }

  if (datasetLoadRequested)
  {
    datasetLoadRequested = false;
    RequestJob(DatasetJobKind::Load);
  }

  StartPendingJob();
  RefreshStatusText();

  Scene::Update(deltaTime);

  if (rotationEnabled)
  {
    const glm::vec3 rotationDelta(0.0f, 0.0f, deltaTime * rotationSpeed);
    const auto rotate = [&rotationDelta](const auto &object)
    {
      if (object)
      {
        object->SetRotation(object->GetRotation() + rotationDelta);
      }
    };
    rotate(dtiVolume);
    rotate(fodfVolume);
    rotate(brainSurfaceObject);
    rotate(dtiStreamlineObject);
    rotate(fodfStreamlineObject);
    rotate(seedPointsObject);
    rotate(tensorOdfGlyphField);
    rotate(fodfGlyphField);
    // The volumes and glyphs carry the plane implicitly (it lives in their object space); only
    // its own drawing needs the spin.
    rotate(sliceController);
  }

  RebuildInspectProvidersIfLayoutChanged();
}

void MriScene::RebuildInspectProvidersIfLayoutChanged()
{
  const float offsetLimit = sliceController->GetOffsetLimit();
  const float offsetStep = sliceController->GetOffsetStep();
  const auto displayModeOf = [](const std::shared_ptr<SHGlyphField> &glyphField)
  { return glyphField ? static_cast<int>(glyphField->GetDisplayMode()) : -1; };
  const int tensorOdfGlyphDisplayMode = displayModeOf(tensorOdfGlyphField);
  const int fodfGlyphDisplayMode = displayModeOf(fodfGlyphField);
  // Exact comparison is intended: the inputs are recomputed deterministically.
  if (offsetLimit == publishedOffsetLimit && offsetStep == publishedOffsetStep &&
      tensorOdfGlyphDisplayMode == publishedTensorOdfGlyphDisplayMode &&
      fodfGlyphDisplayMode == publishedFodfGlyphDisplayMode)
  {
    return;
  }

  publishedOffsetLimit = offsetLimit;
  publishedOffsetStep = offsetStep;
  publishedTensorOdfGlyphDisplayMode = tensorOdfGlyphDisplayMode;
  publishedFodfGlyphDisplayMode = fodfGlyphDisplayMode;
  RebuildInspectProviders(); // bumps the provider generation -> the UI re-fetches field metadata
}

void MriScene::Apply(IUniformSink &shader) const
{
  Scene::Apply(shader);

  // Without a dataset there is nothing to cut; shaders lacking the uniform ignore these setters.
  const bool cutEnabled = sliceController && sliceController->IsCutEnabled() && dtiVolume;
  shader.SetBool("clipPlane.enabled", cutEnabled);
  if (cutEnabled)
  {
    const PlaneEquation worldPlane = sliceController->GetWorldClipPlane();
    shader.SetVec3("clipPlane.normal", worldPlane.normal);
    shader.SetFloat("clipPlane.offset", worldPlane.offset);
  }
}

/**
 * @brief Create the Scene Dashboard fields controlling scene-wide parameters such as preprocessing and rotation.
 *
 * @return engine inspection fields
 */
std::vector<InspectFieldPtr> MriScene::GetDashboardFields()
{
  std::vector<InspectFieldPtr> fields = Scene::GetDashboardFields();
  const auto append = [&fields](const std::vector<InspectFieldPtr> &more)
  { fields.insert(fields.end(), more.begin(), more.end()); };

  append(GetRepresentationDashboardFields());
  append(tractographySettingsInspectable->GetInspectFields());
  append(fodfTractographySettingsInspectable->GetInspectFields());
  append(fodfSettingsInspectable->GetInspectFields());

  const auto addFileField = [&fields](const std::string &id, const std::string &label, std::string *path)
  {
    fields.push_back(MakeInspectField(
        id, label, "Preprocessing", InspectFieldType::File, *path,
        [path]() -> InspectValue
        { return *path; },
        [path](const InspectValue &value)
        {
          if (const auto *selectedPath = std::get_if<std::string>(&value))
            *path = *selectedPath;
        }));
  };
  addFileField("dwiVolumePath", "DWI Volume", &currentRequest.dwiVolumePath);
  addFileField("bvalPath", "B-Values", &currentRequest.bvalPath);
  addFileField("bvecPath", "B-Vectors", &currentRequest.bvecPath);

  fields.push_back(MakeInspectField(
      "rerunPreprocessing", dtiVolume ? "Rerun preprocessing" : "Run Preprocessing", "Preprocessing", InspectFieldType::Action,
      std::monostate{}, {},
      [this](const InspectValue &)
      { datasetReloadRequested = true; }));

  auto statusField = MakeInspectField(
      "preprocessingStatus", "Status", "Preprocessing", InspectFieldType::Text, GetPreprocessingStatus(),
      [this]() -> InspectValue
      { return GetPreprocessingStatus(); });
  statusField->readOnly = true;
  fields.push_back(statusField);

  fields.push_back(MakeInspectField(
      "cancelPreprocessing", "Cancel preprocessing", "Preprocessing", InspectFieldType::Action,
      std::monostate{}, {},
      [this](const InspectValue &)
      { cancelRequested = true; }));

  fields.push_back(MakeInspectField(
      "saveOutputDirectory", "Save Output Folder", "Preprocessing", InspectFieldType::Directory,
      pendingSaveOutputDirectory,
      [this]() -> InspectValue
      { return pendingSaveOutputDirectory; },
      [this](const InspectValue &value)
      {
        if (const auto *path = std::get_if<std::string>(&value))
          pendingSaveOutputDirectory = *path;
      }));

  fields.push_back(MakeInspectField(
      "saveCurrentDataset", "Save current dataset...", "Preprocessing", InspectFieldType::Action,
      std::monostate{}, {},
      [this](const InspectValue &)
      { datasetSaveRequested = true; }));

  addFileField("loadDatasetMetadataPath", "Load Saved Dataset (metadata.json)", &pendingLoadMetadataPath);

  fields.push_back(MakeInspectField(
      "loadSavedDataset", "Load dataset", "Preprocessing", InspectFieldType::Action,
      std::monostate{}, {},
      [this](const InspectValue &)
      { datasetLoadRequested = true; }));

  // Rotation controls
  fields.push_back(MakeInspectField(
      "rotationEnabled", "Enabled", "Rotation", InspectFieldType::Boolean, rotationEnabled,
      [this]() -> InspectValue
      { return rotationEnabled; },
      [this](const InspectValue &value)
      {
        if (const auto *enabled = std::get_if<bool>(&value))
          rotationEnabled = *enabled;
      }));

  auto rotationSpeedField = MakeInspectField(
      "rotationSpeed", "Speed", "Rotation", InspectFieldType::Number, static_cast<double>(rotationSpeed),
      [this]() -> InspectValue
      { return static_cast<double>(rotationSpeed); },
      [this](const InspectValue &value)
      {
        if (const auto *speed = std::get_if<double>(&value))
          rotationSpeed = static_cast<float>(*speed);
      });
  rotationSpeedField->minimum = 0.0;
  rotationSpeedField->maximum = 5.0;
  rotationSpeedField->step = 0.01;
  fields.push_back(rotationSpeedField);

  return fields;
}
