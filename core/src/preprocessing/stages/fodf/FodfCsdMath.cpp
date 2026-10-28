#include "FodfCsdMath.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <glm/gtc/constants.hpp>

#include "Preprocessing/stages/fodf/FodfPreprocessingStages.h"
#include "foundation/log/EngineLog.h"
#include "scene/geometry/IcosphereGeometry.h"
#include "scene/spherical_harmonics/RealSH.h"

namespace FodfCsdMath
{
  namespace
  {
    bool IsSupportedResponseOrder(int lmax)
    {
      return lmax >= 0 && lmax <= RealSH::kSHMaxOrder && lmax % 2 == 0;
    }

    bool IsSupportedCsdOrder(int lmax)
    {
      return lmax >= 2 && lmax <= RealSH::kSHMaxOrder && lmax % 2 == 0;
    }

    std::vector<glm::dvec3> NormalizedDirections(const std::vector<glm::vec3>& directions)
    {
      std::vector<glm::dvec3> unitDirections;
      unitDirections.reserve(directions.size());
      for (const glm::vec3& direction : directions)
      {
        unitDirections.push_back(glm::normalize(glm::dvec3(direction)));
      }
      return unitDirections;
    }
  }

  double ZonalDelta(int l)
  {
    return std::sqrt((2.0 * l + 1.0) / (4.0 * glm::pi<double>()));
  }

  Rotation3 RotationTakingAxisToZ(const glm::dvec3& axis)
  {
    glm::dvec3 a = axis / glm::length(axis);
    // Even-l SH and the signal are antipodally symmetric, so flipping keeps 1 + a.z >= 1 and the
    // Rodrigues form below away from its singularity at a = -z.
    if (a.z < 0.0)
    {
      a = -a;
    }

    const double h = 1.0 / (1.0 + a.z);
    Rotation3 rotation{};
    rotation.m[0][0] = 1.0 - h * a.x * a.x;
    rotation.m[0][1] = -h * a.x * a.y;
    rotation.m[0][2] = -a.x;
    rotation.m[1][0] = -h * a.x * a.y;
    rotation.m[1][1] = 1.0 - h * a.y * a.y;
    rotation.m[1][2] = -a.y;
    rotation.m[2][0] = a.x;
    rotation.m[2][1] = a.y;
    rotation.m[2][2] = 1.0 - h * (a.x * a.x + a.y * a.y);
    return rotation;
  }

  glm::dvec3 Rotate(const Rotation3& rotation, const glm::dvec3& v)
  {
    return glm::dvec3(rotation.m[0][0] * v.x + rotation.m[0][1] * v.y + rotation.m[0][2] * v.z,
                      rotation.m[1][0] * v.x + rotation.m[1][1] * v.y + rotation.m[1][2] * v.z,
                      rotation.m[2][0] * v.x + rotation.m[2][1] * v.y + rotation.m[2][2] * v.z);
  }

  std::vector<glm::dvec3> BuildHrHemisphere()
  {
    const std::vector<glm::dvec3> all = NormalizedDirections(IcosphereGeometry::GenerateDirections(kHrIcosphereLevel));
    std::vector<glm::dvec3> hemisphere;
    hemisphere.reserve(all.size() / 2);
    for (const glm::dvec3& d : all)
    {
      const bool onEquator = std::abs(d.z) <= kHemisphereEpsilon;
      const bool keep = d.z > kHemisphereEpsilon ||
                        (onEquator && d.y > kHemisphereEpsilon) ||
                        (onEquator && std::abs(d.y) <= kHemisphereEpsilon && d.x > 0.0);
      if (keep)
      {
        hemisphere.push_back(d);
      }
    }

    if (hemisphere.size() != all.size() / 2)
    {
      throw std::logic_error("fODF CSD: HR hemisphere dedupe produced " + std::to_string(hemisphere.size()) +
                             " directions (expected " + std::to_string(all.size() / 2) + ").");
    }
    return hemisphere;
  }

