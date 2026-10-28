#include "scene/spherical_harmonics/RealSH.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include "scene/geometry/IcosphereGeometry.h"
#include "foundation/log/EngineLog.h"

namespace
{
  constexpr double kGradientEpsilon = 1e-4;

  std::string FormatScientific(double value)
  {
    std::ostringstream stream;
    stream.precision(3);
    stream << std::scientific << value;
    return stream.str();
  }

  bool IsSupportedOrder(int lmax)
  {
    return lmax >= 0 && lmax <= RealSH::kSHMaxOrder && lmax % 2 == 0;
  }

  double Factorial(int n)
  {
    double result = 1.0;
    for (int i = 2; i <= n; ++i)
    {
      result *= static_cast<double>(i);
    }
    return result;
  }

  // Associated Legendre function P_l^m(x) for 0 <= m <= l, including the Condon-Shortley phase,
  // by the standard upward recurrence in l.
  double AssociatedLegendre(int l, int m, double x)
  {
    double pmm = 1.0;
    if (m > 0)
    {
      const double sinTheta = std::sqrt(std::max(0.0, (1.0 - x) * (1.0 + x)));
      double oddFactor = 1.0;
      for (int i = 1; i <= m; ++i)
      {
        pmm *= -oddFactor * sinTheta;
        oddFactor += 2.0;
      }
    }
    if (l == m)
    {
      return pmm;
    }

    double pmmPlus1 = x * (2.0 * m + 1.0) * pmm;
    for (int ll = m + 2; ll <= l; ++ll)
    {
      const double pll = ((2.0 * ll - 1.0) * x * pmmPlus1 - (ll + m - 1.0) * pmm) / (ll - m);
      pmm = pmmPlus1;
      pmmPlus1 = pll;
    }
    return pmmPlus1;
  }

  // Orthonormalisation factor of the complex harmonic Y_l^m.
  double Normalisation(int l, int m)
  {
    return std::sqrt((2.0 * l + 1.0) / (4.0 * glm::pi<double>()) * Factorial(l - m) / Factorial(l + m));
  }

  // Any unit vector orthogonal to `normal`, chosen from the least-aligned axis so it never degenerates.
  glm::dvec3 AnyTangent(const glm::dvec3& normal)
  {
    const glm::dvec3 axis = std::abs(normal.x) < 0.9 ? glm::dvec3(1.0, 0.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
    return glm::normalize(glm::cross(normal, axis));
  }

  // Lower-triangular Cholesky factor of the symmetric n x n row-major matrix, in place (the upper
  // triangle is left untouched). Returns false when the matrix is not positive definite.
  bool CholeskyDecompose(std::vector<double>& matrix, int n)
  {
    for (int j = 0; j < n; ++j)
    {
      double diagonal = matrix[j * n + j];
      for (int k = 0; k < j; ++k)
      {
        diagonal -= matrix[j * n + k] * matrix[j * n + k];
      }
      if (!(diagonal > 0.0))
      {
        return false;
      }
      const double lJJ = std::sqrt(diagonal);
      matrix[j * n + j] = lJJ;

      for (int i = j + 1; i < n; ++i)
      {
        double value = matrix[i * n + j];
        for (int k = 0; k < j; ++k)
        {
          value -= matrix[i * n + k] * matrix[j * n + k];
        }
        matrix[i * n + j] = value / lJJ;
      }
    }
    return true;
  }

  // Solves L L^T x = rhs in place, given the factor from CholeskyDecompose.
  void CholeskySolve(const std::vector<double>& factor, int n, std::vector<double>& rhs)
  {
    for (int i = 0; i < n; ++i)
    {
      double value = rhs[i];
      for (int k = 0; k < i; ++k)
      {
        value -= factor[i * n + k] * rhs[k];
      }
      rhs[i] = value / factor[i * n + i];
    }
    for (int i = n - 1; i >= 0; --i)
    {
      double value = rhs[i];
      for (int k = i + 1; k < n; ++k)
      {
        value -= factor[k * n + i] * rhs[k];
      }
      rhs[i] = value / factor[i * n + i];
    }
  }
}

namespace RealSH
{
  int CoefficientCount(int lmax)
  {
    return IsSupportedOrder(lmax) ? (lmax + 1) * (lmax + 2) / 2 : 0;
  }

