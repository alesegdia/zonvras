# Push Constants Layout

The `PushConstants` struct in [src/shared_types.h](../src/shared_types.h) must match the
`layout(push_constant)` block in every fragment shader byte-for-byte. The explicit padding
fields exist to satisfy **GLSL `std430` alignment rules**, which the Vulkan driver enforces
when mapping the CPU memory block to shader registers.

## Alignment rules (std430)

| GLSL type | Base alignment |
|-----------|---------------|
| `float`   | 4 bytes |
| `vec2`    | 8 bytes |
| `vec3`    | **16 bytes** (same as `vec4`) |
| `vec4`    | 16 bytes |

Every field must start at a byte offset that is a multiple of its base alignment.

## Field-by-field layout

```
offset  0  │ time       float  (4 B)
        4  │ pad0       float  (4 B)  ┐
        8  │ pad1       float  (4 B)  │ filler so that resolution (align-8) can sit at
       12  │ pad2       float  (4 B)  │ offset 16 — keeping camPos (align-16) at 32
       16  │ resolution vec2   (8 B)  ┘
       24  │ pad3       vec2   (8 B)    filler so camPos lands on a 16-byte boundary (32)
       32  │ camPos     vec3  (12 B)
       44  │ pad4       float  (4 B)    filler so camRot lands on a 16-byte boundary (48)
       48  │ camRot     vec4  (16 B)
       ────┴───────────────────────────
total: 64 bytes
```

The most surprising rule is that `vec3` aligns to **16 bytes**, not 12. It has the same
base alignment as `vec4`. Without `pad4`, `camRot` would start at offset 44 (not a
multiple of 16), and the GPU would read it as garbage.

## Keeping CPU and GPU in sync

Both [src/shared_types.h](../src/shared_types.h) and the `layout(push_constant)` block at
the top of each fragment shader (e.g. [shaders/examples/default/main.frag](../shaders/examples/default/main.frag))
declare the identical fields in the identical order. If you add a new uniform:

1. Add it (plus any required padding) to the C++ struct.
2. Add the same field(s) to the GLSL block in **every** shader that uses it.
3. Verify the total struct size stays within the device's
   `maxPushConstantsSize` limit (guaranteed ≥ 128 bytes by the Vulkan spec).