  bool AxialResponse::IsValid() const
  {
    return error.empty() && lmax >= 0 && voxelCount > 0;
  }

  AxialResponse FitAxialResponse(const std::vector<glm::dvec3>& shellDirs,
                                 const std::vector<double>& signals,
                                 const std::vector<glm::dvec3>& axes,
                                 int lmax)
  {
    AxialResponse response;
    if (!IsSupportedResponseOrder(lmax))
    {
      response.error = "invalid lmax " + std::to_string(lmax);
      return response;
    }
    if (shellDirs.size() < kMinResponseDirections)
    {
      response.error = "shell has " + std::to_string(shellDirs.size()) + " directions; at least 6 are needed";
      return response;
    }
    if (signals.size() != axes.size() * shellDirs.size())
    {
      response.error = "signal/axis size mismatch";
      return response;
    }

    const size_t shellCount = shellDirs.size();
    const int zonalCount = lmax / 2 + 1;

    std::vector<glm::dvec3> unitShellDirs;
    unitShellDirs.reserve(shellCount);
    for (const glm::dvec3& direction : shellDirs)
    {
      unitShellDirs.push_back(glm::normalize(direction));
    }

    Eigen::MatrixXd normalMatrix = Eigen::MatrixXd::Zero(zonalCount, zonalCount);
    Eigen::VectorXd rhs = Eigen::VectorXd::Zero(zonalCount);
    Eigen::VectorXd row(zonalCount);
    double basis[RealSH::kSHMaxCoefficients];
    std::unordered_set<long long> polarAngleBins;

    for (size_t v = 0; v < axes.size(); ++v)
    {
      const double* voxelSignals = &signals[v * shellCount];
      const bool signalsFinite = std::all_of(voxelSignals, voxelSignals + shellCount,
                                             [](double value) { return std::isfinite(value); });
      if (!(glm::length(axes[v]) >= 1e-12) || !signalsFinite)
      {
        ++response.failedVoxelCount;
        continue;
      }

      const Rotation3 rotation = RotationTakingAxisToZ(axes[v]);
      for (size_t i = 0; i < shellCount; ++i)
      {
        const glm::dvec3 g = glm::normalize(Rotate(rotation, unitShellDirs[i]));
        RealSH::EvaluateBasis(g, lmax, basis);
        for (int j = 0; j < zonalCount; ++j)
        {
          row[j] = basis[RealSH::Index(2 * j, 0)];
        }
        normalMatrix.noalias() += row * row.transpose();
        rhs += row * voxelSignals[i];
        polarAngleBins.insert(static_cast<long long>(std::floor(std::abs(g.z) / kPolarAngleBinWidth)));
      }
      ++response.voxelCount;
    }

    response.rowCount = response.voxelCount * shellCount;
    response.distinctPolarAngles = polarAngleBins.size();

    if (response.voxelCount == 0)
    {
      response.error = "no response voxel has a finite signal and a valid axis";
      return response;
    }
    // The zonal basis is a polynomial in cos^2(theta), so zonalCount distinct |cos theta| values
    // are needed for the fit to be identifiable.
    if (response.distinctPolarAngles < static_cast<size_t>(zonalCount))
    {
      response.error = "rotated directions cover only " + std::to_string(response.distinctPolarAngles) +
                       " distinct polar angles; " + std::to_string(zonalCount) + " are needed for an lmax-" +
                       std::to_string(lmax) + " response";
      return response;
    }

    normalMatrix += kResponseFitRidge * Eigen::MatrixXd::Identity(zonalCount, zonalCount);
    const Eigen::LLT<Eigen::MatrixXd> llt(normalMatrix);
    if (llt.info() != Eigen::Success)
    {
      response.error = "joint zonal normal matrix is not positive definite";
      return response;
    }
    const Eigen::VectorXd zonal = llt.solve(rhs);
    if (!zonal.allFinite())
    {
      response.error = "joint zonal fit produced non-finite coefficients";
      return response;
    }

    response.lmax = lmax;
    response.zonal.resize(static_cast<size_t>(zonalCount));
    response.rotational.resize(static_cast<size_t>(zonalCount));
    for (int j = 0; j < zonalCount; ++j)
    {
      response.zonal[static_cast<size_t>(j)] = zonal[j];
      response.rotational[static_cast<size_t>(j)] = zonal[j] / ZonalDelta(2 * j);
    }
    return response;
  }

