#version 450 core

in vec2 v_TexCoord;
uniform sampler2DArray u_TextureArray;
uniform int u_TextureLayer;
uniform vec4 u_BaseColor;     // rgb diffuse tint, a opacity
uniform vec4 u_UvTransform;   // xy scale, zw offset
uniform int u_UvWrap;         // 0 repeat, 1 clamp to edge
uniform float u_AlphaCutoff;  // > 0 enables alpha masking
out vec4 FragColor;

void main() {
    vec2 uv = v_TexCoord * u_UvTransform.xy + u_UvTransform.zw;
    uv = u_UvWrap == 1 ? clamp(uv, 0.0, 1.0) : fract(uv);
    // sRGB storage is decoded by the sampler; work in linear light and let the framebuffer
    // encode the output exactly once.
    vec4 color = texture(u_TextureArray, vec3(uv, float(u_TextureLayer))) * u_BaseColor;
    if (u_AlphaCutoff > 0.0 && color.a < u_AlphaCutoff) {
        discard;
    }
    FragColor = color;
}
