#version 330 core

layout (location = 0) in vec3 aPos;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
};

struct VolumeObjectUniforms {
  mat4 modelMatrix;
  mat4 inverseModelMatrix;
};

uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;

out vec3 fragObjectPosition;
out vec3 fragWorldPosition;

void main()
{
    vec4 worldPosition = volumeObject.modelMatrix * vec4(aPos, 1.0);
    fragObjectPosition = aPos;
    fragWorldPosition = worldPosition.xyz;
    gl_Position = camera.projectionMatrix * camera.viewMatrix * worldPosition;
}
