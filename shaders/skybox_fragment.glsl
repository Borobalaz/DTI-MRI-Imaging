#version 330 core

in vec3 texCoords;

out vec4 FragColor;

uniform samplerCube skyboxTexture;

void main()
{
    FragColor = texture(skyboxTexture, normalize(texCoords));
}