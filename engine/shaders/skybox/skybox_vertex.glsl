#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 texCoords;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main()
{
    texCoords = aPos;
    vec4 clipPosition = projectionMatrix * viewMatrix * vec4(aPos, 1.0);
    gl_Position = clipPosition.xyww;
}