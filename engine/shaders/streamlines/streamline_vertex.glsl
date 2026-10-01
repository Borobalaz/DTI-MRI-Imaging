#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;

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
out vec3 fragWorldNormal;
out vec3 fragTangent;
out vec3 fragColor;

void main()
{
    vec4 worldPos = gameObject.modelMatrix * vec4(aPos, 1.0);
    fragWorldPosition = worldPos.xyz;

    mat3 normalMatrix = mat3(gameObject.modelMatrix);
    
    // Surface normal (radial direction) for cylindrical lighting
    vec3 surfaceNormal = normalize(normalMatrix * aNormal);
    fragWorldNormal = surfaceNormal;
    
    // Tangent direction for color
    vec3 tangent = normalize(normalMatrix * aTangent);
    fragTangent = tangent;

    // Convert tangent direction -> color (abs removes sign ambiguity)
    fragColor = abs(tangent);

    gl_Position =
        camera.projectionMatrix *
        camera.viewMatrix *
        worldPos;
}