  void EvaluateBasis(const glm::dvec3& unitDir, int lmax, double* out)
  {
    if (!IsSupportedOrder(lmax))
    {
      return;
    }

    const double cosTheta = std::clamp(unitDir.z, -1.0, 1.0);
    // atan2(0, 0) is 0 at the poles, where every m != 0 term vanishes anyway.
    const double phi = std::atan2(unitDir.y, unitDir.x);
    const double sqrt2 = std::sqrt(2.0);

    for (int l = 0; l <= lmax; l += 2)
    {
      out[Index(l, 0)] = Normalisation(l, 0) * AssociatedLegendre(l, 0, cosTheta);
      for (int m = 1; m <= l; ++m)
      {
        const double radial = sqrt2 * Normalisation(l, m) * AssociatedLegendre(l, m, cosTheta);
        out[Index(l, m)] = radial * std::cos(m * phi);
        out[Index(l, -m)] = radial * std::sin(m * phi);
      }
    }
  }

  std::vector<float> BuildBasisTable(const std::vector<glm::vec3>& unitDirs)
  {
    std::vector<float> table;
    table.reserve(unitDirs.size() * kSHMaxCoefficients * 4);

    double value[kSHMaxCoefficients];
    double plus1[kSHMaxCoefficients];
    double minus1[kSHMaxCoefficients];
    double plus2[kSHMaxCoefficients];
    double minus2[kSHMaxCoefficients];

    for (const glm::vec3& direction : unitDirs)
    {
      const glm::dvec3 normal = glm::normalize(glm::dvec3(direction));
      const glm::dvec3 tangent1 = AnyTangent(normal);
      const glm::dvec3 tangent2 = glm::cross(normal, tangent1);

      EvaluateBasis(normal, kSHMaxOrder, value);
      EvaluateBasis(glm::normalize(normal + kGradientEpsilon * tangent1), kSHMaxOrder, plus1);
      EvaluateBasis(glm::normalize(normal - kGradientEpsilon * tangent1), kSHMaxOrder, minus1);
      EvaluateBasis(glm::normalize(normal + kGradientEpsilon * tangent2), kSHMaxOrder, plus2);
      EvaluateBasis(glm::normalize(normal - kGradientEpsilon * tangent2), kSHMaxOrder, minus2);

      for (int k = 0; k < kSHMaxCoefficients; ++k)
      {
        const double derivative1 = (plus1[k] - minus1[k]) / (2.0 * kGradientEpsilon);
        const double derivative2 = (plus2[k] - minus2[k]) / (2.0 * kGradientEpsilon);
        const glm::dvec3 gradient = tangent1 * derivative1 + tangent2 * derivative2;

        table.push_back(static_cast<float>(value[k]));
        table.push_back(static_cast<float>(gradient.x));
        table.push_back(static_cast<float>(gradient.y));
        table.push_back(static_cast<float>(gradient.z));
      }
    }
    return table;
  }

  std::vector<double> BuildFitMatrix(const std::vector<glm::vec3>& unitDirs, int lmax, double lambda)
  {
    const int coefficientCount = CoefficientCount(lmax);
    if (coefficientCount == 0 || unitDirs.empty())
    {
      EngineLog::Error("SH fit matrix needs an even lmax in [0, 8] and at least one direction, got lmax " +
                       std::to_string(lmax) + " and " + std::to_string(unitDirs.size()) + " directions", "RealSH");
      return {};
    }

    const size_t directionCount = unitDirs.size();
    const size_t k = static_cast<size_t>(coefficientCount);

    // B: directionCount x coefficientCount, row-major.
    std::vector<double> basis(directionCount * k);
    for (size_t i = 0; i < directionCount; ++i)
    {
      EvaluateBasis(glm::normalize(glm::dvec3(unitDirs[i])), lmax, &basis[i * k]);
    }

    // Normal matrix B^T B + lambda I.
    std::vector<double> normalMatrix(k * k, 0.0);
    for (size_t row = 0; row < k; ++row)
    {
      for (size_t column = 0; column <= row; ++column)
      {
        double sum = 0.0;
        for (size_t i = 0; i < directionCount; ++i)
        {
          sum += basis[i * k + row] * basis[i * k + column];
        }
        normalMatrix[row * k + column] = sum;
        normalMatrix[column * k + row] = sum;
      }
      normalMatrix[row * k + row] += lambda;
    }

    if (!CholeskyDecompose(normalMatrix, coefficientCount))
    {
      EngineLog::Error("SH fit matrix: normal matrix is not positive definite (lambda " +
                       std::to_string(lambda) + ")", "RealSH");
      return {};
    }

    // Column i of B+ solves (B^T B + lambda I) x = B^T e_i = row i of B.
    std::vector<double> fitMatrix(k * directionCount);
    std::vector<double> column(k);
    for (size_t i = 0; i < directionCount; ++i)
    {
      std::copy(basis.begin() + i * k, basis.begin() + (i + 1) * k, column.begin());
      CholeskySolve(normalMatrix, coefficientCount, column);
      for (size_t row = 0; row < k; ++row)
      {
        fitMatrix[row * directionCount + i] = column[row];
      }
    }
    return fitMatrix;
  }

