#include "VolumeFileLoader.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef CONNECTOMICS_ENABLE_ITK_IO
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageIOBase.h>
#include <itkImageIOFactory.h>
#include <itkImageFileReader.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageSeriesReader.h>
#endif

namespace
{
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

  /**
   * @brief Tries to load a medical image file (like NIfTI) or DICOM series using ITK.
   * 
   * @tparam TOutputVoxel 
   * @tparam TInputVoxel 
   * @param filePath 
   * @return std::optional<VolumeData<TOutputVoxel>> 
   */
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
    catch (...)
    {
      return std::nullopt;
    }
  }

  /**
   * @brief Tries to load an image file using ITK.
   * 
   * @tparam TOutputVoxel 
   * @param filePath 
   * @return std::optional<VolumeData<TOutputVoxel>> 
   */
  template <typename TOutputVoxel>
  std::optional<VolumeData<TOutputVoxel>> TryLoadImageFile(const std::string& filePath)
  {
    using IOComponentType = itk::IOComponentEnum;
    using IOPixelType = itk::IOPixelEnum;

    itk::ImageIOBase::Pointer imageIo =
      itk::ImageIOFactory::CreateImageIO(filePath.c_str(), itk::CommonEnums::IOFileMode::ReadMode);
    if (!imageIo)
    {
      return std::nullopt;
      std::cerr << "ITK does not have a suitable ImageIO for file '" << filePath << "'.\n";
    }

    try
    {
      imageIo->SetFileName(filePath);
      imageIo->ReadImageInformation();
    }
    catch (...)
    {
      return std::nullopt;
      std::cerr << "ITK failed to read image information from file '" << filePath << "'.\n";
    }

    const IOPixelType pixelType = imageIo->GetPixelType();
    if (pixelType != IOPixelType::SCALAR)
    {
      return std::nullopt;
      std::cerr << "ITK loaded image from file '" << filePath
                << "' does not have scalar pixel type. Only scalar volumes are supported.\n";
    }

    const unsigned int dimension = imageIo->GetNumberOfDimensions();
    if (dimension != 3)
    {
      return std::nullopt;
      std::cerr << "ITK loaded image from file '" << filePath
                << "' does not have 3 dimensions. Only 3D volumes are supported.\n";
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

    std::cerr << "ITK loaded image from file '" << filePath
              << "' has unsupported component type. Only scalar volumes with standard component types are supported.\n";
    return std::nullopt;
  }

  /**
   * @brief Tries to load a DICOM series from a directory.
   * 
   * @tparam TVoxel 
   * @param directoryPath 
   * @return std::optional<VolumeData<TVoxel>> 
   */
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
    catch (...)
    {
      return std::nullopt;
    }
  }

  template <typename TVoxel>
  std::optional<VolumeData<TVoxel>> TryLoadMedicalVolumeWithItk(const std::string& filePath)
  {
    if (std::filesystem::is_directory(filePath))
    {
      return TryLoadDicomSeriesDirectory<TVoxel>(filePath);
    }

    return TryLoadImageFile<TVoxel>(filePath);
  }
}

/**
 * @brief 
 * 
 * @param filePath 
 * @return std::optional<LoadedVolumeVariant> 
 */
std::optional<LoadedVolumeVariant> VolumeFileLoader::Load(const std::string& filePath)
{
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
  (void)filePath;
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
  (void)filePath;
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
  (void)filePath;
  return std::nullopt;
#endif
}
