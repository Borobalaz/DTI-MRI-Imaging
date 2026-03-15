#version 330 core

#define MAX_LIGHTS 16

in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

struct MaterialUniforms {
  sampler2D diffuseTexture;
  sampler2D specularTexture;
  vec3 ambientColor;
  vec3 diffuseColor;
  vec3 specularColor;
  float shininess;
  bool hasDiffuseTexture;
  bool hasSpecularTexture;
};

struct CameraUniforms {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec3 viewPosition;
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

uniform MaterialUniforms material;
uniform CameraUniforms camera;
uniform LightUniforms lights[MAX_LIGHTS];
uniform int lightCount;

out vec4 FragColor;

vec3 ResolveDiffuseAlbedo()
{
  vec3 baseColor = material.diffuseColor;
  if (material.hasDiffuseTexture)
  {
    baseColor *= texture(material.diffuseTexture, fragTexCoord).rgb;
  }
  return baseColor;
}

vec3 ResolveSpecularColor()
{
  vec3 baseSpecular = material.specularColor;
  if (material.hasSpecularTexture)
  {
    baseSpecular *= texture(material.specularTexture, fragTexCoord).rgb;
  }
  return baseSpecular;
}

vec3 ComputeLight(LightUniforms light,
                  vec3 normal,
                  vec3 fragmentPosition,
                  vec3 viewDirection,
                  vec3 diffuseAlbedo,
                  vec3 specularAlbedo)
{
  vec3 lightDirection = normalize(-light.direction);
  float attenuation = 1.0;

  if (light.type == 0)
  {
    vec3 lightToFragment = light.position - fragmentPosition;
    float distanceToLight = length(lightToFragment);
    lightDirection = normalize(lightToFragment);
    attenuation = 1.0 / (light.constant
      + light.linear * distanceToLight
      + light.quadratic * distanceToLight * distanceToLight);
  }

  float diffuseStrength = max(dot(normal, lightDirection), 0.0);
  vec3 reflectDirection = reflect(-lightDirection, normal);
  float specularStrength = pow(max(dot(viewDirection, reflectDirection), 0.0), material.shininess);

  vec3 ambientTerm = light.ambient * material.ambientColor;
  vec3 diffuseTerm = light.diffuse * diffuseStrength * diffuseAlbedo;
  vec3 specularTerm = light.specular * specularStrength * specularAlbedo;
  return (ambientTerm + diffuseTerm + specularTerm) * attenuation;
}

void main()
{
  vec3 normal = normalize(fragWorldNormal);
  vec3 viewDirection = normalize(camera.viewPosition - fragWorldPosition);
  vec3 diffuseAlbedo = ResolveDiffuseAlbedo();
  vec3 specularAlbedo = ResolveSpecularColor();

  vec3 color = vec3(0.0);

  for (int i = 0; i < lightCount; ++i)
  {
    color += ComputeLight(lights[i], normal, fragWorldPosition, viewDirection, diffuseAlbedo, specularAlbedo);
  }

  FragColor = vec4(color, 1.0);
  //FragColor = vec4(1.0, 1.0, 1.0 ,1.0);
}