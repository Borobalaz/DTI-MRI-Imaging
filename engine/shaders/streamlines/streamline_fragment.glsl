#version 330 core

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec3 fragColor;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
  vec3 focalPoint;
  float focalSize;
};

uniform CameraUniforms camera;

out vec4 FragColor;

void main()
{
    vec3 normal = normalize(fragWorldNormal);
    vec3 viewDirection = normalize(camera.viewPosition - fragWorldPosition);

    float diffuse = max(dot(normal, viewDirection), 0.0);
    float shading = mix(0.25, 1.0, diffuse);

    FragColor = vec4(fragColor * shading, 1.0);
}