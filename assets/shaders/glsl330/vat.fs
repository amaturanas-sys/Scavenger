// VAT Fragment Shader - GLSL 330 (Desktop)
// Hybrid Animation System: Bone Rig + Vertex Animation Textures (VAT)
// ---------------------------------------------------------------------
// Reads vertex position from VAT texture and applies material/color.
// ---------------------------------------------------------------------

#version 330 core

// Input from vertex shader
varying vec2 vTexCoord;
varying vec3 vWorldPos;

// Material uniforms (from raylib)
uniform vec3 matDiffuseColor;
uniform vec3 matSpecularColor;
uniform float matShininess;
uniform int matMapDiffuse;        // Texture map index (0 = none)
uniform sampler2D texture0;       // Default texture sampler

// VAT uniforms
uniform vec3 vatPositionMin;      // Decoded position minimum
uniform vec3 vatPositionExtent;   // Decoded position extent
uniform int vatVertexCount;       // Total vertex count
uniform int vatFrameCount;        // Total frame count

// Lighting (simplified directional light)
uniform vec3 lightDir;            // Directional light direction (normalized)
uniform vec3 lightColor;          // Light color

// Ambient light
uniform vec3 ambientColor;
uniform float ambientIntensity;

void main() {
    // Sample texture (if any)
    vec4 texColor = texture(texture0, vTexCoord);
    
    // Combine with material color
    vec3 finalColor = matDiffuseColor * texColor.rgb;
    
    // Simple directional lighting
    float diff = max(dot(normalize(vWorldPos - vec3(0.0, 10.0, 0.0)), lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Apply ambient
    vec3 ambient = ambientIntensity * ambientColor;
    
    // Final color with lighting
    vec3 litColor = ambient + diffuse * finalColor;
    
    // Output with alpha from texture (if textured) or material
    float alpha = texColor.a * matDiffuseColor.a;
    gl_FragColor = vec4(litColor, alpha);
}
