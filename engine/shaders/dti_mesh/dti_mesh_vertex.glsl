#version 330 core

// Dedicated vertex shader for the DTI brain surface mesh: the shared
// vertex.glsl attribute layout (positions/normals only) plus a world-space
// slice-plane cut. Pairs with dti_mesh_fragment.glsl.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

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

// Frame-wide clip plane (set by DtiVolumeScene when "Cut Geometry" is on).
// Unset => enabled reads false => nothing is clipped.
struct ClipPlaneUniforms {
  bool enabled;
  vec3 normal;   // world space, unit length
  float offset;  // fragment REMOVED when dot(normal, p) - offset > 0
};

uniform CameraUniforms camera;
uniform GameObjectUniforms gameObject;
uniform ClipPlaneUniforms clipPlane;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
// Signed distance to the clip plane (> 0 = removed side; -1 when clipping is off).
out float fragClipDistance;

void main()
{
  vec4 worldPosition = gameObject.modelMatrix * vec4(aPos, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(gameObject.modelMatrix)));

  gl_Position = camera.projectionMatrix
              * camera.viewMatrix
              * worldPosition;

  fragWorldPosition = vec3(worldPosition);
  fragWorldNormal = normalize(normalMatrix * aNormal);
  fragClipDistance = clipPlane.enabled
    ? dot(clipPlane.normal, worldPosition.xyz) - clipPlane.offset
    : -1.0;
}
