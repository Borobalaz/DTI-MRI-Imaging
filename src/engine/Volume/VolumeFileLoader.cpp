#include "VolumeFileLoader.h"

#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#ifdef CONNECTOMICS_ENABLE_ITK_IO
#include <itkGDCMImageIO.h>
#include <itkGDCMImageIOFactory.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageIOBase.h>
#include <itkImageIOFactory.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageSeriesReader.h>
#include <itkMetaImageIOFactory.h>
#include <itkNiftiImageIOFactory.h>
#include <itkNrrdImageIOFactory.h>
#include <itkObjectFactoryBase.h>
#endif

namespace
{
  std::string g_lastVolumeLoaderError;

  void SetVolumeLoaderError(const std::string &message)
  {
    g_lastVolumeLoaderError = message;
    std::cerr << message << std::endl;
  }

  std::optional<VolumeFileHeader> TryReadHeader(const std::string &filePath)
  {
    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open())
    {
      return std::nullopt;
    }

    VolumeFileHeader header{};
    input.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!input.good())
    {
      return std::nullopt;
    }

    if (std::memcmp(header.magic, "VXA1", 4) != 0 || header.version != 1)
    {
      return std::nullopt;
    }

    return header;
  }

  std::optional<VolumeSeriesData> VolumeToSingleFrameSeries(const VolumeData &volume)
  {
    const VolumeMetadata &metadata = volume.GetMetadata();
    VolumeSeriesData series(
      metadata.dimensions.x,
      metadata.dimensions.y,
      metadata.dimensions.z,
      1,
      metadata.spacing,
      1.0f);

    series.GetVoxels() = volume.GetVoxels();
    return series;
  }

