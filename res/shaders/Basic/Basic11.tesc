#version 400 core

layout(vertices = 3) out;

in vec3 v_ModelPos[];
out vec3 tc_ModelPos[];

uniform int u_TessLevel;

void main() {
    tc_ModelPos[gl_InvocationID] = v_ModelPos[gl_InvocationID];
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;

    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = u_TessLevel;
        gl_TessLevelOuter[1] = u_TessLevel;
        gl_TessLevelOuter[2] = u_TessLevel;
        gl_TessLevelInner[0] = u_TessLevel;
    }
}