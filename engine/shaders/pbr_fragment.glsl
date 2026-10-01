#version 330 core

#define MAX_LIGHTS 16
#define PI 3.14159265359

// ========== PBR Material Uniforms ==========
struct MaterialUniforms
{
  sampler2D albedo;
  sampler2D roughness;
  sampler2D metallic;
  sampler2D normal;
  sampler2D emissive;

  vec3 albedoFactor;
  float roughnessFactor;
  float metallicFactor;
  vec3 emissiveFactor;

  bool hasAlbedo;
  bool hasRoughness;
  bool hasMetallic;
  bool hasNormal;
  bool hasEmissive;
};

uniform MaterialUniforms material;

struct Material
{
  vec3 baseColor;
  float roughness;
  float metallic;
  vec3 emissive;
};

// ========== PBR BRDF Functions ==========

/**
 * @brief Fresnel-Schlick approximation.
 */
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/**
 * @brief Trowbridge-Reitz GGX Normal Distribution Function.
 */
float DistributionGGX(float NdotH, float roughness)
{
  float a = roughness * roughness;
  float a2 = a * a;
  float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
  denom = PI * denom * denom;
  return a2 / max(denom, 0.0001);
}

/**
 * @brief Schlick-GGX Geometry Function (single-directional).
 */
float GeometrySchlickGGX(float NdotV, float roughness)
{
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  float denom = NdotV * (1.0 - k) + k;
  return NdotV / max(denom, 0.0001);
}

/**
 * @brief Schlick-GGX Geometry Function (combined).
 */
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
  float ggx2 = GeometrySchlickGGX(NdotV, roughness);
  float ggx1 = GeometrySchlickGGX(NdotL, roughness);
  return ggx1 * ggx2;
}

/**
 * @brief Compute material's Fresnel reflectance (F0).
 */
vec3 ComputeF0(vec3 baseColor, float metallic)
{
  const vec3 DielectricF0 = vec3(0.04);
  return mix(DielectricF0, baseColor, metallic);
}

/**
 * @brief Cook-Torrance BRDF for a single light source.
 */
vec3 CookTorranceBRDF(vec3 N, vec3 V, vec3 L, in Material material, vec3 lightColor)
{
  vec3 H = normalize(V + L);
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float NdotH = max(dot(N, H), 0.0);
  float VdotH = max(dot(V, H), 0.0);

  vec3 F0 = ComputeF0(material.baseColor, material.metallic);
  vec3 F = FresnelSchlick(VdotH, F0);

  float D = DistributionGGX(NdotH, material.roughness);
  float G = GeometrySmith(NdotV, NdotL, material.roughness);

  vec3 kS = F;
  vec3 kD = (1.0 - kS) * (1.0 - material.metallic);

  vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
  vec3 diffuse = material.baseColor / PI;

  return (kD * diffuse + specular) * lightColor * NdotL;
}

// ========== Fragment Shader Inputs ==========
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec3 fragWorldTangent;
in vec3 fragWorldBitangent;

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
  vec3 focalPoint;
  float focalSize;
};

struct LightUniforms {
  int type;
  vec3 position;
  vec3 direction;
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  float intensity;
  float constant;
  float linear;
  float quadratic;
};

uniform CameraUniforms camera;
uniform LightUniforms lights[MAX_LIGHTS];
uniform int lightCount;

out vec4 FragColor;

// ========== Utility Functions ==========

vec3 SampleTextureOrDefault(sampler2D textureSampler, bool hasTexture, vec3 defaultValue)
{
  if (hasTexture)
    return texture(textureSampler, fragTexCoord).rgb;
  return defaultValue;
}

float SampleTextureOrDefault(sampler2D textureSampler, bool hasTexture, float defaultValue)
{
  if (hasTexture)
    return texture(textureSampler, fragTexCoord).r;
  return defaultValue;
}

vec3 UnpackNormal(vec3 normalMap, bool useNormalMap, vec3 surfaceNormal)
{
  if (!useNormalMap)
    return normalize(surfaceNormal);

  vec3 normal = normalize(normalMap * 2.0 - 1.0);

  vec3 N = normalize(surfaceNormal);
  vec3 T = normalize(fragWorldTangent);
  vec3 B = normalize(fragWorldBitangent);

  T = normalize(T - dot(T, N) * N);
  B = cross(N, T);

  return normalize(mat3(T, B, N) * normal);
}

vec3 ComputeLightRadiance(LightUniforms light,
                         vec3 normal,
                         vec3 viewDir,
                         in Material materialData)
{
  vec3 lightDir;
  float attenuation = 1.0;

  if (light.type == 0)  // Point light
  {
    vec3 toLight = light.position - fragWorldPosition;
    float distance = length(toLight);
    lightDir = normalize(toLight);
    attenuation = 1.0 / (light.constant
      + light.linear * distance
      + light.quadratic * distance * distance);
  }
  else  // Directional light (type == 1)
  {
    lightDir = normalize(-light.direction);
  }

  vec3 lightRadiance = light.diffuse * attenuation * light.intensity;
  vec3 radiance = CookTorranceBRDF(normal, viewDir, lightDir, materialData, lightRadiance);
  radiance += light.ambient * materialData.baseColor * 0.1 * light.intensity;

  return radiance;
}

vec3 ReinhardToneMap(vec3 color)
{
  return color / (color + vec3(1.0));
}

vec3 GammaCorrect(vec3 color)
{
  return pow(color, vec3(1.0 / 2.2));
}

void main()
{
  // Sample material properties
  vec3 baseColor = SampleTextureOrDefault(
    material.albedo,
    material.hasAlbedo,
    material.albedoFactor
  );
  
  float roughness = SampleTextureOrDefault(
    material.roughness,
    material.hasRoughness,
    material.roughnessFactor
  );
  
  float metallic = SampleTextureOrDefault(
    material.metallic,
    material.hasMetallic,
    material.metallicFactor
  );

  // Create material for this fragment
  Material fragMaterial;
  fragMaterial.baseColor = baseColor;
  fragMaterial.roughness = roughness;
  fragMaterial.metallic = metallic;
  fragMaterial.emissive = material.emissiveFactor;

  // Sample and unpack normal map
  vec3 normalMapSample = SampleTextureOrDefault(
    material.normal,
    material.hasNormal,
    vec3(0.5, 0.5, 1.0)
  );
  vec3 normal = UnpackNormal(normalMapSample, material.hasNormal, fragWorldNormal);
  // Compute view direction
  vec3 viewDir = normalize(camera.viewPosition - fragWorldPosition);

  // Accumulate radiance from all lights
  vec3 radiance = vec3(0.0);
  for (int i = 0; i < lightCount; ++i)
  {
    radiance += ComputeLightRadiance(lights[i], normal, viewDir, fragMaterial);
  }

  // Add emissive contribution
  vec3 emissiveColor = SampleTextureOrDefault(
    material.emissive,
    material.hasEmissive,
    fragMaterial.emissive
  );
  radiance += emissiveColor;

  // Tone map and gamma correct
  vec3 toneMapped = ReinhardToneMap(radiance);
  vec3 color = GammaCorrect(toneMapped);

  FragColor = vec4(color, 1.0);
}
