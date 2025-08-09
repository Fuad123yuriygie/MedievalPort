#version 400 core

layout(location = 0) in vec3 a_Position; // Vertex position
layout(location = 1) in vec3 a_Normal;   // Vertex normal
layout(location = 2) in vec2 a_TexCoord; // Texture coordinates
layout(location = 3) in int a_MaterialID;

uniform mat4 u_MVP;       // Model-View-Projection matrix

out vec2 v_TexCoord;      // Pass texture coordinates to fragment shader
flat out int v_MaterialID;

void main() {
    v_TexCoord = a_TexCoord;                           // Pass texture coordinates
    v_MaterialID = a_MaterialID;
    gl_Position = u_MVP * vec4(a_Position, 1.0);       // Transform vertex position to clip space
}
