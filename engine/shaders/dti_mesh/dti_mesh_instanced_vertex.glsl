#version 330 core

// Dedicated instanced vertex shader for the DTI seed points: the shared
// vertex_instanced.glsl layout (positions/normals + per-instance matrix) plus
// a whole-instance world-space slice-plane cut. Pairs with
// dti_mesh_fragment.glsl.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

// Per-instance model matrix, one vec4 per column (divisor 1).
// Must match Geometry::DrawInstanced's attribute locations/layout.
layout (location = 4) in vec4 aInstanceModelMatrixCol0;
layout (location = 5) in vec4 aInstanceModelMatrixCol1;
layout (location = 6) in vec4 aInstanceModelMatrixCol2;
layout (location = 7) in vec4 aInstanceModelMatrixCol3;

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

// Frame-wide clip plane (set by MriScene when "Cut Geometry" is on).
// Unset => enabled reads false => nothing is clipped.
struct ClipPlaneUniforms {
  bool enabled;
  vec3 normal;   // world space, unit length
  float offset;  // instance REMOVED when dot(normal, centre) - offset > epsilon
};

// World-space tolerance for the instance-centre clip test; keeps instances
// whose centre lies on the plane.
const float kClipCentreEpsilon = 1e-4;

uniform CameraUniforms camera;
// Shared outer/group transform applied to every instance (the owning
// GameObject's own transform) - combined with each instance's own matrix as
// gameObject.modelMatrix * instanceModelMatrix. Lets the whole instanced
// batch be moved or animated as a group without re-uploading the per-
// instance buffer. See GameObject::BuildInstancedRenderProxy.
uniform GameObjectUniforms gameObject;
uniform ClipPlaneUniforms clipPlane;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
// Constant per instance: 1.0 when the instance centre is on the removed side
// of the clip plane (whole instance discarded), otherwise -1.0.
out float fragClipDistance;

void main()
{
  mat4 instanceModelMatrix = mat4(
    aInstanceModelMatrixCol0,
    aInstanceModelMatrixCol1,
    aInstanceModelMatrixCol2,
    aInstanceModelMatrixCol3);

  mat4 worldModelMatrix = gameObject.modelMatrix * instanceModelMatrix;

  vec4 worldPosition = worldModelMatrix * vec4(aPos, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(worldModelMatrix)));

  gl_Position = camera.projectionMatrix
              * camera.viewMatrix
              * worldPosition;

  fragWorldPosition = vec3(worldPosition);
  fragWorldNormal = normalize(normalMatrix * aNormal);

  // Whole-instance clip by centre, so instances are never sliced open.
  vec3 centreWorld = (worldModelMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  bool removed = clipPlane.enabled
    && dot(clipPlane.normal, centreWorld) - clipPlane.offset > kClipCentreEpsilon;
  fragClipDistance = removed ? 1.0 : -1.0;
}
