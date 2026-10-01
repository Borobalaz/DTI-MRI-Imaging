#version 330 core

in vec3 fragWorldNormal;

out vec4 FragColor;

void main()
{
  vec3 normal = normalize(fragWorldNormal);
  // Map normal from [-1, 1] to [0, 1] so it can be displayed as a color
  vec3 normalColor = normal * 0.5 + 0.5;
  FragColor = vec4(normalColor, 1.0);
}
