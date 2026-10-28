// VAT Vertex Shader - GLSL 330 (Desktop)
// Hybrid Animation System: Bone Rig + Vertex Animation Textures (VAT)
// ---------------------------------------------------------------------
// VAT encoding contract:
// - Texture width = vertex_count
// - Texture height = frame_count
// - x = (vertex_id + 0.5) / vertex_count (normalized vertex ID in X)
// - y = (frame + 0.5) / frame_count (normalized frame in Y)
// - Each texel stores RGB8 position encoded in range [0, 1]
// - Decode: position = vatPositionMin + rgb * vatPositionExtent
// - Use point filtering, no interpolation
// - Frame clamped to [0, vatFrameCount - 1]
// - Vertex ID carried via UV attribute (convention: UV.x = normalized vertex ID)
// ---------------------------------------------------------------------

#version 330 core

// Input attributes
attribute vec3 attrPosition;      // Original mesh position
attribute vec2 attrTexCoord;      // UV coordinates (UV.x = normalized vertex ID)

// Output to fragment shader
varying vec2 vTexCoord;
varying vec3 vWorldPos;

// VAT uniforms
uniform mat4 matModel;            // Mesh transform (local to world)
uniform mat4 matView;             // View matrix
uniform mat4 matProjection;       // Projection matrix
uniform vec3 vatPositionMin;      // Decoded position minimum
uniform vec3 vatPositionExtent;   // Decoded position extent (range size)
uniform float vatFrame;           // Current frame (0 to vatFrameCount-1)
uniform int vatFrameCount;        // Total frame count
uniform int vatVertexCount;       // Total vertex count

// Vertex ID from UV.x (normalized to [0, vertex_count))
// UV.x = (vertex_id + 0.5) / vertex_count
// vertex_id = floor(UV.x * vertex_count)
int getVertexID(vec2 uv) {
    float v = uv.x * float(vatVertexCount);
    return int(floor(v));
}

// Frame from UV.y (normalized to [0, frame_count))
// UV.y = (frame + 0.5) / frame_count
// frame = floor(UV.y * frame_count)
int getFrame(vec2 uv) {
    float f = uv.y * float(vatFrameCount);
    return int(floor(f));
}

// Clamp frame to valid range
int clampFrame(int frame) {
    return max(0, min(frame, vatFrameCount - 1));
}

void main() {
    // Get vertex ID and frame from UV coordinates
    int vid = getVertexID(attrTexCoord);
    int frame = clampFrame(getFrame(attrTexCoord));
    
    // Calculate normalized position from VAT texture
    // position = vatPositionMin + rgb * vatPositionExtent
    // RGB is stored as normalized float in the texture
    vec3 vatPos = vatPositionMin + attrTexCoord * vatPositionExtent;
    
    // Apply mesh transform
    vec4 worldPos = matModel * vec4(vatPos, 1.0);
    
    // Apply view and projection
    gl_Position = matProjection * matView * worldPos;
    
    // Pass UV and world position to fragment
    vTexCoord = attrTexCoord;
    vWorldPos = worldPos.xyz;
}
