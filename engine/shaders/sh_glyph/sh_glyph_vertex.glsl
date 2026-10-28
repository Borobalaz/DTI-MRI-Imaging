#version 330 core

// Spherical-harmonics glyph vertex shader.
// Displaces a shared unit icosphere (aPos = unit direction n) by the per-instance SH radius
// r(n) = sum_k c_k * Y_k(n), using a precomputed per-vertex basis table fetched by gl_VertexID.
// Shading normals are computed per fragment in screen space (sh_glyph_fragment.glsl).
// Convention: MRtrix3 / tournier07 real even basis, lmax 8, 45 coefficients.

layout (location = 0) in vec3 aPos;      // Unit direction n (icosphere, radius 1)
layout (location = 2) in vec2 aTexCoord;

// Per-instance model matrix, one vec4 per column (divisor 1).
// Must match the attribute locations/layout of Geometry::DrawInstanced. Translation + uniform cell scale.
layout (location = 4) in vec4 aInstanceModelMatrixCol0;
layout (location = 5) in vec4 aInstanceModelMatrixCol1;
layout (location = 6) in vec4 aInstanceModelMatrixCol2;
layout (location = 7) in vec4 aInstanceModelMatrixCol3;

// Per-instance max positive radius over the mesh directions (divisor 1).
layout (location = 8) in float aInstanceMaxRadius;

// Texels per mesh vertex in shGlyph.basis; equals the coefficient texels per instance.
#define SH_BASIS_TEXELS_PER_VERTEX 12

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
  // RGBA32F, SH_BASIS_TEXELS_PER_VERTEX texels per mesh vertex: texel t holds
  // (Y_4t, Y_4t+1, Y_4t+2, Y_4t+3) of the vertex direction, Y_45..Y_47 = 0.
  samplerBuffer basis;
  // Coefficient texels per instance in instanceCoefficients (12 = 48 floats).
  int coefficientTexelsPerInstance;
  float scale;
  bool normalize;
};

uniform CameraUniforms camera;
// Shared outer/group transform applied to every instance - combined with each
// instance matrix as gameObject.modelMatrix * instanceModelMatrix.
uniform GameObjectUniforms gameObject;
uniform SHGlyphUniforms shGlyph;

// RGBA32F, coefficientTexelsPerInstance texels per instance. Coefficient k is
// texelFetch(instanceCoefficients, gl_InstanceID * texels + k / 4)[k % 4];
// entries k >= 45 are zero padding.
uniform samplerBuffer instanceCoefficients;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragDirectionColor;

void main()
{
  vec3 n = normalize(aPos);

  // ---- r(n) = sum_k c_k Y_k(n): 12 vec4 dot products (both tables zero-padded to 48) ----
  int coefficientBase = gl_InstanceID * shGlyph.coefficientTexelsPerInstance;
  int basisBase = gl_VertexID * SH_BASIS_TEXELS_PER_VERTEX;

  float r = 0.0;
  for (int t = 0; t < SH_BASIS_TEXELS_PER_VERTEX; ++t)
  {
    r += dot(texelFetch(instanceCoefficients, coefficientBase + t),
             texelFetch(shGlyph.basis, basisBase + t));
  }

  // ---- Per-glyph max normalization ----
  if (shGlyph.normalize && aInstanceMaxRadius > 0.0)
  {
    r /= aInstanceMaxRadius;
  }

  // ---- Position (negative lobes clamped to 0) ----
  vec3 localPosition = n * (max(r, 0.0) * shGlyph.scale);

  mat4 instanceModelMatrix = mat4(
    aInstanceModelMatrixCol0,
    aInstanceModelMatrixCol1,
    aInstanceModelMatrixCol2,
    aInstanceModelMatrixCol3);

  vec4 worldPosition = gameObject.modelMatrix * (instanceModelMatrix * vec4(localPosition, 1.0));

  gl_Position = camera.projectionMatrix * camera.viewMatrix * worldPosition;

  fragTexCoord = aTexCoord;
  fragWorldPosition = vec3(worldPosition);
  // Directionally-encoded color (DEC) from the object-space direction.
  fragDirectionColor = abs(n);
}
