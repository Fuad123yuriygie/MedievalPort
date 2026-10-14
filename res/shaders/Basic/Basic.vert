#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_MVP;
uniform mat3 u_NormalMatrix;
out vec2 v_TexCoord;
out vec3 v_WorldNormal;

void main() {
    v_TexCoord = a_TexCoord;
    v_WorldNormal = u_NormalMatrix * a_Normal;
    gl_Position = u_MVP * vec4(a_Position, 1.0);
}