  struct CsdSolver::Impl
  {
    int lmax = 0;
    int coefficientCount = 0;
    size_t shellCount = 0;
    size_t hrCount = 0;
    double tau = 0.0;
    int maxIterations = 0;
    double lambdaEff = 0.0;
    double lambdaEff2 = 0.0;

    Eigen::MatrixXd A;                  // nShell x n, Q * diag(r)
    Eigen::MatrixXd AtA;                // n x n, with relative ridge
    Eigen::MatrixXd initialPseudoInverse; // n x nShell, zero beyond the lmax-4 rows
    Eigen::MatrixXd P;                  // nHR x n
  };

  struct CsdSolver::Workspace::Impl
  {
    Eigen::VectorXd b;
    Eigen::VectorXd Atb;
    Eigen::VectorXd f;
    Eigen::VectorXd fNew;
    Eigen::VectorXd u;
    Eigen::MatrixXd M;
    Eigen::MatrixXd Pneg;
    Eigen::LLT<Eigen::MatrixXd> llt;
    std::vector<int> neg;
    std::vector<int> prevNeg;

    Impl(int coefficientCount, size_t shellCount, size_t hrCount)
      : b(static_cast<Eigen::Index>(shellCount)),
        Atb(coefficientCount),
        f(coefficientCount),
        fNew(coefficientCount),
        u(static_cast<Eigen::Index>(hrCount)),
        M(coefficientCount, coefficientCount),
        Pneg(static_cast<Eigen::Index>(hrCount), coefficientCount),
        llt(coefficientCount)
    {
      neg.reserve(hrCount);
      prevNeg.reserve(hrCount);
    }
  };

  CsdSolver::Workspace::Workspace(const CsdSolver& solver)
    : impl(std::make_unique<Impl>(solver.impl->coefficientCount, solver.impl->shellCount, solver.impl->hrCount))
  {
  }

  CsdSolver::Workspace::~Workspace() = default;
  CsdSolver::Workspace::Workspace(Workspace&&) noexcept = default;
  CsdSolver::Workspace& CsdSolver::Workspace::operator=(Workspace&&) noexcept = default;

