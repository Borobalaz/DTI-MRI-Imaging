#version 330 core

#define MAX_VOLUME_TEXTURES 8

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

struct ShaderUniforms {
  float density;
  float sliceZ;
  int selectedChannel;
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
  if (rayEnd <= rayStart || abs(rayDirectionObject.z) < 1e-7)
  {
    discard;
  }

  float sliceObjectZ = clamp(shader.sliceZ, 0.0, 1.0) - 0.5;
  float tSlice = (sliceObjectZ - rayOriginObject.z) / rayDirectionObject.z;
  if (tSlice < rayStart || tSlice > rayEnd)
  {
    discard;
  }

  vec3 samplePositionObject = rayOriginObject + rayDirectionObject * tSlice;
  vec3 textureCoord = samplePositionObject + vec3(0.5);
  vec3 samplePositionWorld = vec3(volumeObject.modelMatrix * vec4(samplePositionObject, 1.0));

  if (any(lessThan(textureCoord, vec3(0.0))) || any(greaterThan(textureCoord, vec3(1.0))))
  {
    discard;
  }

  vec3 principalDirection = texture(volumeTextures[2], textureCoord).rgb;
  vec3 color = abs(principalDirection);

  float gain = max(shader.density, 1e-4);
  float alpha = clamp(length(color) * gain, 0.0, 1.0);
  if (alpha < 1e-3)
  {
    discard;
  }

  gl_FragDepth = ComputeDepth(samplePositionWorld);

  FragColor = vec4(clamp(color, 0.0, 1.0), alpha);
}
