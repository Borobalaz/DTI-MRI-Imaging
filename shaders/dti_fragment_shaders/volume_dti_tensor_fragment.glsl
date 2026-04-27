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
  float time;
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

float SampleSelectedChannel(vec3 textureCoord, int channel)
{
  vec3 tensorDiag = texture(volumeTextures[0], textureCoord).rgb;     // Dxx, Dyy, Dzz
  vec3 tensorOffDiag = texture(volumeTextures[1], textureCoord).rgb;  // Dxy, Dxz, Dyz
  vec3 principalEv = texture(volumeTextures[2], textureCoord).rgb;    // EVx, EVy, EVz
  vec3 eigenvalues = texture(volumeTextures[3], textureCoord).rgb;    // L1, L2, L3
  vec4 scalars = texture(volumeTextures[4], textureCoord).rgba;       // FA, MD, AD, RD

  if (channel == 0) return tensorDiag.r;     // Dxx
  if (channel == 1) return tensorDiag.g;     // Dyy
  if (channel == 2) return tensorDiag.b;     // Dzz
  if (channel == 3) return tensorOffDiag.r;  // Dxy
  if (channel == 4) return tensorOffDiag.g;  // Dxz
  if (channel == 5) return tensorOffDiag.b;  // Dyz
  if (channel == 6) return principalEv.r;    // EVx
  if (channel == 7) return principalEv.g;    // EVy
  if (channel == 8) return principalEv.b;    // EVz
  if (channel == 9) return scalars.r;        // FA
  if (channel == 10) return scalars.g;       // MD
  if (channel == 11) return scalars.b;       // AD
  if (channel == 12) return scalars.a;       // RD
  if (channel == 13) return eigenvalues.r;   // L1
  if (channel == 14) return eigenvalues.g;   // L2
  if (channel == 15) return eigenvalues.b;   // L3

  return 0.0;
}

vec3 EncodeChannelColor(int channel, float value, float gain)
{
  // EV components are signed in [-1, 1], map to visible [0, 1].
  if (channel >= 6 && channel <= 8)
  {
    float mapped = clamp(0.5 + 0.5 * value, 0.0, 1.0);
    return vec3(mapped);
  }

  float mapped = clamp(value * gain, 0.0, 1.0);
  return vec3(mapped);
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

  float sliceObjectZ = sin(0.2*shader.time) * 0.5;
  //float sliceObjectZ = clamp(shader.sliceZ, 0.0, 1.0) - 0.5;
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
  vec3 samplePositionWorld = vec3(volumeObject.modelMatrix * vec4(samplePositionObject, 1.0));

  if (any(lessThan(textureCoord, vec3(0.0))) || any(greaterThan(textureCoord, vec3(1.0))))
  {
    discard;
  }

  int selected = clamp(shader.selectedChannel, 0, 15);
  float value = SampleSelectedChannel(textureCoord, selected);

  float gain = max(shader.density, 1e-4);
  vec3 color = vec3(value);

  vec3 ev = texture(volumeTextures[2], textureCoord).rgb;
  bool asd = abs(ev.r - ev.g) > 5e-5;
  asd = asd && abs(ev.r - ev.b) > 5e-5;
  asd = asd && abs(ev.g - ev.b) > 5e-5;
  float alpha = asd ? 1.0 : 0.0;

  gl_FragDepth = ComputeDepth(samplePositionWorld);

  FragColor = vec4(color, alpha);
}