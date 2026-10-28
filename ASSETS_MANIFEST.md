// VAT External Asset Manifest
// Hybrid Animation System - External Asset Generation Plan
// ---------------------------------------------------------------------
// This document defines the exact VAT encoding contract that must be
// followed by the external VAT generator tool.
// ---------------------------------------------------------------------

# VAT External Asset Generation Plan

## Overview
This manifest defines the exact file paths, dimensions, and encoding format
required for VAT (Vertex Animation Texture) assets used by the hybrid
animation system.

## Encoding Contract

### VAT Texture Format
- **Width**: `vertex_count` (number of vertices in the static mesh)
- **Height**: `frame_count` (number of animation frames)
- **Pixel Format**: RGB8 (each channel = 0-255)
- **Encoding**: Absolute vertex positions in range [0, 1]
- **Decoding**: `position = vatPositionMin + rgb * vatPositionExtent`

### UV Coordinate Mapping
- **X coordinate**: Normalized vertex ID
  - Formula: `x = (vertex_id + 0.5) / vertex_count`
  - Range: [0, 1]
- **Y coordinate**: Normalized frame index
  - Formula: `y = (frame + 0.5) / frame_count`
  - Range: [0, 1]

### Frame Clamping
- Frames are clamped to valid range: `[0, vatFrameCount - 1]`
- No frame interpolation in this first version

### Filtering
- Point filtering must be used (no interpolation between texels)

## Asset Paths

### Goat NPC Archetype (First Integration)

| File Path | Type | Description | Dimensions | Notes |
|-----------|------|-------------|------------|-------|
| `assets/models/animales/cabra_static.glb` | Static Mesh | Goat base mesh without animation | - | Must have UV coordinates for vertex ID |
| `assets/models/animales/cabra_vat.png` | VAT Texture | Vertex positions per frame | `W x H` | W = vertex_count, H = frame_count |
| `assets/models/animales/cabra.anim` | Animation Clip | Animation metadata | - | For external generator reference |

### Example: Goat with 800 vertices, 24 frames
- **Texture dimensions**: 800 x 24 pixels
- **Vertex ID in UV**: `x = (vertex_id + 0.5) / 800`
- **Frame in UV**: `y = (frame + 0.5) / 24`

## GLSL 330 Shader (Desktop)
- **Path**: `assets/shaders/glsl330/vat.vs`
- **Features**: `in`, `out`, `texture()` API
- **Platform**: OpenGL 3.3 / Desktop

## GLSL ES 100 Shader (Android)
- **Path**: `assets/shaders/glsl100/vat.vs`
- **Features**: `attribute`, `varying`, `texture2D` API
- **Platform**: OpenGL ES 2.0 / Android

## Uniforms

### Vertex Shader Uniforms
- `matModel`: Mesh transformation matrix
- `matView`: View matrix
- `matProjection`: Projection matrix
- `vatPositionMin`: Decoded position minimum (vec3)
- `vatPositionExtent`: Decoded position extent (vec3)
- `vatFrame`: Current frame (float)
- `vatFrameCount`: Total frame count (int)
- `vatVertexCount`: Total vertex count (int)

### Fragment Shader Uniforms
- `matDiffuseColor`: Material diffuse color (vec3)
- `texture0`: VAT texture sampler
- `lightDir`: Directional light direction (vec3)
- `lightColor`: Light color (vec3)
- `ambientColor`: Ambient light color (vec3)
- `ambientIntensity`: Ambient intensity (float)

## Integration Notes

### For the Goat NPC (First Integration)
1. Load static mesh: `assets/models/animales/cabra_static.glb`
2. Load VAT texture: `assets/models/animales/cabra_vat.png`
3. Set `vatFrameCount` from texture height
4. Set `vatVertexCount` from mesh vertex count
5. Pass `vatPositionMin = (0,0,0)` and `vatPositionExtent = (1,1,1)`
6. Set current frame uniform each frame

### For Other NPCs, Animals, Environmental Elements
- Each archetype will have its own static mesh and VAT texture
- Use the same encoding contract
- Each VAT texture represents one static mesh and one animation clip

## External Generator Requirements

The external VAT generator tool must:
1. Load the static mesh (GLB/OBJ) with UV coordinates
2. Extract vertex positions for each animation frame
3. Encode positions as RGB8 in range [0, 1]
4. Write texture with dimensions: `vertex_count x frame_count`
5. Generate metadata file with:
   - `vertex_count`
   - `frame_count`
   - `fps`
   - `model_path`
   - `animation_name`

## Validation

To validate VAT assets:
1. Load static mesh and verify UV coordinates exist
2. Load VAT texture and verify dimensions
3. Verify texture pixel format (RGB8)
4. Test rendering with shader (requires actual assets to be valid)

## Notes
- All VAT textures must be placed in `assets/models/` subdirectories
- Static meshes must use UV coordinates to carry vertex IDs
- GLSL ES 100 shaders must not use `gl_VertexID` (not available)
- Vertex ID is carried via UV.x attribute
