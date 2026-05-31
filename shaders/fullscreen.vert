#version 450

// Generate a fullscreen triangle with no vertex buffer.
// Indices 0,1,2 cover the entire clip-space when mapped:
//   0 -> (-1, -1)   bottom-left
//   1 -> ( 3, -1)   far right (off-screen)
//   2 -> (-1,  3)   far top   (off-screen)
// The triangle covers every pixel of the viewport.

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

layout(location = 0) out vec2 fragUV;

void main() {
    // UV in [0,1]^2, origin bottom-left in OpenGL convention
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    fragUV   = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
