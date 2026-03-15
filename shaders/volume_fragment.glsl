#version 330 core

#define MAX_VOLUME_TEXTURES 4

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
  float stepSize;
  float opacityScale;
  float intensityScale;
  float threshold;
  int maxSteps;
  int textureCount;
  int voxelKind;
  int renderMode;
  int matrixRows;
  int matrixCols;
};

uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;
uniform VolumeUniforms volume;
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

mat3 SampleMatrixVoxel(vec3 textureCoord)
{
    vec3 row0 = texture(volumeTextures[0], textureCoord).rgb;
    vec3 row1 = texture(volumeTextures[1], textureCoord).rgb;
    vec3 row2 = texture(volumeTextures[2], textureCoord).rgb;

    return mat3(
        vec3(row0.x, row1.x, row2.x),
        vec3(row0.y, row1.y, row2.y),
        vec3(row0.z, row1.z, row2.z)
    );
}

float SampleScalarValue(vec3 textureCoord)
{
    if (volume.voxelKind == 4)
    {
        mat3 tensor = SampleMatrixVoxel(textureCoord);

        if (volume.renderMode == 1)
        {
            return tensor[0][0] + tensor[1][1] + tensor[2][2];
        }
        if (volume.renderMode == 2)
        {
            return determinant(tensor);
        }
        if (volume.renderMode == 3)
        {
            return sqrt(dot(tensor[0], tensor[0]) + dot(tensor[1], tensor[1]) + dot(tensor[2], tensor[2]));
        }
        if (volume.renderMode == 4)
        {
            return tensor[0][0];
        }
        if (volume.renderMode == 5)
        {
            return tensor[1][1];
        }
        if (volume.renderMode == 6)
        {
            return tensor[2][2];
        }

        return tensor[0][0] + tensor[1][1] + tensor[2][2];
    }

    return texture(volumeTextures[0], textureCoord).r;
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

    float currentDistance = max(tEnter, 0.0);
    vec3 accumulatedColor = vec3(0.0);
    float accumulatedAlpha = 0.0;

    for (int stepIndex = 0; stepIndex < volume.maxSteps; ++stepIndex)
    {
        if (currentDistance > tExit || accumulatedAlpha >= 0.98)
        {
            break;
        }

        vec3 samplePosition = rayOriginObject + rayDirectionObject * currentDistance;
        vec3 textureCoord = samplePosition + vec3(0.5);
        float sampledValue = SampleScalarValue(textureCoord);
        float density = max((sampledValue - volume.threshold) * volume.intensityScale, 0.0);
        float alpha = 1.0 - exp(-density * volume.opacityScale);
        vec3 color = vec3(clamp(density, 0.0, 1.0));

        accumulatedColor += (1.0 - accumulatedAlpha) * alpha * color;
        accumulatedAlpha += (1.0 - accumulatedAlpha) * alpha;
        currentDistance += max(volume.stepSize, 0.001);
    }

    if (accumulatedAlpha <= 0.001)
    {
        discard;
    }

    FragColor = vec4(accumulatedColor, accumulatedAlpha);
}
