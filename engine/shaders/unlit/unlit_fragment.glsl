#version 330 core

// Unlit fragment shader: constant colour from the Material (base colour + opacity). No lighting, no
// tone mapping, no gamma: the colour is written as the display value. Opacity only has an effect in
// passes that blend (TransparentPass); MeshGeometryPass draws with blending off.
struct UnlitMaterialUniforms {
  vec3 albedoFactor;   // Material::SetBaseColor
  float opacity;       // Material::SetOpacity
};

uniform UnlitMaterialUniforms material;

out vec4 FragColor;

void main()
{
  FragColor = vec4(material.albedoFactor, material.opacity);
}
