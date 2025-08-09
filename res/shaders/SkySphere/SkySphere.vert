#version 330 core
layout(location = 0) in vec3 aPos;
out vec3 TexCoords;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    TexCoords = aPos;
    vec4 pos = u_Projection * u_View * vec4(aPos, 1.0);
    gl_Position = pos.xyww; // Set w = w for depth = 1.0
}
