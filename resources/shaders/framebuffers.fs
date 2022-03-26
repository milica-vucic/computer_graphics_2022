#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;

uniform int effect;
uniform float gamma;
uniform bool hdr;
uniform bool bloom;
uniform float exposure;

const float offset = 1.0 / 300.0;
vec3 col = vec3(0.0);
vec3 sampleTex[9];

vec2 offsets[9] = vec2[](
    vec2(-offset,  offset), // top-left
    vec2( 0.0f,    offset), // top-center
    vec2( offset,  offset), // top-right
    vec2(-offset,  0.0f),   // center-left
    vec2( 0.0f,    0.0f),   // center-center
    vec2( offset,  0.0f),   // center-right
    vec2(-offset, -offset), // bottom-left
    vec2( 0.0f,   -offset), // bottom-center
    vec2( offset, -offset)  // bottom-right
);

float blurKernel[9] = float[](
    1.0 / 16, 2.0 / 16, 1.0 / 16,
    2.0 / 16, 4.0 / 16, 2.0 / 16,
    1.0 / 16, 2.0 / 16, 1.0 / 16
);

float edgeDetectionKernel[9] = float[](
    1.0, 1.0, 1.0,
    1.0, -8.0, 1.0,
    1.0, 1.0, 1.0
);

void main ()
{
//     const float gamma = 2.2;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
    vec3 hdrColor = texture(scene, TexCoords).rgb;

    if (effect == 0) {
        for(int i = 0; i < 9; i++)
            sampleTex[i] = vec3(texture(scene, TexCoords.st + offsets[i]));
            col = vec3(0.0);
                for(int i = 0; i < 9; i++)
                    col += sampleTex[i] * blurKernel[i];
        FragColor = vec4(col, 1.0);
    } else if (effect == 1) {
        FragColor = texture(scene, TexCoords);
        float average = 0.2126 * FragColor.r + 0.7152 * FragColor.g + 0.0722 * FragColor.b;
        FragColor = vec4(average, average, average, 1.0);
    } else if (effect == 2) {
        for(int i = 0; i < 9; i++)
            sampleTex[i] = vec3(texture(scene, TexCoords.st + offsets[i]));
            col = vec3(0.0);
                for(int i = 0; i < 9; i++)
                    col += sampleTex[i] * edgeDetectionKernel[i];
        FragColor = vec4(col, 1.0);
    } else if (effect == 3) {
        if (hdr) {
            if (bloom)
                hdrColor += bloomColor;
            vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
            FragColor = vec4(pow(result, vec3(1.0 / gamma)), 1.0);
        } else {
            FragColor = vec4(hdrColor, 1.0);
        }
    }
}