  CsdSolver::CsdSolver(const std::vector<glm::dvec3>& shellDirs,
                       const std::vector<double>& rotational,
                       const Settings& settings)
  {
    const int lmax = settings.lmax;
    if (!IsSupportedCsdOrder(lmax))
    {
      throw std::runtime_error("fODF CSD: unsupported lmax " + std::to_string(lmax) + ".");
    }
    if (rotational.size() < static_cast<size_t>(lmax / 2 + 1))
    {
      const int responseLmax = rotational.empty() ? -2 : 2 * (static_cast<int>(rotational.size()) - 1);
      throw std::runtime_error("fODF CSD: response function lmax " + std::to_string(responseLmax) +
                               " is below the CSD lmax " + std::to_string(lmax) + ".");
    }

    const size_t shellCount = shellDirs.size();
    const int initialLmax = std::min(kInitialLmax, lmax);
    const int initialCount = RealSH::CoefficientCount(initialLmax);
    if (shellCount < static_cast<size_t>(initialCount))
    {
      throw std::runtime_error("fODF CSD: shell has " + std::to_string(shellCount) + " directions; the lmax-" +
                               std::to_string(initialLmax) + " initial estimate needs at least " +
                               std::to_string(initialCount) + ".");
    }

    auto solverImpl = std::make_unique<Impl>();
    const int n = RealSH::CoefficientCount(lmax);
    const Eigen::Index rows = static_cast<Eigen::Index>(shellCount);
    double basis[RealSH::kSHMaxCoefficients];

    Eigen::MatrixXd Q(rows, n);
    for (size_t i = 0; i < shellCount; ++i)
    {
      RealSH::EvaluateBasis(glm::normalize(shellDirs[i]), lmax, basis);
      for (int k = 0; k < n; ++k)
      {
        Q(static_cast<Eigen::Index>(i), k) = basis[k];
      }
    }

    Eigen::VectorXd rCol(n);
    for (int l = 0; l <= lmax; l += 2)
    {
      for (int m = -l; m <= l; ++m)
      {
        rCol[RealSH::Index(l, m)] = rotational[static_cast<size_t>(l / 2)];
      }
    }

    Eigen::MatrixXd& A = solverImpl->A;
    A.resize(rows, n);
    for (int k = 0; k < n; ++k)
    {
      A.col(k) = Q.col(k) * rCol[k];
    }

    // When super-resolved AtA is rank-deficient by construction; the relative ridge keeps LLT from
    // failing while the negativity rows carry the missing information.
    solverImpl->AtA = A.transpose() * A;
    const double ridge = kCsdRelativeRidge * solverImpl->AtA.trace() / static_cast<double>(n);
    solverImpl->AtA.diagonal().array() += ridge;

    const Eigen::MatrixXd A0 = A.leftCols(initialCount);
    Eigen::MatrixXd N0 = A0.transpose() * A0;
    const double initialRidge = kCsdRelativeRidge * N0.trace() / static_cast<double>(initialCount);
    N0.diagonal().array() += initialRidge;
    const Eigen::LLT<Eigen::MatrixXd> initialLlt(N0);
    if (initialLlt.info() != Eigen::Success)
    {
      throw std::runtime_error("fODF CSD: failed to build the initial lmax-" + std::to_string(initialLmax) +
                               " pseudo-inverse (normal matrix not positive definite).");
    }
    solverImpl->initialPseudoInverse = Eigen::MatrixXd::Zero(n, rows);
    solverImpl->initialPseudoInverse.topRows(initialCount) = initialLlt.solve(A0.transpose());

    const std::vector<glm::dvec3> hr = BuildHrHemisphere();
    const Eigen::Index hrRows = static_cast<Eigen::Index>(hr.size());
    solverImpl->P.resize(hrRows, n);
    for (size_t m = 0; m < hr.size(); ++m)
    {
      RealSH::EvaluateBasis(hr[m], lmax, basis);
      for (int k = 0; k < n; ++k)
      {
        solverImpl->P(static_cast<Eigen::Index>(m), k) = basis[k];
      }
    }

    // Normalised lambda: per-row weight ratio nShell/nHR, scaled by the mean row norms so signal
    // units and r_l are absorbed through A.
    double meanRowA = 0.0;
    for (Eigen::Index i = 0; i < rows; ++i)
    {
      meanRowA += A.row(i).norm();
    }
    meanRowA /= static_cast<double>(shellCount);
    double meanRowP = 0.0;
    for (Eigen::Index m = 0; m < hrRows; ++m)
    {
      meanRowP += solverImpl->P.row(m).norm();
    }
    meanRowP /= static_cast<double>(hr.size());

    solverImpl->lambdaEff = settings.lambda * std::sqrt(static_cast<double>(shellCount) / static_cast<double>(hr.size())) *
                            meanRowA / meanRowP;
    solverImpl->lambdaEff2 = solverImpl->lambdaEff * solverImpl->lambdaEff;
    solverImpl->tau = settings.tau;
    solverImpl->maxIterations = settings.maxIterations;
    solverImpl->lmax = lmax;
    solverImpl->coefficientCount = n;
    solverImpl->shellCount = shellCount;
    solverImpl->hrCount = hr.size();

    impl = std::move(solverImpl);
  }

  CsdSolver::~CsdSolver() = default;

