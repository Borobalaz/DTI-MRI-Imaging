#version 330 core

in vec2 fragTexCoord;

out vec4 FragColor;

uniform sampler2D sceneColor;
uniform vec2 invScreenSize;

const float FXAA_SPAN_MAX = 8.0;
const float FXAA_REDUCE_MUL = 1.0 / 8.0;
const float FXAA_REDUCE_MIN = 1.0 / 128.0;
const vec3 LUMA_WEIGHTS = vec3(0.299, 0.587, 0.114);

void main()
{
    vec2 texel = invScreenSize;

    vec3 rgbNW = texture(sceneColor, fragTexCoord + vec2(-texel.x, -texel.y)).rgb;
    vec3 rgbNE = texture(sceneColor, fragTexCoord + vec2( texel.x, -texel.y)).rgb;
    vec3 rgbSW = texture(sceneColor, fragTexCoord + vec2(-texel.x,  texel.y)).rgb;
    vec3 rgbSE = texture(sceneColor, fragTexCoord + vec2( texel.x,  texel.y)).rgb;
    vec3 rgbM  = texture(sceneColor, fragTexCoord).rgb;

    float lumaNW = dot(rgbNW, LUMA_WEIGHTS);
    float lumaNE = dot(rgbNE, LUMA_WEIGHTS);
    float lumaSW = dot(rgbSW, LUMA_WEIGHTS);
    float lumaSE = dot(rgbSE, LUMA_WEIGHTS);
    float lumaM  = dot(rgbM,  LUMA_WEIGHTS);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * texel;

    vec3 rgbA = 0.5 * (
        texture(sceneColor, fragTexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(sceneColor, fragTexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);

    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(sceneColor, fragTexCoord + dir * -0.5).rgb +
        texture(sceneColor, fragTexCoord + dir *  0.5).rgb);

    float lumaB = dot(rgbB, LUMA_WEIGHTS);

    FragColor = vec4((lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB, 1.0);
}
