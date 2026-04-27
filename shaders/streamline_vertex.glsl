#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aDirection;

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

out vec3 fragWorldPosition;
out vec3 fragWorldDirection;
out vec3 fragColor;

void main()
{
    vec4 worldPos = gameObject.modelMatrix * vec4(aPos, 1.0);
    fragWorldPosition = worldPos.xyz;

    mat3 normalMatrix = mat3(gameObject.modelMatrix);
    vec3 tangent = normalize(normalMatrix * aDirection);

    fragWorldDirection = tangent;

    // Convert direction -> color (abs removes sign ambiguity)
    fragColor = abs(tangent);

    gl_Position =
        camera.projectionMatrix *
        camera.viewMatrix *
        worldPos;
}