#version 330 core

#define MAX_VOLUME_TEXTURES 8
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
  float sliceZ;
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

mat3 SampleTensor(vec3 textureCoord)
{
  float Dxx = texture(volumeTextures[0], textureCoord).r;
  float Dyy = texture(volumeTextures[1], textureCoord).r;
  float Dzz = texture(volumeTextures[2], textureCoord).r;
  float Dxy = texture(volumeTextures[3], textureCoord).r;
  float Dxz = texture(volumeTextures[4], textureCoord).r;
  float Dyz = texture(volumeTextures[5], textureCoord).r;

  return mat3(
    Dxx, Dxy, Dxz,
    Dxy, Dyy, Dyz,
    Dxz, Dyz, Dzz
  );
}

vec3 PrincipalDirection(mat3 tensor)
{
  vec3 v = normalize(vec3(0.57735, 0.57735, 0.57735));
  for (int i = 0; i < 6; ++i)
  {
    v = tensor * v;
    float lenV = length(v);
    if (lenV < 1e-7)
    {
      return vec3(0.0);
    }
    v /= lenV;
  }
  return v;
}

float PrincipalEigenvalue(mat3 tensor, vec3 principalDir)
{
  return dot(principalDir, tensor * principalDir);
}

void main()
{
  if (volume.textureCount < 6)
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

  float sliceObjectZ = clamp(shader.sliceZ, 0.0, 1.0) - 0.5;
  if (abs(rayDirectionObject.z) < 1e-7)
  {
    discard;
  }

  float tSlice = (sliceObjectZ - rayOriginObject.z) / rayDirectionObject.z;
  if (tSlice < rayStart || tSlice > rayEnd)
  {
    discard;
  }

  vec3 samplePositionObject = rayOriginObject + rayDirectionObject * tSlice;
  vec3 textureCoord = samplePositionObject + vec3(0.5);

  if (any(lessThan(textureCoord, vec3(0.0))) || any(greaterThan(textureCoord, vec3(1.0))))
  {
    discard;
  }

  mat3 tensor = SampleTensor(textureCoord);
  vec3 principalDir = PrincipalDirection(tensor);
  if (length(principalDir) < 1e-6)
  {
    discard;
  }

  float lambda1 = max(PrincipalEigenvalue(tensor, principalDir), 0.0);

  // Direction-encoded color in [0, 1].
  vec3 color = 0.5 * (principalDir + vec3(1.0));

  // Map low-magnitude eigenvalues to visible opacity using configurable gain.
  float gain = max(shader.density, 1e-4);
  float alpha = clamp(lambda1 * gain, 0.0, 1.0);
  if (alpha <= 1e-5)
  {
    discard;
  }

  FragColor = vec4(color, alpha);
}