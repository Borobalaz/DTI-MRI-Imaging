#version 330 core

in vec3 fragWorldPosition;
in vec3 fragWorldDirection;
in vec3 fragColor;

struct MaterialUniforms {
  vec3 specularColor;
  float shininess;
};

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
  vec3 focalPoint;
  float focalSize;
};

uniform MaterialUniforms material;
uniform CameraUniforms camera;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(fragWorldDirection);
    vec3 V = normalize(camera.viewPosition - fragWorldPosition);

    // simple lighting
    vec3 lightDir = normalize(vec3(0.3, 0.8, 0.5));

    float diff = max(dot(N, lightDir), 0.0);

    vec3 baseColor = fragColor; // 👈 orientation-based color

    // specular highlight (tube feel)
    vec3 R = reflect(-lightDir, N);
    float spec = pow(max(dot(V, R), 0.0), material.shininess);

    vec3 color =
        baseColor * (0.3 + 0.7 * diff) +
        material.specularColor * spec * 0.5;

    FragColor = vec4(color, 1.0);
}