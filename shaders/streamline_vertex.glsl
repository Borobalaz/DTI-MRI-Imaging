#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aDirection;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
};

struct GameObjectUniforms {
  mat4 modelMatrix;
};

uniform CameraUniforms camera;
uniform GameObjectUniforms gameObject;

out vec3 fragWorldPosition;
out vec3 fragWorldDirection;

void main()
{
  vec4 worldPosition = gameObject.modelMatrix * vec4(aPos, 1.0);
  mat3 directionMatrix = mat3(gameObject.modelMatrix);
  fragWorldPosition = vec3(worldPosition);
  fragWorldDirection = normalize(directionMatrix * aDirection);
  gl_Position = camera.projectionMatrix * camera.viewMatrix * worldPosition;
}
