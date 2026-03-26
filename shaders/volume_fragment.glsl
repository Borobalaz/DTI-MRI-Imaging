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

struct DtiUniforms {
  int metricCount;
  int activeMetric;
  int activeMetricTexture;
  float threshold;
  float opacityScale;
};

uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;
uniform VolumeUniforms volume;
uniform ShaderUniforms shader;
uniform sampler3D volumeTextures[MAX_VOLUME_TEXTURES];
uniform DtiUniforms dti;

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
  // For DTI volumes, use the active metric texture if available
  if (dti.metricCount > 0 && dti.activeMetricTexture >= 0 && dti.activeMetricTexture < volume.textureCount)
  {
    return texture(volumeTextures[dti.activeMetricTexture], textureCoord).r;
  }
  // Fallback to first texture for non-DTI volumes
  return texture(volumeTextures[0], textureCoord).r;
}

void main()
{
  if (volume.textureCount < 1)
  {
    discard;
  }

  // Transform the ray into the volume's local space
  vec3 rayOriginObject = vec3(volumeObject.inverseModelMatrix * vec4(camera.viewPosition, 1.0));
  vec3 rayDirectionObject = normalize(fragObjectPosition - rayOriginObject);

  // Intersect the ray with the volume's bounding box
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

    // Front-to-back compositing raymarch.
    vec3 accumulatedColor = vec3(0.0);
    float accumulatedAlpha = 0.0;

    float t = rayStart;
    for (int stepIndex = 0; stepIndex < MAX_MARCH_STEPS && t <= rayEnd; ++stepIndex)
    {
      vec3 samplePositionObject = rayOriginObject + rayDirectionObject * t;
      vec3 textureCoord = samplePositionObject + vec3(0.5); // local space to texture space

      if (all(greaterThanEqual(textureCoord, vec3(0.0))) &&
          all(lessThanEqual(textureCoord, vec3(1.0))))
      {
        float scalar = SampleScalar(textureCoord);
        
        // Use DTI threshold if available, otherwise fall back to shader.threshold
        float threshold = (dti.metricCount > 0) ? dti.threshold : shader.threshold;
        float normalizedDensity = clamp(
          (scalar - threshold) / max(1.0 - threshold, 1e-5),
          0.0,
          1.0);

        // Use DTI opacity scale if available, otherwise use default scaling
        float opacityScale = (dti.metricCount > 0) ? dti.opacityScale : 1.0;
        float sampleAlpha = normalizedDensity * stepSize * maxDimension * 0.2 * opacityScale;
        sampleAlpha = clamp(sampleAlpha, 0.0, 1.0);
        vec3 sampleColor = shader.color * normalizedDensity;

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

    if (accumulatedAlpha > 0.001)
    {
      FragColor = vec4(accumulatedColor, accumulatedAlpha);
      return;
    }
  }

  discard;
}
