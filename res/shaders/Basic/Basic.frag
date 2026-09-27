#version 450 core

in vec2 v_TexCoord;
uniform sampler2DArray u_TextureArray;
uniform int u_TextureLayer;
out vec4 FragColor;

void main() {
    FragColor = texture(u_TextureArray, vec3(v_TexCoord, float(u_TextureLayer)));
}
