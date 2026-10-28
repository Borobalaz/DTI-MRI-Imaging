#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// Internal numerics shared by the fODF response and CSD stages (Tournier 2007). No Eigen types
// appear here; Eigen is confined to FodfCsdMath.cpp behind the pimpls.
namespace FodfCsdMath
{
  constexpr float  kB0Threshold           = 50.0f;
  constexpr double kResponseFitRidge      = 1e-6;   // absolute, on the joint zonal normal matrix
  constexpr double kCsdRelativeRidge      = 1e-12;  // x trace/n
  constexpr unsigned int kHrIcosphereLevel   = 3;   // 642 -> 321 hemisphere
  constexpr unsigned int kPeakIcosphereLevel = 4;   // 2562
  constexpr double kHemisphereEpsilon     = 1e-6;
  constexpr int    kInitialLmax           = 4;
  constexpr int    kMinResponseVoxels     = 10;
  constexpr size_t kMinResponseDirections = 6;
  constexpr double kPolarAngleBinWidth    = 1e-3;   // |cos theta| bin width for the distinct-angle check

  // Y_l0(+z) = sqrt((2l+1)/(4 pi)), the delta_l0 of r_l = R_l0 / delta_l0.
  double ZonalDelta(int l);

  struct Rotation3 { double m[3][3]; };                       // row-major: out_i = sum_j m[i][j] * v_j
  // Rotation taking the (sign-agnostic) axis onto +z; the axis need not be unit length but must be non-zero.
  Rotation3  RotationTakingAxisToZ(const glm::dvec3& axis);
  glm::dvec3 Rotate(const Rotation3& rotation, const glm::dvec3& v);

  // One direction per antipodal pair of the level-3 icosphere (321 directions), in icosphere order.
  std::vector<glm::dvec3> BuildHrHemisphere();

  struct AxialResponse
  {
    int lmax = -1;
    std::vector<double> zonal;          // R_l0, element l/2, size lmax/2+1
    std::vector<double> rotational;     // r_l = R_l0 / delta_l0, same indexing
    size_t voxelCount = 0;              // voxels whose rows entered the joint fit
    size_t failedVoxelCount = 0;        // input voxels rejected (non-finite signal or degenerate axis)
    size_t rowCount = 0;                // voxelCount * shellDirs.size()
    size_t distinctPolarAngles = 0;     // distinct |cos theta| bins among the rotated rows
    std::string error;                  // empty on success
    bool IsValid() const;               // error.empty() && lmax >= 0 && voxelCount > 0
  };

  // Joint axially-symmetric response fit: stacks every voxel's rotated rows and solves only the
  // lmax/2+1 zonal unknowns.
  AxialResponse FitAxialResponse(const std::vector<glm::dvec3>& shellDirs,
                                 const std::vector<double>& signals,   // voxel-major, axes.size() * shellDirs.size()
                                 const std::vector<glm::dvec3>& axes,  // one per voxel, any sign
                                 int lmax);

  // Per-voxel CSD with iterative Tikhonov non-negativity on the HR hemisphere. Immutable after
  // construction; each thread solves through its own Workspace.
  class CsdSolver
  {
  public:
    struct Settings { int lmax; double tau; double lambda; int maxIterations; };
    enum class VoxelStatus { Converged, NotConverged, Underdetermined, SolveFailed };
    struct VoxelResult { VoxelStatus status; int iterations; };

    class Workspace
    {
    public:
      explicit Workspace(const CsdSolver& solver);
      ~Workspace();
      Workspace(Workspace&&) noexcept;
      Workspace& operator=(Workspace&&) noexcept;
    private:
      friend class CsdSolver;
      struct Impl;
      std::unique_ptr<Impl> impl;
    };

    CsdSolver(const std::vector<glm::dvec3>& shellDirs,
              const std::vector<double>& rotational,   // r_l at element l/2
              const Settings& settings);               // throws std::runtime_error
    ~CsdSolver();
    CsdSolver(const CsdSolver&) = delete;
    CsdSolver& operator=(const CsdSolver&) = delete;

    int    GetLmax() const;
    int    GetCoefficientCount() const;   // RealSH::CoefficientCount(lmax)
    size_t GetShellCount() const;
    size_t GetHrCount() const;
    double GetEffectiveLambda() const;
    bool   IsSuperResolved() const;       // GetCoefficientCount() > GetShellCount()

    // signal: GetShellCount() values (S/S0) in shellDirs order; coefficientsOut: GetCoefficientCount() doubles in
    // RealSH::Index order. Const; safe concurrently with distinct Workspaces.
    VoxelResult SolveVoxel(const double* signal, Workspace& workspace, double* coefficientsOut) const;

  private:
    struct Impl;
    std::unique_ptr<const Impl> impl;
  };

  // Up to maxPeaks antipodally-symmetric local maxima of the SH function over sampleDirs
  // (neighbourhood = directions within neighbourAngleDeg), strongest first, separated by at least
  // separationDeg.
  std::vector<glm::dvec3> FindPeaks(const double* coefficients, int lmax,
                                    const std::vector<glm::dvec3>& sampleDirs,
                                    size_t maxPeaks, double neighbourAngleDeg, double separationDeg);

  std::string FormatScientific(double value);          // precision 3
  std::string FormatFixed(double value, int decimals);
}