#ifdef CONNECTOMICS_ENABLE_ITK_IO
  void EnsureItkImageIoFactoriesRegistered()
  {
    static bool registered = false;
    if (registered)
    {
      return;
    }

    itk::ObjectFactoryBase::RegisterFactory(itk::NiftiImageIOFactory::New());
    itk::ObjectFactoryBase::RegisterFactory(itk::NrrdImageIOFactory::New());
    itk::ObjectFactoryBase::RegisterFactory(itk::MetaImageIOFactory::New());
    itk::ObjectFactoryBase::RegisterFactory(itk::GDCMImageIOFactory::New());
    registered = true;
  }

  std::optional<VolumeData> CopyItkImageToVolume(const itk::Image<float, 3> *image)
  {
    if (image == nullptr)
    {
      return std::nullopt;
    }

    const auto region = image->GetLargestPossibleRegion();
    const auto size = region.GetSize();
    if (size[0] == 0 || size[1] == 0 || size[2] == 0)
    {
      return std::nullopt;
    }

    const auto spacing = image->GetSpacing();
    VolumeData volume(
        static_cast<int>(size[0]),
        static_cast<int>(size[1]),
        static_cast<int>(size[2]),
        glm::vec3(
            static_cast<float>(spacing[0]),
            static_cast<float>(spacing[1]),
            static_cast<float>(spacing[2])));

    std::vector<float> &voxels = volume.GetVoxels();
    itk::ImageRegionConstIterator<itk::Image<float, 3>> it(image, region);

    size_t index = 0;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
      voxels[index++] = it.Get();
    }

    return volume;
  }

  std::optional<VolumeData> ReadScalarImageAsFloat(const std::string &filePath)
  {
    using ImageType = itk::Image<float, 3>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    try
    {
      typename ReaderType::Pointer reader = ReaderType::New();
      reader->SetFileName(filePath);
      reader->Update();
      return CopyItkImageToVolume(reader->GetOutput());
    }
    catch (const itk::ExceptionObject &ex)
    {
      SetVolumeLoaderError(std::string("ITK reader failed for '") + filePath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception &ex)
    {
      SetVolumeLoaderError(std::string("Reader exception for '") + filePath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(std::string("Unknown reader failure for '") + filePath + "'.");
      return std::nullopt;
    }
  }

  std::optional<VolumeSeriesData> CopyItkImage4ToSeries(const itk::Image<float, 4> *image)
  {
    if (image == nullptr)
    {
      return std::nullopt;
    }

    const auto region = image->GetLargestPossibleRegion();
    const auto size = region.GetSize();
    if (size[0] == 0 || size[1] == 0 || size[2] == 0 || size[3] == 0)
    {
      return std::nullopt;
    }

    const uint64_t voxelCountU64 =
        static_cast<uint64_t>(size[0]) *
        static_cast<uint64_t>(size[1]) *
        static_cast<uint64_t>(size[2]) *
        static_cast<uint64_t>(size[3]);
    if (voxelCountU64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    {
      SetVolumeLoaderError("4D image voxel count overflows addressable memory.");
      return std::nullopt;
    }

    const auto spacing = image->GetSpacing();
    VolumeSeriesData series(
      static_cast<int>(size[0]),
      static_cast<int>(size[1]),
      static_cast<int>(size[2]),
      static_cast<int>(size[3]),
      glm::vec3(
        static_cast<float>(spacing[0]),
        static_cast<float>(spacing[1]),
        static_cast<float>(spacing[2])),
      static_cast<float>(spacing[3]));

    std::vector<float> &voxels = series.GetVoxels();
    itk::ImageRegionConstIterator<itk::Image<float, 4>> it(image, region);

    size_t index = 0;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
      voxels[index++] = it.Get();
    }

    return series;
  }

  std::optional<VolumeSeriesData> ReadScalarImageSeriesAsFloat(const std::string &filePath)
  {
    using ImageType4D = itk::Image<float, 4>;
    using ReaderType4D = itk::ImageFileReader<ImageType4D>;

    try
    {
      typename ReaderType4D::Pointer reader = ReaderType4D::New();
      reader->SetFileName(filePath);
      reader->Update();
      return CopyItkImage4ToSeries(reader->GetOutput());
    }
    catch (const itk::ExceptionObject &ex)
    {
      SetVolumeLoaderError(std::string("ITK 4D reader failed for '") + filePath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception &ex)
    {
      SetVolumeLoaderError(std::string("4D reader exception for '") + filePath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(std::string("Unknown 4D reader failure for '") + filePath + "'.");
      return std::nullopt;
    }
  }

  std::optional<VolumeData> TryLoadImageFile(const std::string &filePath)
  {
    EnsureItkImageIoFactoriesRegistered();

    using IOComponentType = itk::IOComponentEnum;
    using IOPixelType = itk::IOPixelEnum;

    itk::ImageIOBase::Pointer imageIo =
        itk::ImageIOFactory::CreateImageIO(filePath.c_str(), itk::CommonEnums::IOFileMode::ReadMode);
    if (!imageIo)
    {
      SetVolumeLoaderError(std::string("ITK has no ImageIO for '") + filePath + "'.");
      return std::nullopt;
    }

    try
    {
      imageIo->SetFileName(filePath);
      imageIo->ReadImageInformation();
    }
    catch (const itk::ExceptionObject &ex)
    {
      SetVolumeLoaderError(std::string("ITK failed reading image info for '") + filePath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception &ex)
    {
      SetVolumeLoaderError(std::string("Image info exception for '") + filePath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(std::string("Unknown image info failure for '") + filePath + "'.");
      return std::nullopt;
    }

    if (imageIo->GetPixelType() != IOPixelType::SCALAR)
    {
      SetVolumeLoaderError(std::string("Unsupported pixel type for '") + filePath + "'. Only scalar volumes are supported.");
      return std::nullopt;
    }

    if (imageIo->GetNumberOfDimensions() == 4)
    {
      SetVolumeLoaderError(
          std::string("4D medical image detected for '") + filePath +
          "'. Use VolumeFileLoader::LoadSeries or LoadFrame for 4D inputs.");
      return std::nullopt;
    }

    if (imageIo->GetNumberOfDimensions() != 3)
    {
      SetVolumeLoaderError(std::string("Unsupported dimension for '") + filePath + "'. Only 3D volumes are supported.");
      return std::nullopt;
    }

    switch (imageIo->GetComponentType())
    {
    case IOComponentType::UCHAR:
    case IOComponentType::CHAR:
    case IOComponentType::USHORT:
    case IOComponentType::SHORT:
    case IOComponentType::UINT:
    case IOComponentType::INT:
    case IOComponentType::ULONG:
    case IOComponentType::LONG:
    case IOComponentType::ULONGLONG:
    case IOComponentType::LONGLONG:
    case IOComponentType::FLOAT:
    case IOComponentType::DOUBLE:
      return ReadScalarImageAsFloat(filePath);
    default:
      break;
    }

    SetVolumeLoaderError(std::string("Unsupported component type in '") + filePath + "'.");
    return std::nullopt;
  }

  std::optional<VolumeData> TryLoadDicomSeriesDirectory(const std::string &directoryPath)
  {
    if (!std::filesystem::is_directory(directoryPath))
    {
      return std::nullopt;
    }

    using ImageType = itk::Image<float, 3>;
    using SeriesReaderType = itk::ImageSeriesReader<ImageType>;

    try
    {
      itk::GDCMImageIO::Pointer dicomIo = itk::GDCMImageIO::New();
      itk::GDCMSeriesFileNames::Pointer fileNamesGenerator = itk::GDCMSeriesFileNames::New();
      fileNamesGenerator->SetUseSeriesDetails(true);
      fileNamesGenerator->SetDirectory(directoryPath);

      const std::vector<std::string> seriesUids = fileNamesGenerator->GetSeriesUIDs();
      if (seriesUids.empty())
      {
        return std::nullopt;
      }

      const std::vector<std::string> dicomFileNames =
          fileNamesGenerator->GetFileNames(seriesUids.front());
      if (dicomFileNames.empty())
      {
        return std::nullopt;
      }

      typename SeriesReaderType::Pointer reader = SeriesReaderType::New();
      reader->SetImageIO(dicomIo);
      reader->SetFileNames(dicomFileNames);
      reader->Update();

      return CopyItkImageToVolume(reader->GetOutput());
    }
    catch (const itk::ExceptionObject &ex)
    {
      SetVolumeLoaderError(std::string("DICOM series read failed for '") + directoryPath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception &ex)
    {
      SetVolumeLoaderError(std::string("DICOM series exception for '") + directoryPath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(std::string("Unknown DICOM series failure for '") + directoryPath + "'.");
      return std::nullopt;
    }
  }

  std::optional<VolumeData> TryLoadMedicalVolumeWithItk(const std::string &filePath)
  {
    if (std::filesystem::is_directory(filePath))
    {
      return TryLoadDicomSeriesDirectory(filePath);
    }

    return TryLoadImageFile(filePath);
  }

  std::optional<VolumeSeriesData> TryLoadMedicalSeriesWithItk(const std::string &filePath)
  {
    EnsureItkImageIoFactoriesRegistered();

    if (std::filesystem::is_directory(filePath))
    {
      if (const auto dicomVolume = TryLoadDicomSeriesDirectory(filePath))
      {
        return VolumeToSingleFrameSeries(*dicomVolume);
      }
      return std::nullopt;
    }

    itk::ImageIOBase::Pointer imageIo =
        itk::ImageIOFactory::CreateImageIO(filePath.c_str(), itk::CommonEnums::IOFileMode::ReadMode);
    if (!imageIo)
    {
      SetVolumeLoaderError(std::string("ITK has no ImageIO for '") + filePath + "'.");
      return std::nullopt;
    }

    try
    {
      imageIo->SetFileName(filePath);
      imageIo->ReadImageInformation();
    }
    catch (const itk::ExceptionObject &ex)
    {
      SetVolumeLoaderError(std::string("ITK failed reading image info for '") + filePath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception &ex)
    {
      SetVolumeLoaderError(std::string("Image info exception for '") + filePath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(std::string("Unknown image info failure for '") + filePath + "'.");
      return std::nullopt;
    }

    if (imageIo->GetPixelType() != itk::IOPixelEnum::SCALAR)
    {
      SetVolumeLoaderError(
        std::string("Unsupported pixel type for '") + filePath + "'. Only scalar volumes are supported.");
      return std::nullopt;
    }

    const unsigned int dimensions = imageIo->GetNumberOfDimensions();
    if (dimensions == 4)
    {
      return ReadScalarImageSeriesAsFloat(filePath);
    }

    if (dimensions == 3)
    {
      if (const auto volume = ReadScalarImageAsFloat(filePath))
      {
        return VolumeToSingleFrameSeries(*volume);
      }
      return std::nullopt;
    }

    SetVolumeLoaderError(
      std::string("Unsupported dimension for '") + filePath + "'. Only 3D and 4D scalar volumes are supported.");
    return std::nullopt;
  }
#endif
}

/**
 * @brief Load a volume file's data into a VolumeData object. 
 *        Only accept VXA volumes whose field type is float. 
 * 
 * @param filePath 
 * @return std::optional<VolumeData> - VolumeData object pointer
 */
std::optional<VolumeData> VolumeFileLoader::Load(const std::string &filePath)
{
  g_lastVolumeLoaderError.clear();

  if (TryReadHeader(filePath).has_value())
  {
    if (const auto scalarF32 = LoadTypedVxa(filePath))
    {
      return *scalarF32;
    }

    return std::nullopt;
  }

  std::cout << "File '" << filePath << "' does not have a valid VXA header. Attempting medical format loading...\n";
  return LoadTyped(filePath);
}

std::optional<VolumeSeriesData> VolumeFileLoader::LoadSeries(const std::string &filePath)
{
  g_lastVolumeLoaderError.clear();

  if (TryReadHeader(filePath).has_value())
  {
    if (const auto scalarF32 = LoadTypedVxa(filePath))
    {
      return VolumeToSingleFrameSeries(*scalarF32);
    }

    return std::nullopt;
  }

  return TryLoadMedicalSeriesFormat(filePath);
}

std::optional<VolumeData> VolumeFileLoader::LoadFrame(const std::string &filePath, int frameIndex)
{
  if (frameIndex < 0)
  {
    SetVolumeLoaderError("Frame index must be non-negative.");
    return std::nullopt;
  }

  const auto series = LoadSeries(filePath);
  if (!series.has_value())
  {
    return std::nullopt;
  }

  if (frameIndex >= series->GetFrameCount())
  {
    SetVolumeLoaderError(
      std::string("Requested frame index ") + std::to_string(frameIndex) +
      " is out of range for file '" + filePath +
      "' with frame count " + std::to_string(series->GetFrameCount()) + ".");
    return std::nullopt;
  }

  try
  {
    return series->ExtractFrame(frameIndex);
  }
  catch (const std::exception &ex)
  {
    SetVolumeLoaderError(std::string("Failed to extract frame for '") + filePath + "': " + ex.what());
    return std::nullopt;
  }
}

/**
 * @brief Write a VolumeData object's voxel data into a VXA file.
 * 
 * @param filePath 
 * @param volume 
 * @return true - Save successful
 * @return false - Save unsuccessful
 */
bool VolumeFileLoader::Save(const std::string &filePath, const VolumeData &volume)
{
  // Create directories of output path
  const std::filesystem::path path(filePath);
  if (path.has_parent_path())
  {
    std::filesystem::create_directories(path.parent_path());
  }

  // Open output file
  std::ofstream output(filePath, std::ios::binary);
  if (!output.is_open())
  {
    return false;
  }

  // Get metadata to file header
  const VolumeMetadata &metadata = volume.GetMetadata();
  VolumeFileHeader header{};
  header.width = static_cast<uint32_t>(metadata.dimensions.x);
  header.height = static_cast<uint32_t>(metadata.dimensions.y);
  header.depth = static_cast<uint32_t>(metadata.dimensions.z);
  header.spacingX = metadata.spacing.x;
  header.spacingY = metadata.spacing.y;
  header.spacingZ = metadata.spacing.z;

  // Write file header
  output.write(reinterpret_cast<const char *>(&header), sizeof(header));
  
  // Write voxel data
  const std::vector<float> &voxels = volume.GetVoxels();
  output.write(reinterpret_cast<const char *>(voxels.data()), static_cast<std::streamsize>(voxels.size() * sizeof(float)));
  
  // Check output
  return output.good();
}

/**
 * @brief Load a volume that is either a float VXA format or a medical format like niftii or dicom.
 * 
 * @param filePath 
 * @return std::optional<VolumeData> 
 */
std::optional<VolumeData> VolumeFileLoader::LoadTyped(const std::string &filePath)
{
  if (const auto vxa = LoadTypedVxa(filePath))
  {
    return vxa;
  }

  return TryLoadMedicalFormat(filePath);
}

/**
 * @brief Load a VXA volume file into a VolumeData object.
 * 
 * @param filePath 
 * @return std::optional<VolumeData> 
 */
std::optional<VolumeData> VolumeFileLoader::LoadTypedVxa(const std::string &filePath)
{
  // Open file
  std::ifstream input(filePath, std::ios::binary);
  if (!input.is_open())
  {
    return std::nullopt;
  }

  // Read header
  VolumeFileHeader header{};
  input.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (!input.good())
  {
    return std::nullopt;
  }

  // Only accept valid VXA files
  if (std::memcmp(header.magic, "VXA1", 4) != 0 ||
      header.version != 1)
  {
    return std::nullopt;
  }

  // Only accept not empty 3D volumes
  if (header.width == 0 || header.height == 0 || header.depth == 0)
  {
    return std::nullopt;
  }

  // Discard volumes that are too big
  const uint64_t voxelCount =
      static_cast<uint64_t>(header.width) *
      static_cast<uint64_t>(header.height) *
      static_cast<uint64_t>(header.depth);
  if (voxelCount > static_cast<uint64_t>(std::numeric_limits<size_t>::max() / sizeof(float)))
  {
    return std::nullopt;
  }

  // Validate file size
  input.seekg(0, std::ios::end);
  const std::streamoff fileSize = input.tellg();
  if (fileSize < 0)
  {
    return std::nullopt;
  }

  // Expected file size = header size + voxel data size. Reject files that don't match this exactly to avoid partial loading or overflow.
  const uint64_t expectedSize = sizeof(VolumeFileHeader) + voxelCount * sizeof(float);
  if (static_cast<uint64_t>(fileSize) != expectedSize)
  {
    return std::nullopt;
  }

  input.seekg(static_cast<std::streamoff>(sizeof(VolumeFileHeader)), std::ios::beg);

  // Create VolumeData object
  VolumeData volume(
      static_cast<int>(header.width),
      static_cast<int>(header.height),
      static_cast<int>(header.depth),
      glm::vec3(header.spacingX, header.spacingY, header.spacingZ));

  // Write voxel data to VolumeData
  std::vector<float> &voxels = volume.GetVoxels();
  input.read(reinterpret_cast<char *>(voxels.data()), static_cast<std::streamsize>(voxels.size() * sizeof(float)));
  
  // Return success
  if (!input.good())
  {
    return std::nullopt;
  }

  return volume;
}

std::string VolumeFileLoader::GetLastError()
{
  return g_lastVolumeLoaderError;
}

std::optional<VolumeData> VolumeFileLoader::TryLoadMedicalFormat(const std::string &filePath)
{
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  return TryLoadMedicalVolumeWithItk(filePath);
#else
  SetVolumeLoaderError(
      std::string("Medical image loading is disabled at build time; cannot load '") + filePath + "'.");
  return std::nullopt;
#endif
}

std::optional<VolumeSeriesData> VolumeFileLoader::TryLoadMedicalSeriesFormat(const std::string &filePath)
{
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  return TryLoadMedicalSeriesWithItk(filePath);
#else
  SetVolumeLoaderError(
    std::string("Medical image loading is disabled at build time; cannot load series '") + filePath + "'.");
  return std::nullopt;
#endif
}
