#version 330 core

#define MAX_VOLUME_TEXTURES 4
#define MAX_MARCH_STEPS 512

in vec3 fragObjectPosition;
in vec3 fragWorldPosition;

out vec4 FragColor;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
};

struct VolumeObjectUniforms {
  mat4 modelMatrix;
  mat4 inverseModelMatrix;
};

struct VolumeUniforms {
  vec3 dimensions;
  vec3 spacing;
  int textureCount;
};

struct ShaderUniforms {
  float threshold;
  vec3 color;
};

uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;
uniform VolumeUniforms volume;
uniform ShaderUniforms shader;
uniform sampler3D volumeTextures[MAX_VOLUME_TEXTURES];

bool IntersectBox(vec3 rayOrigin, vec3 rayDirection, out float tMin, out float tMax)
{
    vec3 boxMin = vec3(-0.5);
    vec3 boxMax = vec3(0.5);
    vec3 inverseDirection = 1.0 / rayDirection;
    vec3 t0 = (boxMin - rayOrigin) * inverseDirection;
    vec3 t1 = (boxMax - rayOrigin) * inverseDirection;
    vec3 smaller = min(t0, t1);
    vec3 larger = max(t0, t1);

    tMin = max(max(smaller.x, smaller.y), smaller.z);
    tMax = min(min(larger.x, larger.y), larger.z);
    return tMax >= max(tMin, 0.0);
}

float SampleScalar(vec3 textureCoord)
{
    return texture(volumeTextures[0], textureCoord).r;
}

void main()
{
    if (volume.textureCount < 1)
    {
        discard;
    }

    vec3 rayOriginObject = vec3(volumeObject.inverseModelMatrix * vec4(camera.viewPosition, 1.0));
    vec3 rayDirectionObject = normalize(fragObjectPosition - rayOriginObject);

    float tEnter = 0.0;
    float tExit = 0.0;
    if (IntersectBox(rayOriginObject, rayDirectionObject, tEnter, tExit))
    {
      float rayStart = max(tEnter, 0.0);
      float rayEnd = tExit;
      if (rayEnd <= rayStart)
      {
          discard;
      }

      float maxDimension = max(max(volume.dimensions.x, volume.dimensions.y), volume.dimensions.z);
      float stepSize = 1.0 / max(maxDimension, 32.0);
      stepSize = clamp(stepSize, 0.001, 0.05);

      float t = rayStart;
      for (int stepIndex = 0; stepIndex < MAX_MARCH_STEPS && t <= rayEnd; ++stepIndex)
      {
          vec3 samplePositionObject = rayOriginObject + rayDirectionObject * t;
          vec3 textureCoord = samplePositionObject + vec3(0.5);

          if (all(greaterThanEqual(textureCoord, vec3(0.0))) &&
              all(lessThanEqual(textureCoord, vec3(1.0))))
          {
              float scalar = SampleScalar(textureCoord);
              if (scalar > shader.threshold)
              {
                  FragColor = vec4(shader.color, 1.0);
                  return;
              }
          }

          t += stepSize;
      }
    }

    discard;
}
