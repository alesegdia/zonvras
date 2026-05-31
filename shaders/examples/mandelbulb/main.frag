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
    vec4  camRot;
} pc;

layout(location = 0) in  vec2 fragUV;
layout(location = 0) out vec4 outColor;

// ============================================================
//  Helpers
// ============================================================
vec3 quatRotate(vec4 q, vec3 v) {
    vec3  u = q.yzw;
    float s = q.x;
    return 2.0 * dot(u, v) * u
         + (s*s - dot(u, u)) * v
         + 2.0 * s * cross(u, v);
}

// ============================================================
//  Mandelbulb distance estimator  (power 8)
//  Returns the distance to the surface.
//  orbitTrap accumulates a value used for coloring.
// ============================================================
float mandelbulbDE(vec3 pos, out float orbitTrap) {
    const float POWER = 8.0;
    const int   ITER  = 12;

    vec3  z  = pos;
    float dr = 1.0;
    float r  = 0.0;
    orbitTrap = 1e10;

    for (int i = 0; i < ITER; i++) {
        r = length(z);
        if (r > 2.0) break;

        // Orbit trap: distance from the XZ plane
        orbitTrap = min(orbitTrap, abs(z.y));

        // Convert to spherical, scale, rotate, convert back
        float theta = acos(clamp(z.z / r, -1.0, 1.0));
        float phi   = atan(z.y, z.x);
        dr = pow(r, POWER - 1.0) * POWER * dr + 1.0;

        float zr = pow(r, POWER);
        theta *= POWER;
        phi   *= POWER;

        z = zr * vec3(sin(theta)*cos(phi),
                      sin(theta)*sin(phi),
                      cos(theta)) + pos;
    }
    return 0.5 * log(r) * r / dr;
}

// ============================================================
//  Scene wrapper  (slow auto-rotation so it looks good from
//  the default camera position)
// ============================================================
float scene(vec3 p, out float trap) {
    // Gentle Y rotation over time
    float a = pc.time * 0.12;
    float c = cos(a), s = sin(a);
    vec3  q = vec3(c*p.x + s*p.z, p.y, -s*p.x + c*p.z);
    return mandelbulbDE(q, trap);
}

// ============================================================
//  Normal via tetrahedron finite differences (4 samples vs 6)
// ============================================================
vec3 calcNormal(vec3 p) {
    float t0;
    const float h = 0.0005;
    const vec2  k = vec2(1.0, -1.0);
    return normalize(
        k.xyy * scene(p + k.xyy*h, t0) +
        k.yyx * scene(p + k.yyx*h, t0) +
        k.yxy * scene(p + k.yxy*h, t0) +
        k.xxx * scene(p + k.xxx*h, t0)
    );
}

// ============================================================
//  Ambient occlusion
// ============================================================
float calcAO(vec3 p, vec3 n) {
    float occ  = 0.0;
    float scale = 1.0;
    float t0;
    for (int i = 0; i < 5; i++) {
        float h = 0.02 + 0.06 * float(i);
        float d = scene(p + h * n, t0);
        occ  += (h - d) * scale;
        scale *= 0.7;
    }
    return clamp(1.0 - 2.0 * occ, 0.0, 1.0);
}

// ============================================================
//  Raymarcher  (conservative step for mandelbulb DE)
// ============================================================
float raymarch(vec3 ro, vec3 rd, out float trap) {
    float t    = 0.0;
    trap = 1e10;
    for (int i = 0; i < 200; i++) {
        float localTrap;
        float d = scene(ro + rd * t, localTrap);
        trap = min(trap, localTrap);
        if (d < 0.0003 * t) return t;
        if (t > 15.0)       break;
        t += d * 0.55;  // conservative fraction keeps us stable
    }
    return -1.0;
}

// ============================================================
//  Coloring based on orbit trap
// ============================================================
vec3 bulbColor(float trap) {
    // Remap trap to [0,1]
    float t = clamp(sqrt(trap) * 1.5, 0.0, 1.0);
    // Deep purple → hot orange → bright cyan
    vec3 a = vec3(0.08, 0.02, 0.25);
    vec3 b = vec3(0.9,  0.45, 0.05);
    vec3 c = vec3(0.1,  0.85, 0.9 );
    vec3 col = (t < 0.5)
        ? mix(a, b, t * 2.0)
        : mix(b, c, (t - 0.5) * 2.0);
    return col;
}

// ============================================================
//  Background
// ============================================================
vec3 background(vec3 rd) {
    // Subtle starfield + gradient
    vec3 sky1 = vec3(0.005, 0.005, 0.015);
    vec3 sky2 = vec3(0.02, 0.01, 0.04);
    float g = rd.y * 0.5 + 0.5;
    vec3 col = mix(sky1, sky2, g);

    // Hash-based star dots
    vec2  uv  = vec2(atan(rd.z, rd.x), asin(rd.y)) * (1.0 / 3.14159);
    float star = step(0.997, fract(sin(dot(floor(uv * 200.0),
                        vec2(127.1, 311.7))) * 43758.5));
    col += star * 0.6;
    return col;
}

// ============================================================
//  Main
// ============================================================
void main() {
    vec2  ndc = fragUV * 2.0 - 1.0;
    ndc.y    = -ndc.y;
    float aspect    = pc.resolution.x / pc.resolution.y;
    float fovFactor = tan(radians(60.0) * 0.5);

    vec3 rd_local = normalize(vec3(ndc.x * aspect * fovFactor,
                                   ndc.y * fovFactor,
                                   -1.0));
    vec3 ro = pc.camPos;
    vec3 rd = quatRotate(pc.camRot, rd_local);

    float trap;
    float t = raymarch(ro, rd, trap);

    vec3 col;
    if (t > 0.0) {
        vec3  pos  = ro + rd * t;
        vec3  n    = calcNormal(pos);
        float ao   = calcAO(pos, n);

        // Key light
        vec3  ld   = normalize(vec3(1.5, 2.0, 1.0));
        float diff = max(dot(n, ld), 0.0);
        float spec = pow(max(dot(reflect(-ld, n), -rd), 0.0), 40.0);

        // Fill / rim
        float rim  = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        vec3 base = bulbColor(trap);
        col  = base * (0.05 + 0.9 * diff) * ao;
        col += vec3(1.0, 0.9, 0.7) * spec * 0.4 * ao;
        col += vec3(0.3, 0.5, 1.0) * rim  * 0.25;

        // Depth fog toward background
        float fog = 1.0 - exp(-t * 0.12);
        col = mix(col, background(rd), fog);

        // Glow at surface edge (sub-surface scatter fake)
        float glow = clamp(1.0 - length(pos) * 0.3, 0.0, 1.0);
        col += vec3(0.4, 0.1, 0.6) * glow * 0.15;
    } else {
        col = background(rd);
    }

    col = pow(clamp(col, 0.0, 1.0), vec3(1.0/2.2));
    outColor = vec4(col, 1.0);
}
