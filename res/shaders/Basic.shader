#shader vertex
#version 330 core

layout(location = 0) in vec3 a_Position; // Vertex position
layout(location = 1) in vec3 a_Normal;   // Vertex normal
layout(location = 2) in vec2 a_TexCoord; // Texture coordinates

uniform mat4 u_MVP;       // Model-View-Projection matrix
uniform mat4 u_Model;     // Model matrix
uniform mat4 u_NormalMat; // Normal matrix

out vec3 v_Normal;        // Pass normal to fragment shader
out vec3 v_FragPos;       // Pass fragment position to fragment shader
out vec2 v_TexCoord;      // Pass texture coordinates to fragment shader

void main() {
    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0)); // Transform vertex position to world space
    v_Normal = mat3(u_NormalMat) * a_Normal;           // Transform normal to world space
    v_TexCoord = a_TexCoord;                           // Pass texture coordinates
    gl_Position = u_MVP * vec4(a_Position, 1.0);       // Transform vertex position to clip space
}

#shader fragment
#version 330 core

in vec3 v_Normal;        // Normal vector from vertex shader
in vec3 v_FragPos;       // Fragment position from vertex shader
in vec2 v_TexCoord;      // Texture coordinates from vertex shader

uniform sampler2D u_Texture; // Texture sampler
uniform vec3 u_LightPos;     // Light position
uniform vec3 u_ViewPos;      // Camera position
uniform vec3 u_LightColor;   // Light color
uniform vec3 u_ObjectColor;  // Object color

out vec4 FragColor;

void main() {
    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * u_LightColor;

    // Diffuse lighting
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;

    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * u_LightColor;

    // Combine lighting components
    vec3 lighting = (ambient + diffuse + specular) * u_ObjectColor;

    // Apply texture
    vec4 texColor = texture(u_Texture, v_TexCoord);
    FragColor = vec4(lighting, 1.0) * texColor;
}