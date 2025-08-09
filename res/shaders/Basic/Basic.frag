#version 400 core

in vec2 v_TexCoord;      // Texture coordinates from vertex shader
flat in int v_MaterialID;

uniform sampler2DArray u_TextureArray;

out vec4 FragColor;

void main() {
    // Apply texture
    FragColor = texture(u_TextureArray, vec3(v_TexCoord, v_MaterialID));
    // float texIndex = float(v_MaterialID);
    // FragColor = vec4(texIndex, texIndex, texIndex, texIndex);
}
