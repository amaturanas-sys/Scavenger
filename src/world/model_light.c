#include "model_light.h"

#include <math.h>

#include "raymath.h"
#include "rlgl.h"

// La luz horneada de los cuerpos y los animales (tools/modelos/humanoide.py y animales.py): 0,72 + 0,38 por el coseno
// de la cara con el sol, el de todo lo horneado (src/world/terrain.c, tools/modelos/lowres.py: SUN).
#define BAKE_AMBIENT 0.72f
#define BAKE_DIFFUSE 0.38f

// El contraste: lo que mira al sol, a 1,1 (como lo horneado); lo que no, a la luz del cielo y del suelo, de 0,4 abajo a
// 0,65 arriba (lo horneado no bajaba de 0,72: los cuerpos se veian planos).
static const Vector3 SKY = { 0.64f, 0.66f, 0.70f };
static const Vector3 GROUND = { 0.40f, 0.36f, 0.32f };
static const Vector3 SUN_COLOR = { 0.50f, 0.48f, 0.42f };

// Por vertice (el sombreado de Gouraud: barato en Android), con la normal de la pose de este cuadro (raylib la mueve
// con la piel: UpdateModelAnimation).
#define LIGHT_GLSL_VS \
    "ATTR vec3 vertexPosition;\n" \
    "ATTR vec2 vertexTexCoord;\n" \
    "ATTR vec3 vertexNormal;\n" \
    "ATTR vec4 vertexColor;\n" \
    "uniform mat4 mvp;\n" \
    "uniform mat4 matNormal;\n" \
    "uniform vec3 uSun;\n" /* hacia el sol */ \
    "uniform vec3 uSky;\n" /* la luz de lo que mira arriba */ \
    "uniform vec3 uGround;\n" /* la de lo que mira abajo */ \
    "uniform vec3 uSunColor;\n" \
    "VARY vec2 fragTexCoord;\n" \
    "VARY vec4 fragColor;\n" \
    "void main() {\n" \
    "    vec3 n = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);\n" \
    "    vec3 light = mix(uGround, uSky, 0.5 + 0.5 * n.y) + uSunColor * max(dot(n, uSun), 0.0);\n" \
    "    fragColor = vec4(vertexColor.rgb * light, vertexColor.a);\n" \
    "    fragTexCoord = vertexTexCoord;\n" \
    "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n" \
    "}\n"
#define LIGHT_GLSL_FS \
    "uniform sampler2D texture0;\n" \
    "uniform vec4 colDiffuse;\n" \
    "VARY vec2 fragTexCoord;\n" \
    "VARY vec4 fragColor;\n" \
    "void main() {\n" \
    "    FRAG = TEX(texture0, fragTexCoord) * colDiffuse * fragColor;\n" \
    "}\n"
static const char LIGHT_VS_330[] = "#version 330\n#define ATTR in\n#define VARY out\n" LIGHT_GLSL_VS;
static const char LIGHT_FS_330[] = "#version 330\n#define VARY in\n#define TEX texture\n#define FRAG finalColor\nout vec4 finalColor;\n" LIGHT_GLSL_FS;
// En GLSL ES el vertice va en highp, como el del agua y el de raylib (CMakeLists.txt).
static const char LIGHT_VS_100[] = "#version 100\nprecision highp float;\n#define ATTR attribute\n#define VARY varying\n" LIGHT_GLSL_VS;
static const char LIGHT_FS_100[] = "#version 100\n#ifdef GL_FRAGMENT_PRECISION_HIGH\nprecision highp float;\n#else\nprecision mediump float;\n#endif\n"
                                   "#define VARY varying\n#define TEX texture2D\n#define FRAG gl_FragColor\n" LIGHT_GLSL_FS;

static Shader g_shader;
static bool g_ready;

static Vector3 sun_dir(void) { return Vector3Normalize((Vector3){ -0.45f, 1.0f, -0.3f }); }

void model_light_init(void) {
    if (g_ready) return;
    int v = rlGetVersion();
    bool es = v == RL_OPENGL_ES_20 || v == RL_OPENGL_ES_30;
    if (!es && v != RL_OPENGL_33 && v != RL_OPENGL_43) return; // OpenGL 1.1 o 2.1: sin shader
    Shader sh = LoadShaderFromMemory(es ? LIGHT_VS_100 : LIGHT_VS_330, es ? LIGHT_FS_100 : LIGHT_FS_330);
    if (!IsShaderValid(sh) || sh.id == rlGetShaderIdDefault()) {
        TraceLog(LOG_WARNING, "ESTEPA: el shader de la luz de los modelos no compilo: la luz horneada");
        return;
    }
    Vector3 sun = sun_dir();
    SetShaderValue(sh, GetShaderLocation(sh, "uSun"), &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "uSky"), &SKY, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "uGround"), &GROUND, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, GetShaderLocation(sh, "uSunColor"), &SUN_COLOR, SHADER_UNIFORM_VEC3);
    g_shader = sh;
    g_ready = true;
}

void model_light_unload(void) {
    if (g_ready) UnloadShader(g_shader);
    g_ready = false;
}

bool model_light_ready(void) { return g_ready; }

// El color de cada vertice sin la luz horneada: la de su cara (la normal, de sus tres posiciones: la malla va cara por
// cara, sin indices) o, con indices, la del vertice. Una sola vez, al cargar.
static void unbake(Mesh *me) {
    if (!me->colors || me->vertexCount <= 0) return;
    Vector3 sun = sun_dir();
    for (int v = 0; v < me->vertexCount; v++) {
        Vector3 n;
        if (!me->indices && me->vertexCount % 3 == 0) {
            int f = v - v % 3;
            const float *p = &me->vertices[f * 3];
            Vector3 a = { p[0], p[1], p[2] }, b = { p[3], p[4], p[5] }, c = { p[6], p[7], p[8] };
            n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
        } else if (me->normals) {
            n = (Vector3){ me->normals[v * 3], me->normals[v * 3 + 1], me->normals[v * 3 + 2] };
        } else {
            continue;
        }
        float bake = BAKE_AMBIENT + BAKE_DIFFUSE * fmaxf(0.0f, Vector3DotProduct(n, sun));
        unsigned char *c = &me->colors[v * 4];
        for (int k = 0; k < 3; k++) c[k] = (unsigned char)fminf(255.0f, (float)c[k] / bake + 0.5f);
    }
    UpdateMeshBuffer(*me, 3, me->colors, me->vertexCount * 4, 0);
}

void model_light_apply(Model *m) {
    if (!g_ready || !m) return;
    for (int i = 0; i < m->meshCount; i++) unbake(&m->meshes[i]);
    for (int i = 0; i < m->materialCount; i++) m->materials[i].shader = g_shader;
}
