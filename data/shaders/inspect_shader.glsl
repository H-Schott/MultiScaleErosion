#version 430 core

#ifdef VERTEX_SHADER
void main(void)
{
    vec4 vertices[4] = vec4[4](vec4(-1.0, -1.0, 1.0, 1.0),
    vec4( 1.0, -1.0, 1.0, 1.0),
    vec4(-1.0,  1.0, 1.0, 1.0),
    vec4( 1.0,  1.0, 1.0, 1.0));
    vec4 pos = vertices[gl_VertexID];
    gl_Position = pos;

}
#endif

#ifdef FRAGMENT_SHADER

layout(binding = 0, std430) buffer InBuffer { float data[]; };

uniform vec3 maxColor;
uniform vec3 minColor;

// Field2D data
uniform vec2 zRange;
uniform float K;
uniform ivec2 texSize;

uniform int shadingMode;
uniform int n_bands;

out vec4 FragColor;

void main() {
    ivec2 buffer_coord = ivec2(gl_FragCoord.xy);
    int buffer_idx = n_bands * (buffer_coord.x + buffer_coord.y * texSize.x);
    vec3 value;
    switch (n_bands) {
        case 1:
            value = vec3(data[buffer_idx]);
            break;
        case 2:
            value = vec3(vec2(
                data[buffer_idx],
                data[buffer_idx + 1]
            ), 1.0);
            break;
        case 3:
            value = vec3(
                data[buffer_idx],
                data[buffer_idx + 1],
                data[buffer_idx + 2]
            );
            break;
        default:
            value = vec3(data[buffer_coord.x + buffer_coord.y * texSize.x]);
            break;
    }

    value = (value - zRange.x) / (zRange.y - zRange.x);
    vec3 color = mix(minColor, maxColor, value);
    FragColor = vec4(color, 1.0);

}
#endif


