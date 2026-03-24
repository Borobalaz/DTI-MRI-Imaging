#include "VolumeFileLoader.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <exception>

#ifdef CONNECTOMICS_ENABLE_ITK_IO
#include <itkGDCMImageIO.h>
#include <itkGDCMImageIOFactory.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageIOBase.h>
#include <itkImageIOFactory.h>
#include <itkImageFileReader.h>
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

  void SetVolumeLoaderError(const std::string& message)
  {
    g_lastVolumeLoaderError = message;
    std::cerr << message << std::endl;
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
#endif

  /**
   * @brief Tries to read the header from a volume file.
   * 
   * @param filePath 
   * @return std::optional<VolumeFileHeader> 
   */
  std::optional<VolumeFileHeader> TryReadHeader(const std::string& filePath)
  {
    std::ifstream input(filePath, std::ios::binary);
    if (!input.is_open())
    {
      return std::nullopt;
    }

    VolumeFileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
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

  /**
   * @brief Copies an ITK image to a volume data structure.
   * 
   * @tparam TOutputVoxel 
   * @tparam TInputVoxel 
   * @param image 
   * @return std::optional<VolumeData<TOutputVoxel>> 
   */
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  template <typename TOutputVoxel, typename TInputVoxel>
  std::optional<VolumeData<TOutputVoxel>> CopyItkImageToVolume(
    const itk::Image<TInputVoxel, 3>* image)
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

    VolumeData<TOutputVoxel> volume(
      static_cast<int>(size[0]),
      static_cast<int>(size[1]),
      static_cast<int>(size[2]),
      glm::vec3(
        static_cast<float>(spacing[0]),
        static_cast<float>(spacing[1]),
        static_cast<float>(spacing[2])));

    std::vector<TOutputVoxel>& voxels = volume.GetVoxels();
    itk::ImageRegionConstIterator<itk::Image<TInputVoxel, 3>> it(image, region);

    size_t index = 0;
    for (it.GoToBegin(); !it.IsAtEnd(); ++it)
    {
      voxels[index++] = static_cast<TOutputVoxel>(it.Get());
    }

    return volume;
  }
#endif

  /**
   * @brief Tries to load a medical image file (like NIfTI) or DICOM series using ITK.
   * 
   * @tparam TOutputVoxel 
   * @tparam TInputVoxel 
   * @param filePath 
   * @return std::optional<VolumeData<TOutputVoxel>> 
   */
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  template <typename TOutputVoxel, typename TInputVoxel>
  std::optional<VolumeData<TOutputVoxel>> ReadScalarImageAs(const std::string& filePath)
  {
    using ImageType = itk::Image<TInputVoxel, 3>;
    using ReaderType = itk::ImageFileReader<ImageType>;

    try
    {
      typename ReaderType::Pointer reader = ReaderType::New();
      reader->SetFileName(filePath);
      reader->Update();
      return CopyItkImageToVolume<TOutputVoxel, TInputVoxel>(reader->GetOutput());
    }
    catch (const itk::ExceptionObject& ex)
    {
      SetVolumeLoaderError(
        std::string("ITK reader failed for '") + filePath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception& ex)
    {
      SetVolumeLoaderError(
        std::string("Reader exception for '") + filePath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(
        std::string("Unknown reader failure for '") + filePath + "'.");
      return std::nullopt;
    }
  }
#endif

  /**
   * @brief Tries to load an image file using ITK.
   * 
   * @tparam TOutputVoxel 
   * @param filePath 
   * @return std::optional<VolumeData<TOutputVoxel>> 
   */
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  template <typename TOutputVoxel>
  std::optional<VolumeData<TOutputVoxel>> TryLoadImageFile(const std::string& filePath)
  {
#ifdef CONNECTOMICS_ENABLE_ITK_IO
    EnsureItkImageIoFactoriesRegistered();
#endif

    using IOComponentType = itk::IOComponentEnum;
    using IOPixelType = itk::IOPixelEnum;

    itk::ImageIOBase::Pointer imageIo =
      itk::ImageIOFactory::CreateImageIO(filePath.c_str(), itk::CommonEnums::IOFileMode::ReadMode);
    if (!imageIo)
    {
      SetVolumeLoaderError(
        std::string("ITK has no ImageIO for '") + filePath + "'.");
      return std::nullopt;
    }

    try
    {
      imageIo->SetFileName(filePath);
      imageIo->ReadImageInformation();
    }
    catch (const itk::ExceptionObject& ex)
    {
      SetVolumeLoaderError(
        std::string("ITK failed reading image info for '") + filePath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception& ex)
    {
      SetVolumeLoaderError(
        std::string("Image info exception for '") + filePath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(
        std::string("Unknown image info failure for '") + filePath + "'.");
      return std::nullopt;
    }

    const IOPixelType pixelType = imageIo->GetPixelType();
    if (pixelType != IOPixelType::SCALAR)
    {
      SetVolumeLoaderError(
        std::string("Unsupported pixel type for '") + filePath + "'. Only scalar volumes are supported.");
      return std::nullopt;
    }

    const unsigned int dimension = imageIo->GetNumberOfDimensions();
    if (dimension != 3)
    {
      SetVolumeLoaderError(
        std::string("Unsupported dimension for '") + filePath + "'. Only 3D volumes are supported.");
      return std::nullopt;
    }

    const IOComponentType componentType = imageIo->GetComponentType();
    switch (componentType)
    {
      case IOComponentType::UCHAR:
        return ReadScalarImageAs<TOutputVoxel, unsigned char>(filePath);
      case IOComponentType::CHAR:
        return ReadScalarImageAs<TOutputVoxel, char>(filePath);
      case IOComponentType::USHORT:
        return ReadScalarImageAs<TOutputVoxel, unsigned short>(filePath);
      case IOComponentType::SHORT:
        return ReadScalarImageAs<TOutputVoxel, short>(filePath);
      case IOComponentType::UINT:
        return ReadScalarImageAs<TOutputVoxel, unsigned int>(filePath);
      case IOComponentType::INT:
        return ReadScalarImageAs<TOutputVoxel, int>(filePath);
      case IOComponentType::ULONG:
        return ReadScalarImageAs<TOutputVoxel, unsigned long>(filePath);
      case IOComponentType::LONG:
        return ReadScalarImageAs<TOutputVoxel, long>(filePath);
      case IOComponentType::ULONGLONG:
        return ReadScalarImageAs<TOutputVoxel, unsigned long long>(filePath);
      case IOComponentType::LONGLONG:
        return ReadScalarImageAs<TOutputVoxel, long long>(filePath);
      case IOComponentType::FLOAT:
        return ReadScalarImageAs<TOutputVoxel, float>(filePath);
      case IOComponentType::DOUBLE:
        return ReadScalarImageAs<TOutputVoxel, double>(filePath);
      default:
        break;
    }

    SetVolumeLoaderError(
      std::string("Unsupported component type in '") + filePath + "'.");
    return std::nullopt;
  }
#endif

  /**
   * @brief Tries to load a DICOM series from a directory.
   * 
   * @tparam TVoxel 
   * @param directoryPath 
   * @return std::optional<VolumeData<TVoxel>> 
   */
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  template <typename TVoxel>
  std::optional<VolumeData<TVoxel>> TryLoadDicomSeriesDirectory(const std::string& directoryPath)
  {
    if (!std::filesystem::is_directory(directoryPath))
    {
      return std::nullopt;
    }

    using ImageType = itk::Image<TVoxel, 3>;
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

      return CopyItkImageToVolume<TVoxel, TVoxel>(reader->GetOutput());
    }
    catch (const itk::ExceptionObject& ex)
    {
      SetVolumeLoaderError(
        std::string("DICOM series read failed for '") + directoryPath + "': " + ex.GetDescription());
      return std::nullopt;
    }
    catch (const std::exception& ex)
    {
      SetVolumeLoaderError(
        std::string("DICOM series exception for '") + directoryPath + "': " + ex.what());
      return std::nullopt;
    }
    catch (...)
    {
      SetVolumeLoaderError(
        std::string("Unknown DICOM series failure for '") + directoryPath + "'.");
      return std::nullopt;
    }
  }
#endif

  template <typename TVoxel>
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  std::optional<VolumeData<TVoxel>> TryLoadMedicalVolumeWithItk(const std::string& filePath)
  {
    if (std::filesystem::is_directory(filePath))
    {
      return TryLoadDicomSeriesDirectory<TVoxel>(filePath);
    }

    return TryLoadImageFile<TVoxel>(filePath);
  }
#else
  std::optional<VolumeData<TVoxel>> TryLoadMedicalVolumeWithItk(const std::string& filePath)
  {
    SetVolumeLoaderError("Medical image loading is disabled at build time");
    return std::nullopt;
  }
#endif
}

/**
 * @brief 
 * 
 * @param filePath 
 * @return std::optional<LoadedVolumeVariant> 
 */
std::optional<LoadedVolumeVariant> VolumeFileLoader::Load(const std::string& filePath)
{
  g_lastVolumeLoaderError.clear();

  const std::optional<VolumeFileHeader> header = TryReadHeader(filePath);
  if (header.has_value())
  {
    if (const auto matrixF32 = LoadTypedVxa<glm::mat3>(filePath))
    {
      return LoadedVolumeVariant(*matrixF32);
    }

    if (const auto scalarU8 = LoadTypedVxa<uint8_t>(filePath))
    {
      return LoadedVolumeVariant(*scalarU8);
    }

    if (const auto scalarU16 = LoadTypedVxa<uint16_t>(filePath))
    {
      return LoadedVolumeVariant(*scalarU16);
    }

    if (const auto scalarF32 = LoadTypedVxa<float>(filePath))
    {
      return LoadedVolumeVariant(*scalarF32);
    }

    return std::nullopt;
  }
  else std::cout << "File '" << filePath << "' does not have a valid VXA header. Attempting medical format loading...\n";

  if (const auto scalarF32 = LoadTyped<float>(filePath))
  {
    return LoadedVolumeVariant(*scalarF32);
  }

  if (const auto scalarU16 = LoadTyped<uint16_t>(filePath))
  {
    return LoadedVolumeVariant(*scalarU16);
  }

  if (const auto scalarU8 = LoadTyped<uint8_t>(filePath))
  {
    return LoadedVolumeVariant(*scalarU8);
  }

  return std::nullopt;
}

std::string VolumeFileLoader::GetLastError()
{
  return g_lastVolumeLoaderError;
}

/**
 * @brief Tries to load a medical image file (like NIfTI) or DICOM series using ITK.
 * 
 * @tparam uint8_t 
 * @param filePath 
 * @return std::optional<VolumeData<uint8_t>> 
 */
template <>
std::optional<VolumeData<uint8_t>> VolumeFileLoader::TryLoadMedicalFormat<uint8_t>(
  const std::string& filePath)
{
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  return TryLoadMedicalVolumeWithItk<uint8_t>(filePath);
#else
  SetVolumeLoaderError(
    std::string("Medical image loading is disabled at build time; cannot load '") + filePath + "'.");
  return std::nullopt;
#endif
}

/**
 * @brief Tries to load a medical image file (like NIfTI) or DICOM series using ITK.
 * 
 * @tparam uint16_t 
 * @param filePath 
 * @return std::optional<VolumeData<uint16_t>> 
 */
template <>
std::optional<VolumeData<uint16_t>> VolumeFileLoader::TryLoadMedicalFormat<uint16_t>(
  const std::string& filePath)
{
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  return TryLoadMedicalVolumeWithItk<uint16_t>(filePath);
#else
  SetVolumeLoaderError(
    std::string("Medical image loading is disabled at build time; cannot load '") + filePath + "'.");
  return std::nullopt;
#endif
}

/**
 * @brief Tries to load a medical image file (like NIfTI) or DICOM series using ITK.
 * 
 * @tparam float 
 * @param filePath 
 * @return std::optional<VolumeData<float>> 
 */
template <>
std::optional<VolumeData<float>> VolumeFileLoader::TryLoadMedicalFormat<float>(
  const std::string& filePath)
{
#ifdef CONNECTOMICS_ENABLE_ITK_IO
  return TryLoadMedicalVolumeWithItk<float>(filePath);
#else
  SetVolumeLoaderError(
    std::string("Medical image loading is disabled at build time; cannot load '") + filePath + "'.");
  return std::nullopt;
#endif
}
