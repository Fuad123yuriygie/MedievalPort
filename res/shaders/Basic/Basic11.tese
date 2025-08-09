#version 400 core

layout(triangles, equal_spacing, cw) in;

in vec3 tc_ModelPos[];
out vec2 v_TexCoord; // If you want to pass texcoords to frag shader

uniform mat4 u_MVP;

void main() {
    vec3 bary = vec3(gl_TessCoord.x, gl_TessCoord.y, gl_TessCoord.z);

    // Interpolate model position
    vec3 modelPos =
        bary.x * tc_ModelPos[0] +
        bary.y * tc_ModelPos[1] +
        bary.z * tc_ModelPos[2];

    // // Apply sine wave in model space
    // float amplitude = 0.2;
    // float frequency = 5.0;
    // modelPos.y += amplitude * sin(frequency * modelPos.x + frequency * modelPos.z);

    gl_Position = u_MVP * vec4(modelPos, 1.0);
}