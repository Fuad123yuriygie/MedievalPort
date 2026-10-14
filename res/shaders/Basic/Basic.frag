#version 450 core

in vec2 v_TexCoord;
in vec3 v_WorldNormal;
uniform sampler2DArray u_TextureArray;
uniform int u_TextureLayer;
uniform vec4 u_BaseColor;     // rgb diffuse tint, a opacity
uniform vec4 u_UvTransform;   // xy scale, zw offset
uniform int u_UvWrap;         // 0 repeat, 1 clamp to edge
uniform float u_AlphaCutoff;  // > 0 enables alpha masking
uniform bool u_LightingEnabled;
uniform vec3 u_SunDirection;  // normalized world-space ray travel direction
uniform vec3 u_SunColor;      // linear RGB
uniform float u_SunIntensity;
uniform float u_AmbientStrength;
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
    if (u_LightingEnabled) {
        float diffuse = 0.0;
        // Scale before length to avoid overflow; invalid or zero normals receive ambient only.
        if (!any(isnan(v_WorldNormal)) && !any(isinf(v_WorldNormal))) {
            vec3 absoluteNormal = abs(v_WorldNormal);
            float largestComponent = max(max(absoluteNormal.x, absoluteNormal.y), absoluteNormal.z);
            if (largestComponent > 0.0) {
                vec3 scaledNormal = v_WorldNormal / largestComponent;
                float squaredLength = dot(scaledNormal, scaledNormal);
                if (squaredLength > 0.0 && !isnan(squaredLength) && !isinf(squaredLength)) {
                    vec3 normal = scaledNormal * inversesqrt(squaredLength);
                    diffuse = max(dot(normal, -u_SunDirection), 0.0);
                }
            }
        }
        color.rgb *= vec3(u_AmbientStrength) + u_SunColor * u_SunIntensity * diffuse;
    }
    FragColor = color;
}
