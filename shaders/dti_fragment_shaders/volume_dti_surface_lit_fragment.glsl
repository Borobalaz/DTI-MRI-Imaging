#version 330 core

#define MAX_VOLUME_TEXTURES 8
#define MAX_MARCH_STEPS_CAP 2048
#define MAX_LIGHTS 16

in vec3 fragObjectPosition;
in vec3 fragWorldPosition;

out vec4 FragColor;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
  vec3 focalPoint;
  float focalSize;
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

struct LightUniforms {
  int type;
  vec3 position;
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  float constant;
  float linear;
  float quadratic;
};

struct ShaderUniforms {
  float threshold;
  float stepSize;
  int maxSteps;
  float specularStrength;
  float shininess;
  float aoStrength;
  float aoRadius;
  int aoSamples;
};

uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;
uniform VolumeUniforms volume;
uniform LightUniforms lights[MAX_LIGHTS];
uniform int lightCount;
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
  // Texture 4 stores scalar channels as (FA, MD, AD, RD).
  return texture(volumeTextures[4], textureCoord).r;
}

vec3 ComputeGradient(vec3 textureCoord)
{
  vec3 dim = max(volume.dimensions, vec3(1.0));
  vec3 texel = 1.0 / dim;

  float gx = SampleFA(clamp(textureCoord + vec3(texel.x, 0.0, 0.0), vec3(0.0), vec3(1.0))) -
             SampleFA(clamp(textureCoord - vec3(texel.x, 0.0, 0.0), vec3(0.0), vec3(1.0)));
  float gy = SampleFA(clamp(textureCoord + vec3(0.0, texel.y, 0.0), vec3(0.0), vec3(1.0))) -
             SampleFA(clamp(textureCoord - vec3(0.0, texel.y, 0.0), vec3(0.0), vec3(1.0)));
  float gz = SampleFA(clamp(textureCoord + vec3(0.0, 0.0, texel.z), vec3(0.0), vec3(1.0))) -
             SampleFA(clamp(textureCoord - vec3(0.0, 0.0, texel.z), vec3(0.0), vec3(1.0)));

  return vec3(gx, gy, gz);
}

vec3 ComputeObjectSpaceNormal(vec3 textureCoord)
{
  vec3 gradient = ComputeGradient(textureCoord);
  float gradientLengthSq = dot(gradient, gradient);
  if (gradientLengthSq <= 1e-12)
  {
    return vec3(0.0, 0.0, 1.0);
  }

  return normalize(gradient);
}

float ComputeAmbientOcclusion(vec3 textureCoord, vec3 normalObject)
{
  int aoSamples = clamp(shader.aoSamples, 1, 24);
  float radius = max(shader.aoRadius, 0.001);
  float threshold = clamp(shader.threshold, 0.0, 1.0);

  float occlusion = 0.0;
  for (int i = 1; i <= 24; ++i)
  {
    if (i > aoSamples)
    {
      break;
    }

    float stepT = float(i) / float(aoSamples);
    vec3 sampleCoord = textureCoord + normalObject * radius * stepT;
    sampleCoord = clamp(sampleCoord, vec3(0.0), vec3(1.0));

    float sampleFA = SampleFA(sampleCoord);
    float occupancy = smoothstep(threshold * 0.9, threshold * 1.25, sampleFA);
    occlusion += occupancy;
  }

  float averaged = occlusion / float(aoSamples);
  float ao = 1.0 - clamp(averaged * max(shader.aoStrength, 0.0), 0.0, 1.0);
  return clamp(ao, 0.0, 1.0);
}