  int CsdSolver::GetLmax() const
  {
    return impl->lmax;
  }

  int CsdSolver::GetCoefficientCount() const
  {
    return impl->coefficientCount;
  }

  size_t CsdSolver::GetShellCount() const
  {
    return impl->shellCount;
  }

  size_t CsdSolver::GetHrCount() const
  {
    return impl->hrCount;
  }

  double CsdSolver::GetEffectiveLambda() const
  {
    return impl->lambdaEff;
  }

  bool CsdSolver::IsSuperResolved() const
  {
    return static_cast<size_t>(impl->coefficientCount) > impl->shellCount;
  }

  CsdSolver::VoxelResult CsdSolver::SolveVoxel(const double* signal, Workspace& workspace, double* coefficientsOut) const
  {
    const Impl& s = *impl;
    Workspace::Impl& ws = *workspace.impl;
    const Eigen::Index hrCount = static_cast<Eigen::Index>(s.hrCount);
    const size_t n = static_cast<size_t>(s.coefficientCount);

    ws.b = Eigen::Map<const Eigen::VectorXd>(signal, static_cast<Eigen::Index>(s.shellCount));
    ws.Atb.noalias() = s.A.transpose() * ws.b;
    ws.f.noalias() = s.initialPseudoInverse * ws.b;
    ws.u.noalias() = s.P * ws.f;
    const double threshold = s.tau * ws.u.mean();

    bool havePrevious = false;
    int solves = 0;
    VoxelStatus status = VoxelStatus::NotConverged;
    for (int iteration = 0; iteration < s.maxIterations; ++iteration)
    {
      if (iteration > 0)
      {
        ws.u.noalias() = s.P * ws.f;
      }

      ws.neg.clear();
      for (Eigen::Index m = 0; m < hrCount; ++m)
      {
        if (ws.u[m] < threshold)
        {
          ws.neg.push_back(static_cast<int>(m));
        }
      }

      if (havePrevious && ws.neg == ws.prevNeg)
      {
        status = VoxelStatus::Converged;
        break;
      }
      if (s.shellCount + ws.neg.size() < n)
      {
        status = VoxelStatus::Underdetermined;
        break;
      }

      const Eigen::Index k = static_cast<Eigen::Index>(ws.neg.size());
      for (Eigen::Index j = 0; j < k; ++j)
      {
        ws.Pneg.row(j) = s.P.row(ws.neg[static_cast<size_t>(j)]);
      }
      ws.M = s.AtA;
      if (k > 0)
      {
        ws.M.noalias() += s.lambdaEff2 * (ws.Pneg.topRows(k).transpose() * ws.Pneg.topRows(k));
      }

      ws.llt.compute(ws.M);
      if (ws.llt.info() != Eigen::Success)
      {
        status = VoxelStatus::SolveFailed;
        break;
      }
      ws.fNew = ws.llt.solve(ws.Atb);
      if (!ws.fNew.allFinite())
      {
        status = VoxelStatus::SolveFailed;
        break;
      }

      ws.f = ws.fNew;
      std::swap(ws.prevNeg, ws.neg);
      havePrevious = true;
      solves = iteration + 1;
    }

    for (size_t k = 0; k < n; ++k)
    {
      coefficientsOut[k] = ws.f[static_cast<Eigen::Index>(k)];
    }
    return VoxelResult{status, solves};
  }

