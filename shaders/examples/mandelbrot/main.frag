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
//  Zoom target — Seahorse Valley, a classic deep-zoom site
//  with self-similar spiralling structure at every scale.
// ============================================================
const vec2  TARGET     = vec2(-0.7435669, 0.1314023);
const float ZOOM_SPEED = 0.22;   // e-foldings per second
const float ZOOM_MAX   = 13.0;   // loop at ~440 000× (float32 precision limit ~1e7)
const int   MAX_ITER   = 350;
const float BAILOUT2   = 65536.0; // |z|^2 > 256^2

// ============================================================
//  Cosine colour palette  (Inigo Quilez style)
// ============================================================
vec3 palette(float t) {
    const vec3 a = vec3(0.50, 0.50, 0.50);
    const vec3 b = vec3(0.50, 0.50, 0.50);
    const vec3 c = vec3(1.00, 0.70, 0.40);
    const vec3 d = vec3(0.00, 0.15, 0.20);
    return a + b * cos(6.28318 * (c * t + d));
}

// ============================================================
//  Main
// ============================================================
void main() {
    vec2  res    = pc.resolution;
    float aspect = res.x / res.y;

    // Exponential zoom, loops every ZOOM_MAX e-foldings
    float logZoom = mod(pc.time * ZOOM_SPEED, ZOOM_MAX);
    float zoom    = exp(logZoom);
    float scale   = 2.0 / zoom;

    // Map fragment to Mandelbrot plane centered on TARGET
    vec2 uv = (fragUV * 2.0 - 1.0) * vec2(aspect, 1.0);
    vec2 c  = TARGET + uv * scale;

    // Mandelbrot iteration
    vec2  z = vec2(0.0);
    float n = 0.0;
    for (int i = 0; i < MAX_ITER; i++) {
        z = vec2(z.x*z.x - z.y*z.y + c.x,
                 2.0*z.x*z.y       + c.y);
        if (dot(z, z) > BAILOUT2) break;
        n += 1.0;
    }

    vec3 color;
    if (n >= float(MAX_ITER)) {
        // Interior of the set — black
        color = vec3(0.0);
    } else {
        // Smooth (continuous) iteration count.
        // Derived from: mu = n - log2(log2(|z|)) + log2(log2(esc_r))
        // With esc_r = 256:  log2(log2(256)) = log2(8) = 3, and
        // log2(|z|) = 0.5 * log2(|z|^2), so the offset becomes +4.
        float sl = n - log2(log2(dot(z, z))) + 4.0;

        // t cycles colour bands; logZoom drifts the hue gently as we zoom
        float t = sl * 0.012 + logZoom * 0.04;
        color = palette(t);

        // Subtle brightness ripple for extra depth cue
        color *= 0.82 + 0.18 * cos(sl * 0.7);
    }

    outColor = vec4(color, 1.0);
}
