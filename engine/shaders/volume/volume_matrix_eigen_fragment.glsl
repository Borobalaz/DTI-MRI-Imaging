#version 330 core

#define MAX_VOLUME_TEXTURES 4
#define MAX_MARCH_STEPS 512

in vec3 fragObjectPosition;
in vec3 fragWorldPosition;

out vec4 fragColor;

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
    float faThreshold;
    float opacityScale;
    float stepMultiplier;
    vec3 tintColor;
    float specularPower;
};

uniform CameraUniforms camera;
uniform VolumeObjectUniforms volumeObject;
uniform VolumeUniforms volume;
uniform ShaderUniforms shader;
uniform sampler3D volumeTextures[MAX_VOLUME_TEXTURES];

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 1e-12)
    {
        return fallback;
    }

    return value * inversesqrt(lengthSquared);
}

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

vec3 SortDescending(vec3 values)
{
    vec3 sorted = values;
    if (sorted.x < sorted.y)
    {
        float tmp = sorted.x;
        sorted.x = sorted.y;
        sorted.y = tmp;
    }
    if (sorted.y < sorted.z)
    {
        float tmp = sorted.y;
        sorted.y = sorted.z;
        sorted.z = tmp;
    }
    if (sorted.x < sorted.y)
    {
        float tmp = sorted.x;
        sorted.x = sorted.y;
        sorted.y = tmp;
    }
    return sorted;
}

vec3 SymmetricEigenvalues(mat3 tensor)
{
    float a00 = tensor[0][0];
    float a11 = tensor[1][1];
    float a22 = tensor[2][2];
    float a01 = tensor[0][1];
    float a02 = tensor[0][2];
    float a12 = tensor[1][2];

    float p1 = a01 * a01 + a02 * a02 + a12 * a12;
    if (p1 <= 1e-7)
    {
        return SortDescending(vec3(a00, a11, a22));
    }

    float q = (a00 + a11 + a22) / 3.0;
    float b00 = a00 - q;
    float b11 = a11 - q;
    float b22 = a22 - q;
    float p2 = (b00 * b00 + b11 * b11 + b22 * b22 + 2.0 * p1) / 6.0;

    mat3 B = tensor - mat3(q, 0.0, 0.0,
                                                 0.0, q, 0.0,
                                                 0.0, 0.0, q);

    float p = sqrt(max(p2, 1e-12));
    float r = determinant(B) / (2.0 * p * p * p);
    r = clamp(r, -1.0, 1.0);

    float phi = acos(r) / 3.0;
    const float twoPiOverThree = 2.09439510239;

    float eig0 = q + 2.0 * p * cos(phi);
    float eig2 = q + 2.0 * p * cos(phi + twoPiOverThree);
    float eig1 = 3.0 * q - eig0 - eig2;

    return SortDescending(vec3(eig0, eig1, eig2));
}

float ComputeFractionalAnisotropy(vec3 eigenvalues)
{
    float meanValue = (eigenvalues.x + eigenvalues.y + eigenvalues.z) / 3.0;
    vec3 delta = eigenvalues - vec3(meanValue);
    float numerator = sqrt(max(1.5 * dot(delta, delta), 0.0));
    float denominator = sqrt(max(dot(eigenvalues, eigenvalues), 1e-12));
    return clamp(numerator / denominator, 0.0, 1.0);
}

vec3 PrincipalDirection(mat3 tensor)
{
    vec3 direction = vec3(1.0, 0.0, 0.0);
    for (int i = 0; i < 8; ++i)
    {
        direction = SafeNormalize(tensor * direction, direction);
    }
    return direction;
}

vec3 ComputeTubeNormal(vec3 tangent, vec3 viewDirection)
{
    vec3 normal = viewDirection - tangent * dot(viewDirection, tangent);
    if (dot(normal, normal) <= 1e-8)
    {
        vec3 fallbackAxis = abs(tangent.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        normal = cross(tangent, fallbackAxis);
    }

    return SafeNormalize(normal, vec3(0.0, 0.0, 1.0));
}

vec4 ShadeDtiHit(vec3 samplePosition, vec3 tangentObject, float fractionalAnisotropy)
{
    vec3 worldPosition = vec3(volumeObject.modelMatrix * vec4(samplePosition, 1.0));
    vec3 tangentWorld = SafeNormalize(mat3(volumeObject.modelMatrix) * tangentObject, vec3(1.0, 0.0, 0.0));
    vec3 viewDirection = SafeNormalize(camera.viewPosition - worldPosition, vec3(0.0, 0.0, 1.0));
    vec3 normal = ComputeTubeNormal(tangentWorld, viewDirection);
    vec3 lightDirection = SafeNormalize(vec3(0.35, 0.8, 0.45), vec3(0.0, 1.0, 0.0));
    vec3 halfVector = SafeNormalize(lightDirection + viewDirection, viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specularPower = max(shader.specularPower, 1.0);
    float specular = pow(max(dot(normal, halfVector), 0.0), specularPower);

    // DTI convention: principal direction -> RGB orientation map.
    vec3 baseColor = abs(tangentWorld);
    vec3 shadedColor = baseColor * (0.25 + 0.75 * diffuse) + vec3(0.25) * specular;
    shadedColor *= shader.tintColor;

    float opacityScale = clamp(shader.opacityScale, 0.1, 4.0);
    float alpha = clamp(fractionalAnisotropy * opacityScale, 0.0, 1.0);

    return vec4(clamp(shadedColor, 0.0, 1.0), alpha);
}

void main()
{
    if (volume.textureCount < 3)
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

    float currentDistance = max(tEnter, 0.0);
    float maxDimension = max(max(volume.dimensions.x, volume.dimensions.y), volume.dimensions.z);
    float stepBase = clamp(1.0 / max(maxDimension, 32.0), 0.001, 0.03);
    float stepMultiplier = clamp(shader.stepMultiplier, 0.1, 4.0);
    float stepSize = stepBase * stepMultiplier;
    float faThreshold = clamp(shader.faThreshold, 0.0, 1.0);

    for (int stepIndex = 0; stepIndex < MAX_MARCH_STEPS && currentDistance <= tExit; ++stepIndex)
    {
        vec3 samplePosition = rayOriginObject + rayDirectionObject * currentDistance;
        vec3 textureCoord = samplePosition + vec3(0.5);

        if (all(greaterThanEqual(textureCoord, vec3(0.0))) &&
                all(lessThanEqual(textureCoord, vec3(1.0))))
    {
            mat3 tensor = SampleMatrixVoxel(textureCoord);
            vec3 eigenvalues = SymmetricEigenvalues(tensor);
            float fa = ComputeFractionalAnisotropy(eigenvalues);

            if (fa > faThreshold)
            {
                vec3 tangentObject = PrincipalDirection(tensor);
                fragColor = ShadeDtiHit(samplePosition, tangentObject, fa);
                return;
            }
    }

        currentDistance += stepSize;
    }

    discard;
}
