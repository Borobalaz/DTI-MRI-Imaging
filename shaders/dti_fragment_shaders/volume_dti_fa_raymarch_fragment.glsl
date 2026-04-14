#version 330 core

#define MAX_VOLUME_TEXTURES 8
#define MAX_MARCH_STEPS_CAP 2048

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
  float density;
  float threshold;
  float stepSize;
  int maxSteps;
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
  vec3 invDir = 1.0 / rayDirection;
  vec3 t0 = (boxMin - rayOrigin) * invDir;
  vec3 t1 = (boxMax - rayOrigin) * invDir;
  vec3 smaller = min(t0, t1);
  vec3 larger = max(t0, t1);

  tMin = max(max(smaller.x, smaller.y), smaller.z);
  tMax = min(min(larger.x, larger.y), larger.z);
  return tMax >= max(tMin, 0.0);
}

float SampleFA(vec3 textureCoord)
{
  // Texture layout:
  // 0: (Dxx, Dyy, Dzz)
  // 1: (Dxy, Dxz, Dyz)
  // 2: (EVx, EVy, EVz)
  // 3: (L1, L2, L3)
  // 4: (FA, MD, AD, RD)
  return texture(volumeTextures[4], textureCoord).r;
}

void main()
{
  if (volume.textureCount < 5)
  {
    discard;
  }

  vec3 rayOriginObject = vec3(volumeObject.inverseModelMatrix * vec4(camera.viewPosition, 1.0));
  vec3 rayDirectionObject = normalize(fragObjectPosition - rayOriginObject);

  float tEnter = 0.0;
  float tExit = 0.0;
  if (!IntersectBox(rayOriginObject, rayDirectionObject, tEnter, tExit))
  {
    discard;
  }

  float rayStart = max(tEnter, 0.0);
  float rayEnd = tExit;
  if (rayEnd <= rayStart)
  {
    discard;
  }

  float maxDimension = max(max(volume.dimensions.x, volume.dimensions.y), volume.dimensions.z);
  float autoStepSize = 1.0 / max(maxDimension, 64.0);
  autoStepSize = clamp(autoStepSize, 0.001, 0.03);

  float stepSize = shader.stepSize;
  if (stepSize <= 0.0)
  {
    stepSize = autoStepSize;
  }
  stepSize = clamp(stepSize, 0.00025, 0.1);

  int maxSteps = clamp(shader.maxSteps, 1, MAX_MARCH_STEPS_CAP);

  float threshold = clamp(shader.threshold, 0.0, 1.0);
  float density = max(shader.density, 0.0);

  vec3 accumulatedColor = vec3(0.0);
  float accumulatedAlpha = 0.0;

  float t = rayStart;
  for (int stepIndex = 0; stepIndex < MAX_MARCH_STEPS_CAP && stepIndex < maxSteps && t <= rayEnd; ++stepIndex)
  {
    vec3 samplePositionObject = rayOriginObject + rayDirectionObject * t;
    vec3 textureCoord = samplePositionObject + vec3(0.5);

    if (all(greaterThanEqual(textureCoord, vec3(0.0))) &&
        all(lessThanEqual(textureCoord, vec3(1.0))))
    {
      float fa = clamp(SampleFA(textureCoord), 0.0, 1.0);
      float normalized = clamp((fa - threshold) / max(1.0 - threshold, 1e-5), 0.0, 1.0);

      float sampleAlpha = normalized * density * stepSize * maxDimension * 0.2;
      sampleAlpha = clamp(sampleAlpha, 0.0, 1.0);

      vec3 sampleColor = vec3(normalized);
      float transmittance = 1.0 - accumulatedAlpha;
      accumulatedColor += transmittance * sampleColor * sampleAlpha;
      accumulatedAlpha += transmittance * sampleAlpha;

      if (accumulatedAlpha >= 0.98)
      {
        break;
      }
    }

    t += stepSize;
  }

  if (accumulatedAlpha <= 0.001)
  {
    discard;
  }

  FragColor = vec4(accumulatedColor, accumulatedAlpha);
}