  bool SelfTestRealSH()
  {
    // Equal-weight (4*pi/N) quadrature over icosphere directions isn't exact, so the Gram matrix
    // deviates from identity by a few percent at lmax 8 even for a correct basis (~4e-2 measured).
    constexpr double kOrthonormalityTolerance = 0.1;
    constexpr double kRoundTripTolerance = 1e-4;

    const std::vector<glm::vec3> quadratureDirs = IcosphereGeometry::GenerateDirections(4);
    const double weight = 4.0 * glm::pi<double>() / static_cast<double>(quadratureDirs.size());

    std::vector<double> gram(kSHMaxCoefficients * kSHMaxCoefficients, 0.0);
    double values[kSHMaxCoefficients];
    for (const glm::vec3& direction : quadratureDirs)
    {
      EvaluateBasis(glm::normalize(glm::dvec3(direction)), kSHMaxOrder, values);
      for (int i = 0; i < kSHMaxCoefficients; ++i)
      {
        for (int j = 0; j < kSHMaxCoefficients; ++j)
        {
          gram[i * kSHMaxCoefficients + j] += weight * values[i] * values[j];
        }
      }
    }

    double orthonormalityError = 0.0;
    for (int i = 0; i < kSHMaxCoefficients; ++i)
    {
      for (int j = 0; j < kSHMaxCoefficients; ++j)
      {
        const double expected = i == j ? 1.0 : 0.0;
        orthonormalityError = std::max(orthonormalityError, std::abs(gram[i * kSHMaxCoefficients + j] - expected));
      }
    }

    const std::vector<glm::vec3> fitDirs = IcosphereGeometry::GenerateDirections(3);
    const std::vector<double> fitMatrix = BuildFitMatrix(fitDirs, kSHMaxOrder, 1e-6);

    double knownCoefficients[kSHMaxCoefficients];
    for (int k = 0; k < kSHMaxCoefficients; ++k)
    {
      knownCoefficients[k] = std::sin(1.7 * k + 0.3) / (1.0 + 0.1 * k);
    }

    std::vector<double> samples(fitDirs.size(), 0.0);
    for (size_t i = 0; i < fitDirs.size(); ++i)
    {
      EvaluateBasis(glm::normalize(glm::dvec3(fitDirs[i])), kSHMaxOrder, values);
      for (int k = 0; k < kSHMaxCoefficients; ++k)
      {
        samples[i] += knownCoefficients[k] * values[k];
      }
    }

    double roundTripError = fitMatrix.empty() ? 1.0 : 0.0;
    for (int k = 0; k < kSHMaxCoefficients && !fitMatrix.empty(); ++k)
    {
      double fitted = 0.0;
      for (size_t i = 0; i < fitDirs.size(); ++i)
      {
        fitted += fitMatrix[k * fitDirs.size() + i] * samples[i];
      }
      roundTripError = std::max(roundTripError, std::abs(fitted - knownCoefficients[k]));
    }

    const bool passed = orthonormalityError <= kOrthonormalityTolerance && roundTripError <= kRoundTripTolerance;
    EngineLog::Message("SH self-test " + std::string(passed ? "passed" : "FAILED") +
                       ": max |<Y_i,Y_j> - delta_ij| = " + FormatScientific(orthonormalityError) +
                       " (" + std::to_string(quadratureDirs.size()) + " dirs), max fit round-trip error = " +
                       FormatScientific(roundTripError) + " (" + std::to_string(fitDirs.size()) + " dirs)", "RealSH");
    return passed;
  }
}