  std::vector<glm::dvec3> FindPeaks(const double* coefficients, int lmax,
                                    const std::vector<glm::dvec3>& sampleDirs,
                                    size_t maxPeaks, double neighbourAngleDeg, double separationDeg)
  {
    const int coefficientCount = RealSH::CoefficientCount(lmax);
    if (coefficientCount == 0 || sampleDirs.empty() || maxPeaks == 0)
    {
      return {};
    }

    const size_t sampleCount = sampleDirs.size();
    std::vector<glm::dvec3> unitDirs(sampleCount);
    std::vector<double> amplitudes(sampleCount, 0.0);
    double basis[RealSH::kSHMaxCoefficients];
    for (size_t i = 0; i < sampleCount; ++i)
    {
      unitDirs[i] = glm::normalize(sampleDirs[i]);
      RealSH::EvaluateBasis(unitDirs[i], lmax, basis);
      double amplitude = 0.0;
      for (int k = 0; k < coefficientCount; ++k)
      {
        amplitude += coefficients[k] * basis[k];
      }
      amplitudes[i] = amplitude;
    }

    // Brute-force local maxima over an angular neighbourhood; |d_i . d_j| keeps it antipodally symmetric.
    const double neighbourCos = std::cos(glm::radians(neighbourAngleDeg));
    std::vector<size_t> candidates;
    for (size_t i = 0; i < sampleCount; ++i)
    {
      if (!(amplitudes[i] > 0.0))
      {
        continue;
      }
      bool isMaximum = true;
      for (size_t j = 0; j < sampleCount && isMaximum; ++j)
      {
        if (j != i && std::abs(glm::dot(unitDirs[i], unitDirs[j])) >= neighbourCos && amplitudes[i] < amplitudes[j])
        {
          isMaximum = false;
        }
      }
      if (isMaximum)
      {
        candidates.push_back(i);
      }
    }

    std::sort(candidates.begin(), candidates.end(),
              [&amplitudes](size_t lhs, size_t rhs)
              {
                if (amplitudes[lhs] != amplitudes[rhs])
                {
                  return amplitudes[lhs] > amplitudes[rhs];
                }
                return lhs < rhs;
              });

    const double separationCos = std::cos(glm::radians(separationDeg));
    std::vector<glm::dvec3> peaks;
    for (const size_t candidate : candidates)
    {
      const glm::dvec3& d = unitDirs[candidate];
      const bool separated = std::all_of(peaks.begin(), peaks.end(),
                                         [&](const glm::dvec3& p) { return std::abs(glm::dot(d, p)) < separationCos; });
      if (!separated)
      {
        continue;
      }
      peaks.push_back(d);
      if (peaks.size() >= maxPeaks)
      {
        break;
      }
    }
    return peaks;
  }

  std::string FormatScientific(double value)
  {
    std::ostringstream stream;
    stream.precision(3);
    stream << std::scientific << value;
    return stream.str();
  }

  std::string FormatFixed(double value, int decimals)
  {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(decimals) << value;
    return stream.str();
  }
}

namespace
{
  constexpr double kSelfTestLambdaParallel = 1.7e-3;      // mm^2/s
  constexpr double kSelfTestLambdaPerpendicular = 0.3e-3; // mm^2/s
  constexpr double kSelfTestTau = 0.1;
  constexpr double kSelfTestLambda = 1.0;
  constexpr int kSelfTestMaxIterations = 50;
  constexpr double kSelfTestNeighbourAngleDeg = 6.0;
  constexpr double kSelfTestSeparationDeg = 15.0;

  // Noise-free single-fibre signal exp(-b g^T D g), D = R diag(par, perp, perp) R^T with R e1 = fibre.
  double SingleFibreSignal(const glm::dvec3& g, const glm::dvec3& fibre, double bValue)
  {
    const double cosine = glm::dot(g, fibre);
    return std::exp(-bValue * (kSelfTestLambdaPerpendicular +
                               (kSelfTestLambdaParallel - kSelfTestLambdaPerpendicular) * cosine * cosine));
  }

  struct CrossingCaseOutcome
  {
    bool evaluated = false;   // false = skipped
    bool passed = false;
    bool solved = false;      // a solve ran, so iterations are meaningful
    int iterations = 0;
    std::string text;         // "max error X deg" | "skipped" | "failed (<reason>)"
  };

  double AngleBetweenAxesDeg(const glm::dvec3& lhs, const glm::dvec3& rhs)
  {
    return glm::degrees(std::acos(std::min(1.0, std::abs(glm::dot(lhs, rhs)))));
  }

