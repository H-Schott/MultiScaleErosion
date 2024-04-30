#version 450 core

#ifdef COMPUTE_SHADER

// terrain
layout(binding = 0, std430) readonly  buffer InTerrain  { float in_terrain[]; };
layout(binding = 1, std430) writeonly buffer OutTerrain { float out_terrain[]; };

// stream
layout(binding = 2, std430) readonly  buffer InStream   { float in_stream[]; };
layout(binding = 3, std430) writeonly buffer OutStream  { float out_stream[]; };

// hardness
layout(binding = 4, std430) readonly  buffer Hardness   { float in_hardness[]; };


uniform int nx;
uniform int ny;
uniform vec2 a;
uniform vec2 b;
uniform vec2 cellDiag;

uniform float flow_p = 1.3f;

uniform float k = 0.0005f;
uniform float p_sa = 0.8f;
uniform float p_sl = 2.f;
uniform float dt = 1.f;
uniform float max_spe = 10000.;

const ivec2 next8[8] = ivec2[8](
    ivec2(0, 1), ivec2(1, 1), ivec2(1, 0), ivec2(1, -1),
    ivec2(0, -1), ivec2(-1, -1), ivec2(-1, 0), ivec2(-1, 1)
    );

float random(vec2 st) { return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123); }
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x * 34.0) + 1.0) * x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
float snoise(vec3 v) {
    const vec2  C = vec2(1.0 / 6.0, 1.0 / 3.0);  const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);
    vec3 i = floor(v + dot(v, C.yyy));  vec3 x0 = v - i + dot(i, C.xxx);  vec3 g = step(x0.yzx, x0.xyz);  vec3 l = 1.0 - g;  vec3 i1 = min(g.xyz, l.zxy);  vec3 i2 = max(g.xyz, l.zxy);
    vec3 x1 = x0 - i1 + C.xxx;  vec3 x2 = x0 - i2 + C.yyy;   vec3 x3 = x0 - D.yyy;
    i = mod289(i);  vec4 p = permute(permute(permute(i.z + vec4(0.0, i1.z, i2.z, 1.0)) + i.y + vec4(0.0, i1.y, i2.y, 1.0)) + i.x + vec4(0.0, i1.x, i2.x, 1.0));
    float n_ = 0.142857142857;   vec3  ns = n_ * D.wyz - D.xzx;  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)
    vec4 x_ = floor(j * ns.z);  vec4 y_ = floor(j - 7.0 * x_);      vec4 x = x_ * ns.x + ns.yyyy;  vec4 y = y_ * ns.x + ns.yyyy;  vec4 h = 1.0 - abs(x) - abs(y);
    vec4 b0 = vec4(x.xy, y.xy);  vec4 b1 = vec4(x.zw, y.zw);  vec4 s0 = floor(b0) * 2.0 + 1.0;  vec4 s1 = floor(b1) * 2.0 + 1.0;  vec4 sh = -step(h, vec4(0.0));
    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;  vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;  vec3 p0 = vec3(a0.xy, h.x);  vec3 p1 = vec3(a0.zw, h.y);  vec3 p2 = vec3(a1.xy, h.z);  vec3 p3 = vec3(a1.zw, h.w);
    vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));   p0 *= norm.x;  p1 *= norm.y;  p2 *= norm.z;  p3 *= norm.w;
    vec4 m = max(0.6 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);  m = m * m;
    return 42.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}


int ToIndex1D(int i, int j) {
    return i + nx * j;
}

int ToIndex1D(ivec2 p) {
    return p.x + nx * p.y;
}

vec2 ArrayPoint(ivec2 p) {
    return a.xy + vec2(p) * cellDiag;
}

vec3 Point3D(ivec2 p) {
    return vec3(ArrayPoint(p), in_terrain[ToIndex1D(p)]);
}

float Slope(ivec2 p, ivec2 q) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return 0.0f;
    if (q.x < 0 || q.x >= nx || q.y < 0 || q.y >= ny) return 0.0f;
    if (p == q) return 0.0f;
    int index_p = ToIndex1D(p.x, p.y);
    int index_q = ToIndex1D(q.x, q.y);
    float d = length(ArrayPoint(q) - ArrayPoint(p));
    return (in_terrain[index_q] - in_terrain[index_p]) / d;
}

float Stream(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return 0.0f;
    int index_p = ToIndex1D(p.x, p.y);
    return in_stream[index_p];
}

ivec2 GetFlowSteepest(ivec2 p) {
    ivec2 d = ivec2(0, 0);
    float maxSlope = 0.0f;
    for (int i = 0; i < 8; i++) {
        float ss = Slope(p + next8[i], p);
        if (ss > maxSlope) {
            maxSlope = ss;
            d = next8[i];
        }
    }
    return d;
}

float ComputeIncomingFlowSteepest(ivec2 p) {
    float stream = 0.0f;
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        ivec2 fd = GetFlowSteepest(q);
        if (q + fd == p)
            stream += Stream(q);
    }
    return stream;
}

void GetFlowWeighted(ivec2 p, out float sn[8]) {
    float slopeSum = 0.0f;
    for (int i = 0; i < 8; i++) {
        sn[i] = pow(Slope(p + next8[i], p), flow_p);
        if (sn[i] > 0.0f)
            slopeSum += sn[i];
    }
    slopeSum = (slopeSum == 0.0f) ? 1.0f : slopeSum;
    for (int i = 0; i < 8; i++)
        sn[i] = sn[i] / slopeSum;
}

float ComputeIncomingFlowWeighted(ivec2 p) {
    float stream = 0.0f;
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        float sn[8];
        GetFlowWeighted(q, sn);
        float ss = sn[(i + 4) % 8];
        if (ss > 0.0f)
            stream += ss * Stream(q);
    }
    return stream;
}

float ComputeIncomingFlow(ivec2 p) {
    //return ComputeIncomingFlowSteepest(p);
    return ComputeIncomingFlowWeighted(p);
}




layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);
    if (x < 0)   return;
    if (y < 0)   return;
    if (x >= nx) return;
    if (y >= ny) return;

    int id = ToIndex1D(x, y);
    ivec2 p = ivec2(x, y);


    // Flow accumulation at p
    float stream = 1.0f * length(cellDiag);
    stream += ComputeIncomingFlow(p);

    // steepest slope
    ivec2 d = GetFlowSteepest(p);
    float receiver_height = in_terrain[ToIndex1D(p + d)];
    float steepest_slope = abs(Slope(p + d, p));

    // stream power
    float spe = pow(stream, p_sa) * clamp(pow(steepest_slope, p_sl), 0., 1.);
    spe = clamp(spe, 0., max_spe);
    spe *= k;

    // update height
    float old_height = in_terrain[id];
    float new_height = old_height;
    new_height -= dt * spe;
    new_height = max(new_height, receiver_height);
    
    out_terrain[id] = new_height;
    out_stream[id] = stream;
}

#endif
