#version 330 core

// Spherical-harmonics glyph vertex shader.
// Displaces a shared unit icosphere (aPos = unit direction n) by the per-
// instance SH radius r(n) = sum_k c_k * Y_k(n), using a precomputed per-vertex
// basis table (Y_k and its tangential gradient) fetched by gl_VertexID.
// Convention: MRtrix3 / tournier07 real even basis, lmax 8, 45 coefficients.

layout (location = 0) in vec3 aPos;      // Unit direction n (icosphere, radius 1)
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;

// Per-instance model matrix, one vec4 per column (divisor 1).
// Must match Geometry::DrawInstanced's attribute locations/layout.
// Translation + uniform cell scale.
layout (location = 4) in vec4 aInstanceModelMatrixCol0;
layout (location = 5) in vec4 aInstanceModelMatrixCol1;
layout (location = 6) in vec4 aInstanceModelMatrixCol2;
layout (location = 7) in vec4 aInstanceModelMatrixCol3;

// Per-instance max positive radius over the mesh directions (divisor 1).
layout (location = 8) in float aInstanceMaxRadius;

#define SH_COEFFICIENT_COUNT 45
#define SH_BASIS_TEXELS_PER_VERTEX 45

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
  vec3 focalPoint;
  float focalSize;
};

struct GameObjectUniforms {
  mat4 modelMatrix;
};

struct SHGlyphUniforms {
  // RGBA32F, 45 texels per mesh vertex: vec4(Y_k, gradY_k.xyz), where the
  // gradient is the tangential gradient on the unit sphere, object space.
  samplerBuffer basis;
  // Coefficient texels per instance in instanceCoefficients (12 = 48 floats).
  int coefficientTexelsPerInstance;
  float scale;
  bool normalize;
};

uniform CameraUniforms camera;
// Shared outer/group transform applied to every instance - combined with each
// instance's own matrix as gameObject.modelMatrix * instanceModelMatrix.
uniform GameObjectUniforms gameObject;
uniform SHGlyphUniforms shGlyph;

// RGBA32F, coefficientTexelsPerInstance texels per instance. Coefficient k is
// texelFetch(instanceCoefficients, gl_InstanceID * texels + k / 4)[k % 4];
// entries k >= 45 are zero padding.
uniform samplerBuffer instanceCoefficients;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragWorldTangent;
out vec3 fragWorldBitangent;
out vec3 fragDirectionColor;

void main()
{
  vec3 n = normalize(aPos);

  // ---- Evaluate r(n) and its tangential gradient g(n) ----
  float r = 0.0;
  vec3 g = vec3(0.0);

  int coefficientBase = gl_InstanceID * shGlyph.coefficientTexelsPerInstance;
  int basisBase = gl_VertexID * SH_BASIS_TEXELS_PER_VERTEX;

  for (int t = 0; t < shGlyph.coefficientTexelsPerInstance; ++t)
  {
    vec4 coefficients = texelFetch(instanceCoefficients, coefficientBase + t);
    for (int j = 0; j < 4; ++j)
    {
      int k = t * 4 + j;
      if (k >= SH_COEFFICIENT_COUNT)
        break;  // Zero padding past the last real coefficient.

      float c = coefficients[j];
      vec4 b = texelFetch(shGlyph.basis, basisBase + k);
      r += c * b.x;
      g += c * b.yzw;
    }
  }

  // ---- Per-glyph max normalization ----
  if (shGlyph.normalize && aInstanceMaxRadius > 0.0)
  {
    r /= aInstanceMaxRadius;
    g /= aInstanceMaxRadius;
  }

  // ---- Position (negative lobes clamped to 0) ----
  float rc = max(r, 0.0);
  vec3 localPosition = n * rc * shGlyph.scale;

  mat4 instanceModelMatrix = mat4(
    aInstanceModelMatrixCol0,
    aInstanceModelMatrixCol1,
    aInstanceModelMatrixCol2,
    aInstanceModelMatrixCol3);

  mat4 worldModelMatrix = gameObject.modelMatrix * instanceModelMatrix;

  vec4 worldPosition = worldModelMatrix * vec4(localPosition, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(worldModelMatrix)));

  gl_Position = camera.projectionMatrix
              * camera.viewMatrix
              * worldPosition;

  // ---- Normal of the surface p(n) = r(n) * n: proportional to r*n - grad r ----
  vec3 objectNormal = n;
  if (r > 0.0)
  {
    vec3 candidate = r * n - g;
    if (dot(candidate, candidate) > 1e-12)
      objectNormal = normalize(candidate);
  }

  fragTexCoord = aTexCoord;
  fragWorldPosition = vec3(worldPosition);
  vec3 worldNormal = normalize(normalMatrix * objectNormal);
  vec3 worldTangent = mat3(worldModelMatrix) * aTangent;
  worldTangent -= dot(worldTangent, worldNormal) * worldNormal;
  if (dot(worldTangent, worldTangent) < 1e-8)
  {
    vec3 fallbackAxis = abs(worldNormal.y) < 0.999
      ? vec3(0.0, 1.0, 0.0)
      : vec3(1.0, 0.0, 0.0);
    worldTangent = cross(fallbackAxis, worldNormal);
  }

  fragWorldNormal = worldNormal;
  fragWorldTangent = normalize(worldTangent);
  fragWorldBitangent = normalize(cross(worldNormal, fragWorldTangent));

  // Directionally-encoded color (DEC) from the object-space direction.
  fragDirectionColor = abs(n);
}
