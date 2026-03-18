#version 330 core

#define MAX_VOLUME_TEXTURES 4

in vec3 fragObjectPosition;
in vec3 fragWorldPosition;

out vec4 fragColor;

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
};

struct ShaderUniforms {
  vec3 testUniform; // Example uniform for demonstration
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

vec3 DominantEigenvector(mat3 tensor, float eigenvalue)
{
    mat3 shifted = tensor - mat3(
        eigenvalue, 0.0, 0.0,
        0.0, eigenvalue, 0.0,
        0.0, 0.0, eigenvalue);

    vec3 row0 = vec3(shifted[0][0], shifted[1][0], shifted[2][0]);
    vec3 row1 = vec3(shifted[0][1], shifted[1][1], shifted[2][1]);
    vec3 row2 = vec3(shifted[0][2], shifted[1][2], shifted[2][2]);

    vec3 candidate0 = cross(row0, row1);
    vec3 candidate1 = cross(row0, row2);
    vec3 candidate2 = cross(row1, row2);

    float candidate0Length = dot(candidate0, candidate0);
    float candidate1Length = dot(candidate1, candidate1);
    float candidate2Length = dot(candidate2, candidate2);

    vec3 bestCandidate = candidate0;
    float bestLength = candidate0Length;

    if (candidate1Length > bestLength)
    {
        bestCandidate = candidate1;
        bestLength = candidate1Length;
    }

    if (candidate2Length > bestLength)
    {
        bestCandidate = candidate2;
        bestLength = candidate2Length;
    }

    if (bestLength <= 1e-12)
    {
        vec3 fallback = vec3(1.0, 0.0, 0.0);
        fallback = SafeNormalize(tensor * fallback, fallback);
        fallback = SafeNormalize(tensor * fallback, fallback);
        fallback = SafeNormalize(tensor * fallback, fallback);
        return fallback;
    }

    return SafeNormalize(bestCandidate, vec3(1.0, 0.0, 0.0));
}

vec3 ComputeCurveNormal(vec3 tangent, vec3 viewDirection)
{
    vec3 normal = viewDirection - tangent * dot(viewDirection, tangent);

    if (dot(normal, normal) <= 1e-8)
    {
        vec3 fallbackAxis = abs(tangent.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        normal = cross(tangent, fallbackAxis);
    }

    return SafeNormalize(normal, vec3(0.0, 0.0, 1.0));
}

vec4 ShadeCurveHit(vec3 samplePosition, vec3 tangentObject, float eigenvalue)
{
    vec3 worldPosition = vec3(volumeObject.modelMatrix * vec4(samplePosition, 1.0));
    vec3 tangentWorld = SafeNormalize(mat3(volumeObject.modelMatrix) * tangentObject, vec3(1.0, 0.0, 0.0));
    vec3 viewDirection = SafeNormalize(camera.viewPosition - worldPosition, vec3(0.0, 0.0, 1.0));
    vec3 normal = ComputeCurveNormal(tangentWorld, viewDirection);
    vec3 lightDirection = SafeNormalize(vec3(0.35, 0.8, 0.45), vec3(0.0, 1.0, 0.0));
    vec3 halfVector = SafeNormalize(lightDirection + viewDirection, viewDirection);

    float tangentViewDot = dot(tangentWorld, viewDirection);
    float rim = sqrt(max(1.0 - tangentViewDot * tangentViewDot, 0.0));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfVector), 0.0), 24.0);
    float strength = clamp((eigenvalue - volume.threshold) * volume.intensityScale, 0.0, 1.0);

    vec3 baseColor = mix(vec3(0.14, 0.18, 0.24), abs(tangentWorld), 0.85);
    vec3 color = baseColor * (0.2 + 0.5 * diffuse + 0.5 * rim);
    color += vec3(0.35) * specular;
    color *= 0.4 + 0.6 * strength;

    float alpha = clamp((0.3 + 0.7 * rim) * (0.35 + strength * volume.opacityScale), 0.0, 1.0);
    return vec4(clamp(color, 0.0, 1.0), alpha);
}

void main()
{
    fragColor = vec4(shader.testUniform, 1.0); // Example usage of shader uniform
    return;
    
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

    for (int stepIndex = 0; stepIndex < volume.maxSteps; ++stepIndex)
    {
        if (currentDistance > tExit)
        {
            break;
        }

        vec3 samplePosition = rayOriginObject + rayDirectionObject * currentDistance;
        vec3 textureCoord = samplePosition + vec3(0.5);

        mat3 tensor = SampleMatrixVoxel(textureCoord);
        vec3 eigenvalues = SymmetricEigenvalues(tensor);
        float dominantEigenvalue = eigenvalues.x;

        if (dominantEigenvalue > volume.threshold)
        {
            vec3 tangentObject = DominantEigenvector(tensor, dominantEigenvalue);
            fragColor = ShadeCurveHit(samplePosition, tangentObject, dominantEigenvalue);
            return;
        }

        currentDistance += max(volume.stepSize, 0.001);
    }

    discard;
}
