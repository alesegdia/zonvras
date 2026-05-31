#version 450

// ============================================================
//  Uniforms
// ============================================================
layout(push_constant) uniform PC {
    float time;
    float pad0;
    float pad1;
    float pad2;
    vec2  resolution;
    vec2  pad3;
    vec3  camPos;
    float pad4;
    vec4  camRot; // quaternion (w, x, y, z)
} pc;

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outColor;

// ============================================================
//  Math helpers
// ============================================================
vec3 quatRotate(vec4 q, vec3 v) {
    vec3 u  = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u
         + (s*s - dot(u, u)) * v
         + 2.0 * s * cross(u, v);
}

// ============================================================
//  SDF library
// ============================================================
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdFloor(vec3 p) {
    return p.y + 1.0;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b, a, h) - k*h*(1.0-h);
}

// ============================================================
//  Scene
// ============================================================
float scene(vec3 p) {
    float sphere1 = sdSphere(p - vec3(0.0, 0.0, 0.0), 0.8);
    float sphere2 = sdSphere(p - vec3(sin(pc.time)*1.5, 0.3, cos(pc.time)*1.5), 0.5);
    float floor_  = sdFloor(p);

    float d = smin(sphere1, sphere2, 0.4);
    d       = min(d, floor_);
    return d;
}

// ============================================================
//  Normal estimation
// ============================================================
vec3 calcNormal(vec3 p) {
    const float h = 0.001;
    return normalize(vec3(
        scene(p + vec3(h, 0, 0)) - scene(p - vec3(h, 0, 0)),
        scene(p + vec3(0, h, 0)) - scene(p - vec3(0, h, 0)),
        scene(p + vec3(0, 0, h)) - scene(p - vec3(0, 0, h))
    ));
}

// ============================================================
//  Raymarcher
// ============================================================
float raymarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    for (int i = 0; i < 96; i++) {
        float d = scene(ro + rd * t);
        if (d < 0.001) return t;
        if (t > 50.0)  break;
        t += d;
    }
    return -1.0;
}

// ============================================================
//  Lighting
// ============================================================
vec3 lighting(vec3 pos, vec3 rd) {
    vec3 normal    = calcNormal(pos);
    vec3 lightDir  = normalize(vec3(0.6, 1.0, 0.4));
    vec3 lightColor = vec3(1.0, 0.95, 0.8);

    float diff = max(dot(normal, lightDir), 0.0);

    // Soft shadows
    float shadow = 1.0;
    {
        float t = 0.02;
        for (int i = 0; i < 16; i++) {
            float d = scene(pos + lightDir * t);
            shadow  = min(shadow, 8.0*d/t);
            t      += 0.05;
            if (t > 2.0) break;
        }
        shadow = clamp(shadow, 0.0, 1.0);
    }

    vec3 halfDir = normalize(lightDir - rd);
    float spec   = pow(max(dot(normal, halfDir), 0.0), 64.0);

    vec3 ambient = vec3(0.05, 0.07, 0.12);
    vec3 color   = ambient
                 + diff * shadow * lightColor
                 + spec * shadow * vec3(1.0);

    float fog     = 1.0 - exp(-length(pos - pc.camPos) * 0.04);
    color = mix(color, vec3(0.12, 0.15, 0.22), fog);
    return color;
}

// ============================================================
//  Sky
// ============================================================
vec3 sky(vec3 rd) {
    float t = 0.5 * (rd.y + 1.0);
    return mix(vec3(0.3, 0.35, 0.45), vec3(0.05, 0.07, 0.15), t);
}

// ============================================================
//  Main
// ============================================================
void main() {
    vec2  ndc = fragUV * 2.0 - 1.0;
    ndc.y    = -ndc.y;
    float aspect    = pc.resolution.x / pc.resolution.y;
    float fovFactor = tan(radians(70.0) * 0.5);

    vec3 rd_local = normalize(vec3(ndc.x * aspect * fovFactor,
                                   ndc.y * fovFactor,
                                   -1.0));

    vec3 ro = pc.camPos;
    vec3 rd = quatRotate(pc.camRot, rd_local);

    float t   = raymarch(ro, rd);
    vec3  col = (t > 0.0) ? lighting(ro + rd * t, rd) : sky(rd);

    col = pow(clamp(col, 0.0, 1.0), vec3(1.0/2.2));
    outColor = vec4(col, 1.0);
}