  CrossingCaseOutcome RunCrossingCase(const FodfCsdMath::CsdSolver& solver,
                                      const std::vector<glm::dvec3>& dirs,
                                      const std::vector<glm::dvec3>& peakDirs,
                                      const glm::dvec3& fibre1,
                                      const glm::dvec3& fibre2,
                                      double bValue,
                                      double toleranceDeg)
  {
    CrossingCaseOutcome outcome;
    outcome.evaluated = true;

    std::vector<double> signal(dirs.size());
    for (size_t i = 0; i < dirs.size(); ++i)
    {
      signal[i] = 0.5 * SingleFibreSignal(dirs[i], fibre1, bValue) + 0.5 * SingleFibreSignal(dirs[i], fibre2, bValue);
    }

    FodfCsdMath::CsdSolver::Workspace workspace(solver);
    double coefficients[RealSH::kSHMaxCoefficients] = {};
    const FodfCsdMath::CsdSolver::VoxelResult result = solver.SolveVoxel(signal.data(), workspace, coefficients);
    outcome.solved = true;
    outcome.iterations = result.iterations;

    const std::vector<glm::dvec3> peaks = FodfCsdMath::FindPeaks(coefficients, solver.GetLmax(), peakDirs, 2,
                                                                 kSelfTestNeighbourAngleDeg, kSelfTestSeparationDeg);
    if (result.status != FodfCsdMath::CsdSolver::VoxelStatus::Converged)
    {
      outcome.text = "failed (not converged)";
      return outcome;
    }
    if (peaks.size() != 2)
    {
      outcome.text = "failed (" + std::to_string(peaks.size()) + " peak(s) found)";
      return outcome;
    }

    const glm::dvec3 e1 = glm::normalize(fibre1);
    const glm::dvec3 e2 = glm::normalize(fibre2);
    const double maxError = std::min(
        std::max(AngleBetweenAxesDeg(peaks[0], e1), AngleBetweenAxesDeg(peaks[1], e2)),
        std::max(AngleBetweenAxesDeg(peaks[0], e2), AngleBetweenAxesDeg(peaks[1], e1)));
    const std::string errorText = "max error " + FodfCsdMath::FormatFixed(maxError, 1) + " deg";
    outcome.passed = maxError <= toleranceDeg;
    outcome.text = outcome.passed ? errorText : "failed (" + errorText + ")";
    return outcome;
  }

  CrossingCaseOutcome FailedCase(const std::string& reason)
  {
    CrossingCaseOutcome outcome;
    outcome.evaluated = true;
    outcome.text = "failed (" + reason + ")";
    return outcome;
  }

  CrossingCaseOutcome SkippedCase()
  {
    CrossingCaseOutcome outcome;
    outcome.text = "skipped";
    return outcome;
  }

  std::string IterationText(const CrossingCaseOutcome& outcome)
  {
    return outcome.solved ? std::to_string(outcome.iterations) : std::string("-");
  }
}

/**
 * @brief Synthetic CSD check on a gradient scheme: response from synthetic single fibres through the
 *        joint fit, then deconvolution of noise-free 90 and 60 degree crossings and peak recovery.
 *
 * @param shellDirs gradient directions of the shell CSD will use
 * @param bValue shell b-value (s/mm^2)
 * @param lmax CSD lmax
 * @return true when every evaluated crossing case recovered both fibres within tolerance
 */
