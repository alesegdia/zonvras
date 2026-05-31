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
// Rotate vector v by quaternion q (w, x, y, z)
vec3 quatRotate(vec4 q, vec3 v) {
    vec3 u   = q.yzw;          // imaginary part
    float s  = q.x;            // real part
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

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdFloor(vec3 p) {
    return p.y + 1.0;
}

// Smooth minimum — blends two surfaces
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
//  Normal estimation via central differences
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
    return -1.0; // miss
}

// ============================================================
//  Lighting
// ============================================================
vec3 lighting(vec3 pos, vec3 rd) {
    vec3 normal    = calcNormal(pos);
    vec3 lightDir  = normalize(vec3(0.6, 1.0, 0.4));
    vec3 lightColor = vec3(1.0, 0.95, 0.8);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);

    // Ambient occlusion approximation via secondary march
    float shadow = 1.0;
    {
        float t = 0.02;
        for (int i = 0; i < 16; i++) {
            float d = scene(pos + lightDir * t);
            shadow = min(shadow, 8.0*d/t);
            t += 0.05;
            if (t > 2.0) break;
        }
        shadow = clamp(shadow, 0.0, 1.0);
    }

    // Specular (Blinn-Phong)
    vec3 halfDir = normalize(lightDir - rd);
    float spec   = pow(max(dot(normal, halfDir), 0.0), 64.0);

    // Simple sky color for ambient
    vec3 ambient = vec3(0.05, 0.07, 0.12);

    vec3 color = ambient
               + diff * shadow * lightColor
               + spec * shadow * vec3(1.0);

    // Simple fog
    float fogDist = length(pos - pc.camPos);
    float fog     = 1.0 - exp(-fogDist * 0.04);
    vec3  fogColor = vec3(0.12, 0.15, 0.22);
    color = mix(color, fogColor, fog);

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
    // Reconstruct screen-space ray direction
    vec2  uv  = fragUV;
    vec2  ndc = uv * 2.0 - 1.0;
    ndc.y    = -ndc.y; // flip Y (Vulkan clip-space)
    float aspect = pc.resolution.x / pc.resolution.y;
    float fov    = radians(70.0);
    float fovFactor = tan(fov * 0.5);

    // Local camera space ray
    vec3 rd_local = normalize(vec3(ndc.x * aspect * fovFactor,
                                   ndc.y * fovFactor,
                                   -1.0));

    // Transform ray into world space using camera quaternion
    vec3 ro = pc.camPos;
    vec3 rd = quatRotate(pc.camRot, rd_local);

    float t = raymarch(ro, rd);
    vec3  col;
    if (t > 0.0) {
        col = lighting(ro + rd * t, rd);
    } else {
        col = sky(rd);
    }

    // Gamma correction (linear -> sRGB)
    col = pow(clamp(col, 0.0, 1.0), vec3(1.0/2.2));

    outColor = vec4(col, 1.0);
}
