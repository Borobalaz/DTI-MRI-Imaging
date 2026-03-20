#version 330 core

#define MAX_MARCH_STEPS 512
#define MAX_DE_ITERATIONS 16
#define MAX_LIGHTS 16

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

struct ShaderUniforms {
  float power;
  float bailout;
  float hitEpsilon;
  float maxDistance;
  float stepScale;
  float fractalScale;
  float time;
  float animationSpeed;
  int maxSteps;
  int renderMode;
  bool animateBulb;
  vec3 fractalOffset;
  vec3 baseColor;
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


uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;
uniform ShaderUniforms shader;
uniform LightUniforms lights[MAX_LIGHTS];
uniform int lightCount;

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

float MandelbulbDistanceEstimator(vec3 p)
{
  vec3 z = p;
  float dr = 1.0;
  float r = 0.0;

  float rawPower = clamp(shader.power, 0.0, 30.0);
  if (shader.animateBulb)
  {
    float speed = max(shader.animationSpeed, 0.0);
    float oscillation = 0.5 + 0.5 * sin(shader.time * speed);
    rawPower = 30.0 * oscillation + 2.0;
  }

  float power = max(rawPower, 2.0);
  float bailout = max(shader.bailout, 2.0);

  for (int i = 0; i < MAX_DE_ITERATIONS; ++i)
  {
    r = length(z);
    if (r > bailout)
    {
      break;
    }

    float theta = acos(clamp(z.z / max(r, 1e-6), -1.0, 1.0));
    float phi = atan(z.y, z.x);

    float zr = pow(r, power);
    dr = power * pow(max(r, 1e-6), power - 1.0) * dr + 1.0;

    float thetaN = theta * power;
    float phiN = phi * power;

    z = zr * vec3(
      sin(thetaN) * cos(phiN),
      sin(phiN) * sin(thetaN),
      cos(thetaN)
    ) + p;
  }

  return 0.5 * log(max(r, 1e-6)) * r / max(dr, 1e-6);
}

float SceneDistance(vec3 p)
{
  float fractalScale = max(shader.fractalScale, 0.1);
  vec3 fractalPoint = p * fractalScale + shader.fractalOffset;
  return MandelbulbDistanceEstimator(fractalPoint) / fractalScale;
}

vec3 EstimateNormal(vec3 p)
{
  float normalStep = 0.0015 / max(shader.fractalScale, 0.1);
  vec2 e = vec2(1.0, -1.0) * normalStep;
  return normalize(
    e.xyy * SceneDistance(p + e.xyy) +
    e.yyx * SceneDistance(p + e.yyx) +
    e.yxy * SceneDistance(p + e.yxy) +
    e.xxx * SceneDistance(p + e.xxx)
  );
}

vec3 ComputeMandelbulbLighting(vec3 normalWorld, vec3 worldPosition, vec3 viewDirection, vec3 albedo)
{
  vec3 accumulated = vec3(0.0);

  for (int i = 0; i < lightCount; ++i)
  {
    vec3 lightDirection = normalize(-lights[i].direction);
    float attenuation = 1.0;

    if (lights[i].type == 0)
    {
      vec3 lightToFragment = lights[i].position - worldPosition;
      float distanceToLight = length(lightToFragment);
      lightDirection = normalize(lightToFragment);
      attenuation = 1.0 / (
        lights[i].constant +
        lights[i].linear * distanceToLight +
        lights[i].quadratic * distanceToLight * distanceToLight
      );
    }

    float diffuse = max(dot(normalWorld, lightDirection), 0.0);
    vec3 halfVector = normalize(lightDirection + viewDirection);
    float specular = pow(max(dot(normalWorld, halfVector), 0.0), 48.0);

    vec3 ambientTerm = lights[i].ambient * albedo;
    vec3 diffuseTerm = lights[i].diffuse * diffuse * albedo;
    vec3 specularTerm = lights[i].specular * specular;
    accumulated += (ambientTerm + diffuseTerm + specularTerm) * attenuation;
  }

  if (lightCount == 0)
  {
    accumulated = albedo * 0.2;
  }

  return accumulated;
}

vec3 StepColor(float t)
{
  t = clamp(t, 0.0, 1.0);

  vec3 c0 = vec3(0.05, 0.06, 0.22);
  vec3 c1 = vec3(0.12, 0.45, 0.85);
  vec3 c2 = vec3(0.22, 0.82, 0.58);
  vec3 c3 = vec3(0.98, 0.88, 0.18);

  if (t < 0.33)
  {
    return mix(c0, c1, t / 0.33);
  }
  if (t < 0.66)
  {
    return mix(c1, c2, (t - 0.33) / 0.33);
  }
  return mix(c2, c3, (t - 0.66) / 0.34);
}

void main()
{
  vec3 rayOriginObject = vec3(volumeObject.inverseModelMatrix * vec4(camera.viewPosition, 1.0));
  vec3 rayDirectionObject = normalize(fragObjectPosition - rayOriginObject);

  float tEnter = 0.0;
  float tExit = 0.0;
  if (!IntersectBox(rayOriginObject, rayDirectionObject, tEnter, tExit))
  {
    discard;
  }

  float rayT = max(tEnter, 0.0);
  float maxT = min(tExit, max(shader.maxDistance, 0.1));
  float epsilon = clamp(shader.hitEpsilon / 1000, 1e-8, 0.02);
  float stepScale = clamp(shader.stepScale, 0.2, 1.0);
  int maxSteps = clamp(shader.maxSteps, 16, MAX_MARCH_STEPS);

  for (int i = 0; i < MAX_MARCH_STEPS; ++i)
  {
    if (i >= maxSteps || rayT > maxT)
    {
      break;
    }

    vec3 samplePoint = rayOriginObject + rayDirectionObject * rayT;
    float distanceToSurface = SceneDistance(samplePoint);

    if (distanceToSurface < epsilon)
    {
      vec3 normalObject = EstimateNormal(samplePoint);
      vec3 normalWorld = normalize(mat3(transpose(volumeObject.inverseModelMatrix)) * normalObject);

      vec3 worldPosition = vec3(volumeObject.modelMatrix * vec4(samplePoint, 1.0));
      vec3 viewDirection = normalize(camera.viewPosition - worldPosition);

      int mode = clamp(shader.renderMode, 0, 1);
      vec3 albedo = shader.baseColor;
      if (mode == 1)
      {
        float stepRatio = float(i) / float(max(maxSteps - 1, 1));
        albedo = StepColor(stepRatio);
      }

      vec3 color = ComputeMandelbulbLighting(normalWorld, worldPosition, viewDirection, albedo);
      FragColor = vec4(color, 1.0);
      return;
    }

    rayT += distanceToSurface * stepScale;
  }

  discard;
}