bool SelfTestCsd(const std::vector<glm::vec3>& shellDirs, float bValue, int lmax)
{
  if (lmax < 4)
  {
    EngineLog::Message("CSD self-test skipped: lmax " + std::to_string(lmax) + " cannot resolve crossing fibres",
                       "FodfCsd");
    return true;
  }

  constexpr double kTolerance90Deg = 10.0;
  constexpr double kTolerance60Deg = 15.0;
  const bool evaluate60 = lmax == 8;
  const double b = static_cast<double>(bValue);

  CrossingCaseOutcome case90;
  CrossingCaseOutcome case60;
  try
  {
    const std::vector<glm::dvec3> dirs = FodfCsdMath::NormalizedDirections(shellDirs);

    const std::vector<glm::dvec3> responseAxes = {
      glm::dvec3(0.0, 0.0, 1.0),
      glm::dvec3(1.0, 0.0, 0.0),
      glm::dvec3(0.0, 1.0, 0.0),
      -glm::normalize(glm::dvec3(1.0, 1.0, 1.0)),
      glm::normalize(glm::dvec3(1.0, -2.0, 0.5))};
    std::vector<double> responseSignals;
    responseSignals.reserve(responseAxes.size() * dirs.size());
    for (const glm::dvec3& axis : responseAxes)
    {
      const glm::dvec3 fibre = glm::normalize(axis);
      for (const glm::dvec3& g : dirs)
      {
        responseSignals.push_back(SingleFibreSignal(g, fibre, b));
      }
    }

    std::string setupFailure;
    const FodfCsdMath::AxialResponse response = FodfCsdMath::FitAxialResponse(dirs, responseSignals, responseAxes, lmax);
    if (!response.IsValid())
    {
      setupFailure = "response fit failed: " + response.error;
    }
    else if (!(response.rotational[0] > 0.0))
    {
      setupFailure = "response r_0 <= 0";
    }
    else if (response.rotational.size() > 1 && response.rotational[1] >= 0.0)
    {
      setupFailure = "response r_2 >= 0";
    }

    std::unique_ptr<FodfCsdMath::CsdSolver> solver;
    if (setupFailure.empty())
    {
      try
      {
        solver = std::make_unique<FodfCsdMath::CsdSolver>(
            dirs, response.rotational,
            FodfCsdMath::CsdSolver::Settings{lmax, kSelfTestTau, kSelfTestLambda, kSelfTestMaxIterations});
      }
      catch (const std::exception& exception)
      {
        setupFailure = std::string("solver construction: ") + exception.what();
      }
    }

    if (!setupFailure.empty())
    {
      case90 = FailedCase(setupFailure);
      case60 = evaluate60 ? FailedCase(setupFailure) : SkippedCase();
    }
    else
    {
      const std::vector<glm::dvec3> peakDirs =
          FodfCsdMath::NormalizedDirections(IcosphereGeometry::GenerateDirections(FodfCsdMath::kPeakIcosphereLevel));

      try
      {
        case90 = RunCrossingCase(*solver, dirs, peakDirs, glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(0.0, 1.0, 0.0),
                                 b, kTolerance90Deg);
      }
      catch (const std::exception& exception)
      {
        case90 = FailedCase(exception.what());
      }

      if (!evaluate60)
      {
        case60 = SkippedCase();
      }
      else
      {
        try
        {
          case60 = RunCrossingCase(*solver, dirs, peakDirs, glm::dvec3(1.0, 0.0, 0.0),
                                   glm::dvec3(0.5, 0.8660254037844386, 0.0), b, kTolerance60Deg);
        }
        catch (const std::exception& exception)
        {
          case60 = FailedCase(exception.what());
        }
      }
    }
  }
  catch (const std::exception& exception)
  {
    case90 = FailedCase(exception.what());
    case60 = evaluate60 ? FailedCase(exception.what()) : SkippedCase();
  }

  const bool passed = case90.passed && (case60.passed || !case60.evaluated);
  EngineLog::Message(std::string("CSD self-test ") + (passed ? "passed" : "FAILED") + ": 90deg crossing " + case90.text +
                     ", 60deg crossing " + case60.text + " (lmax " + std::to_string(lmax) + ", b=" +
                     std::to_string(std::lround(b)) + ", " + std::to_string(shellDirs.size()) + " dirs, " +
                     IterationText(case90) + " / " + IterationText(case60) + " iterations)",
                     "FodfCsd");
  return passed;
}