vec3 ComputeLighting(vec3 worldPosition,
                    vec3 worldNormal,
                    vec3 viewDirection,
                    vec3 albedo,
                    float ao)
{
  vec3 color = vec3(0.0);
  float shininess = max(shader.shininess, 1.0);
  float specularStrength = max(shader.specularStrength, 0.0);
  float diffuseBoost = 1.15;
  float ambientBoost = 1.2;

  for (int i = 0; i < lightCount && i < MAX_LIGHTS; ++i)
  {
    LightUniforms light = lights[i];

    vec3 lightDirection = normalize(-light.direction);
    float attenuation = 1.0;

    if (light.type == 0)
    {
      vec3 lightToFragment = light.position - worldPosition;
      float distanceToLight = max(length(lightToFragment), 1e-4);
      lightDirection = lightToFragment / distanceToLight;
      attenuation = 1.0 /
          (light.constant + light.linear * distanceToLight + light.quadratic * distanceToLight * distanceToLight);
    }

    float diffuseStrength = max(dot(worldNormal, lightDirection), 0.0);
    vec3 reflectDirection = reflect(-lightDirection, worldNormal);
    float specularTerm = pow(max(dot(viewDirection, reflectDirection), 0.0), shininess) * specularStrength;

    vec3 ambient = light.ambient * albedo * ao * ambientBoost;
    vec3 diffuse = light.diffuse * diffuseStrength * albedo * diffuseBoost;
    vec3 specular = light.specular * specularTerm;

    color += (ambient + diffuse + specular) * attenuation;
  }

  return color;
}

float ComputeDepth(vec3 worldPosition)
{
  vec4 clipPosition = camera.projectionMatrix * camera.viewMatrix * vec4(worldPosition, 1.0);
  if (abs(clipPosition.w) < 1e-7)
  {
    return 1.0;
  }

  float ndcDepth = clipPosition.z / clipPosition.w;
  return clamp(ndcDepth * 0.5 + 0.5, 0.0, 1.0);
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

  bool hit = false;
  vec3 hitObjectPos = vec3(0.0);
  vec3 hitTextureCoord = vec3(0.0);
  float previousFA = 0.0;

  float t = rayStart;
  for (int stepIndex = 0; stepIndex < MAX_MARCH_STEPS_CAP && stepIndex < maxSteps && t <= rayEnd; ++stepIndex)
  {
    vec3 samplePositionObject = rayOriginObject + rayDirectionObject * t;
    vec3 textureCoord = samplePositionObject + vec3(0.5);

    if (all(greaterThanEqual(textureCoord, vec3(0.0))) &&
        all(lessThanEqual(textureCoord, vec3(1.0))))
    {
      float fa = clamp(SampleFA(textureCoord), 0.0, 1.0);
      if (fa >= threshold)
      {
        if (stepIndex > 0)
        {
          float denom = max(abs(fa - previousFA), 1e-5);
          float refinement = clamp((threshold - previousFA) / denom, 0.0, 1.0);
          float refinedT = t - stepSize + refinement * stepSize;
          samplePositionObject = rayOriginObject + rayDirectionObject * refinedT;
          textureCoord = samplePositionObject + vec3(0.5);
        }

        hit = true;
        hitObjectPos = samplePositionObject;
        hitTextureCoord = clamp(textureCoord, vec3(0.0), vec3(1.0));
        break;
      }

      previousFA = fa;
    }

    t += stepSize;
  }

  if (!hit)
  {
    discard;
  }

  vec3 normalObject = ComputeObjectSpaceNormal(hitTextureCoord);
  vec3 normalWorld = normalize(mat3(transpose(volumeObject.inverseModelMatrix)) * normalObject);
  vec3 worldPosition = vec3(volumeObject.modelMatrix * vec4(hitObjectPos, 1.0));
  vec3 viewDirection = normalize(camera.viewPosition - worldPosition);

  vec3 baseAlbedo = vec3(0.62);
  float ao = ComputeAmbientOcclusion(hitTextureCoord, normalObject);
  vec3 litColor = ComputeLighting(worldPosition, normalWorld, viewDirection, baseAlbedo, ao);

  // Add a soft rim term to preserve contour readability without introducing chroma.
  float rim = pow(1.0 - max(dot(normalWorld, viewDirection), 0.0), 2.5);
  litColor += vec3(rim * 0.08);

  gl_FragDepth = ComputeDepth(worldPosition);

  FragColor = vec4(clamp(litColor, 0.0, 1.0), 1.0);
}
