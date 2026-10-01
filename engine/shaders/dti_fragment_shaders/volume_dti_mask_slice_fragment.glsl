#version 330 core

in vec3 fragObjectPosition;
out vec4 FragColor;

#define MAX_VOLUME_TEXTURES 8
uniform sampler3D volumeTextures[MAX_VOLUME_TEXTURES];

struct ShaderFields
{
  float sliceZ;
  float opacity;
};

uniform ShaderFields shader;

void main()
{
  vec3 texCoord = vec3(fragObjectPosition.xy + 0.5, clamp(shader.sliceZ, 0.0, 1.0));

  if (any(lessThan(texCoord, vec3(0.0))) || any(greaterThan(texCoord, vec3(1.0))))
  {
    discard;
  }

  float mask = texture(volumeTextures[5], texCoord).r;
  if (mask <= 0.5)
  {
    discard;
  }

  float alpha = clamp(shader.opacity, 0.0, 1.0);
  FragColor = vec4(vec3(0.0), alpha);
}
