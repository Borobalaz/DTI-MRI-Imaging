#pragma once

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

// Real, even-order, orthonormal spherical-harmonic basis in the MRtrix3 (non-legacy) convention,
// shared by the engine (glyph basis table) and core (ODF -> SH fit). Coefficient k of order l and
// phase m (-l <= m <= l, even l only) sits at Index(l, m) = l(l+1)/2 + m:
//   m < 0 : sqrt(2) * Im(Y_l^|m|)
//   m = 0 : Y_l^0
//   m > 0 : sqrt(2) * Re(Y_l^m)
// where Y_l^m is the complex orthonormal harmonic with polar angle measured from +z.
//
// TODO(plan item 10): the associated Legendre functions below include the Condon-Shortley phase
// (-1)^m. That must be checked against the MRtrix3 docs before any SH file loader is added; until
// then it doesn't matter, because fitting and rendering share this one implementation.
namespace RealSH
{
  constexpr int kSHMaxOrder = 8;
  constexpr int kSHMaxCoefficients = 45; // CoefficientCount(kSHMaxOrder)

  // Glyph basis-table layout: Y_k packed 4 per RGBA texel, zero-padded from 45 to 48 values.
  constexpr int kBasisTexelsPerDirection = 12;
  constexpr int kBasisFloatsPerDirection = 4 * kBasisTexelsPerDirection; // 48

  // Number of coefficients for an even lmax in [0, kSHMaxOrder]; 0 for any other lmax.
  int CoefficientCount(int lmax);

  // Flat coefficient index of (l, m); l must be even and -l <= m <= l.
  inline int Index(int l, int m) { return l * (l + 1) / 2 + m; }

  // Writes CoefficientCount(lmax) basis values for `unitDir` (assumed unit length) into `out`.
  // Writes nothing for an unsupported lmax.
  void EvaluateBasis(const glm::dvec3& unitDir, int lmax, double* out);

  // GPU basis table for the glyph vertex shader, always at kSHMaxOrder: kBasisTexelsPerDirection RGBA
  // texels per direction. Texel d * kBasisTexelsPerDirection + t holds (Y_4t, Y_4t+1, Y_4t+2, Y_4t+3) of
  // direction d, with Y_45..Y_47 = 0 - the same packing as the per-instance coefficient texels, so the
  // shader evaluates r(n) as 12 vec4 dot products. kBasisFloatsPerDirection floats per direction.
  std::vector<float> BuildBasisTable(const std::vector<glm::vec3>& unitDirs);

  // Regularised least-squares fit matrix B+ = (B^T B + lambda I)^-1 B^T, row-major with
  // CoefficientCount(lmax) rows and unitDirs.size() columns, so coefficients = B+ * samples.
  // Returns an empty vector (and logs) for an unsupported lmax, no directions or a failed solve.
  std::vector<double> BuildFitMatrix(const std::vector<glm::vec3>& unitDirs, int lmax, double lambda);

  // Numerical sanity check of the basis (no test framework exists, so this is invoked by hand):
  // orthonormality over icosphere level-4 directions and a fit round-trip on level-3 directions.
  // Logs the measured errors through EngineLog and returns whether both are within tolerance.
  bool SelfTestRealSH();
}
