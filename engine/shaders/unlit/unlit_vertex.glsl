#version 330 core

// Unlit vertex shader: position only. Used by the slice plane outline (lines) and fill (triangles).
layout (location = 0) in vec3 aPos;

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

uniform CameraUniforms camera;
uniform GameObjectUniforms gameObject;

void main()
{
  gl_Position = camera.projectionMatrix * camera.viewMatrix * gameObject.modelMatrix * vec4(aPos, 1.0);
}
