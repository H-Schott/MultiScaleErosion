#version 450 core

#ifdef COMPUTE_SHADER

// terrain
layout(binding = 0, std430) readonly  buffer InTerrain  { float in_terrain[]; };
layout(binding = 1, std430) writeonly buffer OutTerrain { float out_terrain[]; };

//// stream
//layout(binding = 2, std430) readonly  buffer InStream   { float in_stream[]; };
//layout(binding = 3, std430) writeonly buffer OutStream  { float out_stream[]; };
//
// sediment
layout(binding = 2, std430) readonly  buffer InSed      { float in_sed[]; };
layout(binding = 3, std430) writeonly buffer OutSed     { float out_sed[]; };

struct SoilTex {
    float si; // silt
    float sa; // sand
    float cl; // clay
};
layout(binding = 4, std430) readonly  buffer InSoilTex { SoilTex in_soiltex[]; };
layout(binding = 5, std430) writeonly buffer OutSoilTex { SoilTex out_soiltex[]; };

// water
layout(binding = 6, std430) readonly  buffer InWater     { float in_water[]; };
layout(binding = 7, std430) writeonly buffer OutWater    { float out_water[]; };

// velocity
layout(binding = 8, std430) readonly  buffer InVelocity  { vec2 in_velocity[]; };
layout(binding = 9, std430) writeonly buffer OutVelocity { vec2 out_velocity[]; };

//flux
struct Flux {
    float l; // left
    float r; // right
    float t; // top
    float b; // bottom
};
layout(binding = 10, std430) readonly  buffer InFlux      { Flux in_flux[]; };
layout(binding = 11, std430) writeonly buffer OutFlux     { Flux out_flux[]; };

uniform int nx;
uniform int ny;
uniform vec2 a;
uniform vec2 b;
uniform vec2 cellDiag;

uniform float deposition_strength = 1.;

uniform float flow_p = 1.3f;
const float cellArea = (b.x - a.x) * (b.y - a.y) / float((nx - 1) * (ny - 1)) * 0.00001;
const float rain = 2.6;

uniform float p_sa = 0.8f;
uniform float p_sl = 2.0f;

const ivec2 next8[8] = ivec2[8](
    ivec2(0, 1), ivec2(1, 1), ivec2(1, 0), ivec2(1, -1),
    ivec2(0, -1), ivec2(-1, -1), ivec2(-1, 0), ivec2(-1, 1)
    );

int ToIndex1D(int i, int j) {
    return i + nx * j;
}

int ToIndex1D(ivec2 p) {
    return p.x + nx * p.y;
}

#define DEFINE_SAMPLER(bufferName, functionName, type) \
type functionName(ivec2 p) { \
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return type(0.0f); \
    int index_p = ToIndex1D(p); \
    return bufferName[index_p]; \
};

#define DEFINE_BILINEAR_SAMPLER(bufferName, functionName, type) \
type functionName(vec2 p) { \
    vec2 uva = floor(p); \
    vec2 uvb = ceil(p); \
    int id00 = ToIndex1D(ivec2(uva)); \
    int id10 = ToIndex1D(ivec2(uvb.x, uva.y)); \
    int id01 = ToIndex1D(ivec2(uva.x, uvb.y)); \
    int id11 = ToIndex1D(ivec2(uvb)); \
    vec2 d = p - uva;   \
    return bufferName[id00] * (1 - d.x) * (1 - d.y) + \
            bufferName[id10] * d.x * (1 - d.y) + \
            bufferName[id01] * (1 - d.x) * d.y + \
            bufferName[id11] * d.x * d.y; \
};\

DEFINE_SAMPLER(in_terrain, Terrain, float);
//DEFINE_SAMPLER(in_sed, Sediment, float);

DEFINE_BILINEAR_SAMPLER(in_sed, SedimentBilinear, float);
DEFINE_BILINEAR_SAMPLER(in_velocity, VelBilinear, vec2);


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
    int index_p = ToIndex1D(p);
    int index_q = ToIndex1D(q);
    float d = length(ArrayPoint(q) - ArrayPoint(p));
    return (in_terrain[index_q] - in_terrain[index_p]) / d;
}

float Stream(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return 0.0f;
    int index_p = ToIndex1D(p);
//    return in_stream[index_p];
    return 0.0f;
}

float Sed(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return 0.0f;
    int index_p = ToIndex1D(p);
    return in_sed[index_p];
}

vec3 GetSoilTex(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return vec3(0.0f);
    int index_p = ToIndex1D(p);
//    return vec3(in_silt[index_p], in_sand[index_p], in_clay[index_p]);
//    return in_soiltex[index_p].rgb;
    return vec3(in_soiltex[index_p].si, in_soiltex[index_p].sa, in_soiltex[index_p].cl);
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

float SteepestSlope(ivec2 p) {
    return Slope(p + GetFlowSteepest(p), p);
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

float StreamIncomingWeighted(ivec2 p) {
    float stream = 0.0f;
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        float sn[8];
        GetFlowWeighted(q, sn);
        float ss = sn[(i + 4) % 8];
        if (ss > 0.0f) {
            stream += ss * Stream(q);
        }
    }
    return stream;
}

float SedIncomingWeighted(ivec2 p) {
    float sed = 0.0f;
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        float sn[8];
        GetFlowWeighted(q, sn);
        float ss = sn[(i + 4) % 8];
        if (ss > 0.0f) {
            sed += ss * Sed(q);
        }
    }
    return sed;
}

vec3 AddRatio(vec3 a, vec3 b) {
    vec3 result = a + b;
    float sum = result.x + result.y + result.z;
    if (sum > 0.0f) {
        result /= sum;
    } else {
        result = vec3(1.0f / 3.0f);
    }
    return result;
}

//https://www.ars.usda.gov/ARSUserFiles/60600505/rusle/rusle2_science_doc.pdf pg 133
float calcSmallAggregate(vec3 soiltex) {
    float clay = soiltex.b;
    float small_agg = 0.0f;
    if (clay < 0.25f) {
        small_agg = 1.8f * clay;
    } else if (clay <= 0.5f) {
        small_agg = 0.45f - 0.6f * (clay - 0.25f);
    } else {
        small_agg = 0.6f * clay;
    }
    return small_agg;
}

//https://www.ars.usda.gov/ARSUserFiles/60600505/rusle/rusle2_science_doc.pdf pg 133
vec3 calcDetachRatio(vec3 soiltex, float streamPower) {
    float primary_clay = soiltex.b * 0.26;
    float primary_sand = soiltex.g * pow(max(1.0 - soiltex.b, 0.0), 5);
    float small_aggregate = calcSmallAggregate(soiltex);
    float primary_silt = soiltex.r - small_aggregate;
    if (primary_silt < 0.0f) {
        primary_silt = 0.001f;
        small_aggregate = soiltex.r - primary_silt;
    }

//    vec3 detachRatio = vec3(primary_silt, primary_sand, primary_clay);

    float sum = primary_silt + primary_sand + primary_clay + small_aggregate;

    float large_agg = (1.0f - primary_sand - primary_clay - primary_silt - small_aggregate);
    if (large_agg < 0.0f) {
        large_agg = 0.001f;
    }

    vec3 small_agg_comp = vec3(soiltex.r/(soiltex.r+soiltex.b),
                                0.00f,
                                soiltex.b/(soiltex.g+soiltex.g));
    vec3 large_agg_comp = vec3(soiltex.r - primary_silt - small_aggregate*small_agg_comp.r,
                                soiltex.g - primary_sand,
                                soiltex.b - primary_clay - small_aggregate*small_agg_comp.b);
//    if (large_agg_comp.b < 0.5f) {
//        small_aggregate *= 0.5f / large_agg_comp.b;
//    }
    if (large_agg_comp.b < 0.5f * soiltex.b) {
        //        small_aggregate *= 0.5f / large_agg_comp.b;
        float fac = large_agg_comp.b / (0.5f * soiltex.b);
        small_aggregate *= fac;
        large_agg_comp.b = 0.5f * soiltex.b;
        float large_agg = (1.0f - primary_sand - primary_clay - primary_silt - small_aggregate);
        //        large_agg_comp.r = soiltex.r - primary_silt - small_aggregate * small_agg_comp.r;
    }

    //account for density:
    primary_silt *= 2.65f;
    primary_clay *= 2.60f;
    small_aggregate *= 1.80f;
    large_agg *= 1.60f;
    primary_sand *= 2.65f;

    sum += large_agg;
    primary_silt /= sum;
    primary_sand /= sum;
    primary_clay /= sum;
    small_aggregate /= sum;

    //account for seperate transport capacities:
                 // K_t    gamma
//    float tc_silt = 0.1f * primary_silt;
    vec3 detachRatio = vec3(primary_silt, primary_sand, primary_clay)/sum;
//    if (streamPower > 0.0f) {
//        detachRatio.r = 4.0f / 0.1 * streamPower * (cellArea * 0.1 - detachRatio.r * sed_total);
//        detachRatio.g = 20.0f / 0.1 * streamPower * (cellArea * 0.1 - detachRatio.g * sed_total);
//        detachRatio.b = 0.05f / 0.1 * streamPower * (cellArea * 0.1 - detachRatio.b * sed_total);
//        small_aggregate = (4.0f*small_agg_comp.r + 20.0f*small_agg_comp.g + 0.05f*small_agg_comp.b) / 3.0 * 0.1 * streamPower * (cellArea * 0.1 - small_aggregate * sed_total);
//        large_agg = (4.0f*large_agg_comp.r + 20.0f*large_agg_comp.g + 0.05f*large_agg_comp.b) / 3.0 * 0.1 * streamPower * (cellArea * 0.1 - large_agg * sed_total);
//    }

    detachRatio = AddRatio(detachRatio, small_aggregate * small_agg_comp);

    // large aggregate:
    detachRatio = AddRatio(detachRatio,  large_agg * large_agg_comp);
    // account for density
//    detachRatio = AddRatio(detachRatio, detachRatio * vec3(1.0f / 2.65f, 1.0f / 2.65f, 1.0f / 2.65f));


//    detachRatio += soiltex * (1.0f - primary_sand - primary_clay - primary_silt - small_aggregate);

//    if (sum > 0.0f) {
//        detachRatio /= sum;
//    } else {
//        detachRatio = vec3(0.0f / 3.0f);
//    }

    return detachRatio;
}

//vec3 SoilTexIncomingWeighted(ivec2 p, float streamPower, float sed_total) {
//    vec3 soiltex = vec3(0.0f);
//    for (int i = 0; i < 8; i++) {
//        ivec2 q = p + next8[i];
//        float sn[8];
//        GetFlowWeighted(q, sn);
//        float ss = sn[(i + 4) % 8];
//        if (ss > 0.0f) {
//            vec3 detach = calcDetachRatio(GetSoilTex(q), streamPower, sed_total);
////            detach.r
////            soiltex += ss * detach;
////            soiltex = AddRatio(soiltex, ss * SoilTex(q));
//            soiltex = AddRatio(soiltex, ss * (Sed(q)/sed_total) * detach);
////            soiltex += ss * SoilTex(q);
//        }
//    }
//    return soiltex;
//}

bool CheckPit(ivec2 p) {
    for (int i = 0; i < 8; i++) {
        float slope = Slope(p + next8[i], p);
        if (slope > 0.0f) return false;
    }
    return true;
}

vec4 outflow_flux(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return vec4(0.0f);
    int index_p = ToIndex1D(p);
    return vec4(in_flux[index_p].l, in_flux[index_p].r,
                in_flux[index_p].t, in_flux[index_p].b);
}

float Water(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return 0.0f;
    int index_p = ToIndex1D(p);
    return in_water[index_p];
}

// from https://github.com/bshishov/UnityTerrainErosionGPU/blob/master/Assets/Shaders/Erosion.compute
// p is in the range [0, nx], [0, ny]
//vec2 SampleBilinear2(vec2 p, vec2[] buf) {
//    vec2 uva = floor(p);
//    vec2 uvb = ceil(p);
//
//    int id00 = ToIndex1D(ivec2(uva));
//    int id10 = ToIndex1D(ivec2(uvb.x, uva.y));
//    int id01 = ToIndex1D(ivec2(uva.x, uvb.y));
//    int id11 = ToIndex1D(ivec2(uvb));
//
//    vec2 d = p - uva;
//
//    return buf[id00] * (1-d.x) * (1-d.y) +
//           buf[id10] * d.x * (1-d.y) +
//           buf[id01] * (1-d.x) * d.y +
//           buf[id11] * d.x * d.y;
//}

float CalculateTiltAngle(ivec2 p, float length_y, float length_x) {
//     Clamp coordinates to valid range
    int xL = max(p.x - 1, 0);
    int xR = min(p.x + 1, nx - 1);
    int yD = max(p.y - 1, 0);
    int yU = min(p.y + 1, ny - 1);

    // Sample heights
    float hL = in_terrain[ToIndex1D(xL, p.y)];
    float hR = in_terrain[ToIndex1D(xR, p.y)];
    float hD = in_terrain[ToIndex1D(p.x, yD)];
    float hU = in_terrain[ToIndex1D(p.x, yU)];

    vec3 dhdx = vec3(2.0f * float(cellDiag.x), hR - hL, 0.0f);
    vec3 dhdy = vec3(0.0f, hU - hD, 2.0f * float(cellDiag.y));
    vec3 normal = cross(dhdx, dhdy);
    float sinTiltAngle = abs(normal.y) / length(normal);
    return sinTiltAngle;
}

vec2 GetCellCenter(ivec2 p) {
    // Calculate the center of the cell at position p
    return vec2(float(p.x) * cellDiag.x + cellDiag.x * 0.5f,
                float(p.y) * cellDiag.y + cellDiag.y * 0.5f);
}

// Bilinear interpolation for sediment sampling
float SampleSedimentBilinear(vec2 pos) {
    // Clamp to valid domain
    pos = clamp(pos, vec2(0.5f), vec2(float(nx * cellDiag.x) - 0.5f, float(ny * cellDiag.y) - 0.5f));

    // Get integer coordinates of the four surrounding points
    int x0 = int(floor(pos.x - 0.5f));
    int y0 = int(floor(pos.y - 0.5f));
    int x1 = min(x0 + 1, nx - 1);
    int y1 = min(y0 + 1, ny - 1);

    // Ensure bounds
    x0 = max(x0, 0);
    y0 = max(y0, 0);

    // Get fractional parts for interpolation
    float fx = pos.x - 0.5f - float(x0);
    float fy = pos.y - 0.5f - float(y0);

    // Sample the four corner values
    float s00 = in_sed[ToIndex1D(x0, y0)];  // bottom-left
    float s10 = in_sed[ToIndex1D(x1, y0)];  // bottom-right
    float s01 = in_sed[ToIndex1D(x0, y1)];  // top-left
    float s11 = in_sed[ToIndex1D(x1, y1)];  // top-right

    // Bilinear interpolation
    float s0 = mix(s00, s10, fx);  // bottom edge
    float s1 = mix(s01, s11, fx);  // top edge
    return mix(s0, s1, fy);        // final interpolation
}


// Bilinear interpolation for velocity field
vec2 GetVelocityAt(vec2 pos) {
    // Clamp to valid domain
//    pos = clamp(pos, vec2(0.5f), vec2(float(nx) - 0.5f, float(ny) - 0.5f));
    float length_x = cellDiag.x;
    float length_y = cellDiag.y;
    pos = clamp(pos, vec2(a.x + length_x * 0.5f, a.y + length_y * 0.5f),
                vec2(b.x - length_x * 0.5f, b.y - length_y * 0.5f));

    // Get integer coordinates of the four surrounding points
    int x0 = int(floor(pos.x - 0.5f));
    int y0 = int(floor(pos.y - 0.5f));
    int x1 = min(x0 + 1, nx - 1);
    int y1 = min(y0 + 1, ny - 1);

    // Ensure bounds
    x0 = max(x0, 0);
    y0 = max(y0, 0);

    // Get fractional parts for interpolation
    float fx = pos.x - 0.5f - float(x0);
    float fy = pos.y - 0.5f - float(y0);

    // Sample the four corner velocity values
    vec2 v00 = in_velocity[ToIndex1D(x0, y0)].xy;  // bottom-left
    vec2 v10 = in_velocity[ToIndex1D(x1, y0)].xy;  // bottom-right
    vec2 v01 = in_velocity[ToIndex1D(x0, y1)].xy;  // top-left
    vec2 v11 = in_velocity[ToIndex1D(x1, y1)].xy;  // top-right

    // Bilinear interpolation for both x and y components
    vec2 v0 = mix(v00, v10, fx);  // bottom edge
    vec2 v1 = mix(v01, v11, fx);  // top edge
    return mix(v0, v1, fy);       // final interpolation
}

vec2 SemiLagrangianBacktrackSimple(ivec2 p, float dt) {
    float length_x = cellDiag.x;
    float length_y = cellDiag.y;
    vec2 pos = vec2(float(p.x) + 0.5f, float(p.y) + 0.5f);
//    vec2 pos = GetCellCenter(p);

    // Get velocity at current position
    vec2 velocity = GetVelocityAt(pos);

    // Apply CFL limiting:
    // d_t * vel.u <= length_x, d_t * vel.v <= length_y
    if (length_x > 0.0f && length_y > 0.0f) {
        float cfl_x = length_x / (abs(velocity.x) + 1e-6f);
        float cfl_y = length_y / (abs(velocity.y) + 1e-6f);
        float cfl = min(cfl_x, cfl_y);
        dt = min(dt, cfl);
    }
//    float max_displacement = 0.8f;
//    float vel_magnitude = length(velocity);
//    if (vel_magnitude * dt > max_displacement) {
//        velocity = velocity * (max_displacement / (vel_magnitude * dt));
//    }

    // Simple backward Euler step
    return pos - velocity * dt;
}

const float d_t = 0.004f * (128.0f/nx); // time step

// constants from https://github.com/bshishov/UnityTerrainErosionGPU/blob/master/Assets/Scripts/Simulation.cs
// evaporation rate
const float K_evaporation = 0.015f;
// rainfall rate
const float K_rainfall = 0.012f;// * (128.0f/nx); // rainfall rate
// gravity
const float g = 9.81f;
// sediment transport capacity
const float K_transport_capacity = 1.0f;
// sediment deposition rate
const float K_deposition = 1.0f;
// sediment erosion rate
const float K_erosion = 0.5f;


layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {
    int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);
    if (x < 0)   return;
    if (y < 0)   return;
    if (x >= nx) return;
    if (y >= ny) return;

    ivec2 p = ivec2(x, y);
    int id = ToIndex1D(p);
//    float d_t =

//    float
//    float height = in_terrain[id];
    float height = Terrain(p);
//    float stream = in_stream[id];
    float sed = in_sed[id];


    // -- hydraulic erosion --
    float water = in_water[id];
//    float length_y = cellArea / (b.x - a.x);
//    float length_x = cellArea / (b.y - a.y);
//    float length_y = (b.y - a.y)/ float(ny - 1) * 0.01;
//    float length_x = (b.x - a.x)/ float(nx - 1) * 0.01;
    float length_y = cellDiag.y;
    float length_x = cellDiag.x;
    water += K_rainfall * d_t;

    float leftoutflow = outflow_flux(p).r;
    float rightoutflow = outflow_flux(p).g;
    float topoutflow = outflow_flux(p).b;
    float bottomoutflow = outflow_flux(p).a;

    float d_h_left = 0.0f;
    if (p.x > 0)
        d_h_left = height + water - in_terrain[ToIndex1D(p + ivec2(-1, 0))] - Water(p + ivec2(-1, 0));
    float d_h_right = 0.0f;
    if (p.x < nx - 1)
        d_h_right = height + water - in_terrain[ToIndex1D(p + ivec2(1, 0))] - Water(p + ivec2(1, 0));
    float d_h_top = 0.0f;
    if (p.y < ny - 1)
        d_h_top = height + water - in_terrain[ToIndex1D(p + ivec2(0, 1))] - Water(p + ivec2(0, 1));
    float d_h_bottom = 0.0f;
    if (p.y > 0)
        d_h_bottom = height + water - in_terrain[ToIndex1D(p + ivec2(0, -1))] - Water(p + ivec2(0, -1));
//    float d_h_left = height + water - in_terrain[ToIndex1D(p + ivec2(-1, 0))] - Water(p + ivec2(-1, 0));
//    float d_h_right = height + water - in_terrain[ToIndex1D(p + ivec2(1, 0))] - Water(p + ivec2(1, 0));
//    float d_h_top = height + water - in_terrain[ToIndex1D(p + ivec2(0, 1))] - Water(p + ivec2(0, 1));
//    float d_h_bottom = height + water - in_terrain[ToIndex1D(p + ivec2(0, -1))] - Water(p + ivec2(0, -1));

    leftoutflow = max(0.0f, leftoutflow + d_t * length_y* g * d_h_left / length_x);
    rightoutflow = max(0.0f, rightoutflow +d_t *  length_y* g * d_h_right / length_x);
    topoutflow = max(0.0f, topoutflow + d_t * length_x*  g * d_h_top / length_y);
    bottomoutflow = max(0.0f, bottomoutflow + d_t * length_x* g * d_h_bottom/ length_y);


//    float scaling_factor = min(1.0f, (water*cellArea)/((leftoutflow + rightoutflow + topoutflow + bottomoutflow) * d_t));
    float total_outflow = leftoutflow + rightoutflow + topoutflow + bottomoutflow;
    float scaling_factor = min(1.0f, (water * length_x*length_y) / (total_outflow * d_t));
    leftoutflow *= scaling_factor;
    rightoutflow *= scaling_factor;
    topoutflow *= scaling_factor;
    bottomoutflow *= scaling_factor;

    float leftinflow = 0.0f;
    if (p.x > 0) {
        leftinflow = outflow_flux(p + ivec2(-1, 0)).g;
    }
    float rightinflow = 0.0f;
    if (p.x < nx - 1) {
        rightinflow = outflow_flux(p + ivec2(1, 0)).r;
    }
    float topinflow = 0.0f;
    if (p.y < ny - 1) {
        topinflow = outflow_flux(p + ivec2(0, 1)).a;
    }
    float bottominflow = 0.0f;
    if (p.y > 0) {
        bottominflow = outflow_flux(p + ivec2(0, -1)).b;
    }

    vec3 soiltex = GetSoilTex(p);
    vec3 incoming_soiltex_left = GetSoilTex(p + ivec2(-1, 0));
    vec3 incoming_soiltex_right = GetSoilTex(p + ivec2(1, 0));
    vec3 incoming_soiltex_top = GetSoilTex(p + ivec2(0, 1));
    vec3 incoming_soiltex_bottom = GetSoilTex(p + ivec2(0, -1));

    incoming_soiltex_left = calcDetachRatio(incoming_soiltex_left, leftinflow);
    incoming_soiltex_right = calcDetachRatio(incoming_soiltex_right, rightinflow);
    incoming_soiltex_top = calcDetachRatio(incoming_soiltex_top, topinflow);
    incoming_soiltex_bottom = calcDetachRatio(incoming_soiltex_bottom, bottominflow);



    float d_V = leftinflow + rightinflow + topinflow + bottominflow - leftoutflow - rightoutflow - topoutflow - bottomoutflow;
    d_V *= d_t;

    float water_2 = water +  d_V / (length_y * length_x);
    water_2 = max(0.0f, water_2); // prevent negative water
    float avg_water = (water + water_2) / 2.0f;

    float d_W_x = (leftinflow - leftoutflow + rightinflow - rightoutflow) / 2.0f;
    float d_W_y = (topinflow - topoutflow + bottominflow - bottomoutflow) / 2.0f;

    float vel_x = (avg_water > 1e-6f) ? d_W_x / (avg_water * length_y) : 0.0f;
//    float vel_x = d_W_x / (avg_water * length_y);
    float vel_y = (avg_water > 1e-6f) ? d_W_y / (avg_water * length_x) : 0.0f;
//    float vel_y = d_W_y / (avg_water * length_x);
    vec2 vel = vec2(vel_x, vel_y);
    out_velocity[id] = vel;

    float sin_tilt_angle = CalculateTiltAngle(p, length_y, length_x);

    float max_erosion_depth = 0.9f;
    float lmax = 1.0f - (max(0.0f, max_erosion_depth - water_2) / max_erosion_depth);
    lmax = clamp(lmax, 0.0f, 1.0f);

    float capacity = K_transport_capacity * min(sin_tilt_angle, 0.1) * length(vel) * lmax;



//    ivec2 back_velocity = ivec2(int(-vel.x * d_t), int(-vel.y * d_t));
//    vec2 departure_point = SemiLagrangianBacktrackSimple(p, d_t);
    vec2 departure_point = vec2(float(p.x), float(p.y));
    departure_point -= (vel / cellDiag) * d_t; // move back in time
//    departure_point = SemiLagrangianBacktrackSimple(p, d_t);
//    float transported_sed = SampleSedimentBilinear(departure_point);
    float transported_sed = SedimentBilinear(departure_point);
//    float sed = in_sed[id];
//    float transport_strength = clamp(length(vel) * d_t, 0.0f, 1.0f);
//    sed = mix(sed, transported_sed, transport_strength);
    sed = transported_sed;
    if (capacity > sed) {
        float erosion =  d_t* K_erosion * (capacity - sed);
        height -= erosion;
        sed += erosion;
//                water_2 -= erosion;
    } else {
        float deposition = d_t * K_deposition * (sed - capacity);
        height += deposition;
        sed -= deposition;
        //                water_2 += deposition;
//        soiltex = AddRatio(soiltex, incoming_soiltex_left * leftinflow * deposition);
//        soiltex = AddRatio(soiltex, incoming_soiltex_right * rightinflow * deposition);
//        soiltex = AddRatio(soiltex, incoming_soiltex_top * topinflow * deposition);
//        soiltex = AddRatio(soiltex, incoming_soiltex_bottom * bottominflow * deposition);
        vec3 incoming_soiltex = GetSoilTex(ivec2(departure_point));
        incoming_soiltex = calcDetachRatio(incoming_soiltex, length(vel) );
        float incoming_surf_area = dot(incoming_soiltex, vec3(4.0f, 0.05f, 20.0f));
        float surf_area = dot(soiltex, vec3(4.0f, 0.05f, 20.0f));
        float enrichment_ratio = incoming_surf_area / surf_area;

        soiltex = AddRatio(soiltex, incoming_soiltex * enrichment_ratio * deposition * d_t);
    }
    vec3 debug_detach = calcDetachRatio(soiltex, 1.0f);

    out_water[id] = water_2 * (1.0f - K_evaporation * d_t); // evaporation
    out_flux[id] = Flux(leftoutflow, rightoutflow, topoutflow, bottomoutflow);
    out_sed[id] = sed;
    out_terrain[id] = height;
    out_velocity[id] = vel;

//    out_soiltex[id].si = sed /100.0f;
//    out_soiltex[id].cl = water_2 * 2560.0f/ 100.0f;
    out_soiltex[id].si = soiltex.r;
    out_soiltex[id].sa = soiltex.g;
    out_soiltex[id].cl = soiltex.b;
//    out_soiltex[id].si = debug_detach.r;
//    out_soiltex[id].sa = debug_detach.g;
//    out_soiltex[id].cl = debug_detach.b;
//    out_soiltex[id].si = ivec2(departure_point).x/100.0f;
//    out_soiltex[id].sa = ivec2(departure_point).y/100.0f;

    // -- end hydraulic erosion --
//    float initial_sed = sed;
//    vec3 soiltex = vec3(in_silt[id], in_sand[id], in_clay[id]);
//
//
//    float steepest_slope = SteepestSlope(p);
//
//	// Modify water & sediment values
//	if (!CheckPit(p)) {
//		sed = 0.;
//	}
//
//	// Add sediment and water
//	stream = rain * cellArea + StreamIncomingWeighted(p);
//    float incoming_sed = SedIncomingWeighted(p);
//    sed += incoming_sed;

//    soiltex =

//	float speed = clamp(pow(steepest_slope, 2.), 0., 1.);
//    float streamPower = pow(stream, 0.3) * speed;
//
//	// Deposit
//    // TODO: sediment classes should be seperated at this point
//    float deposit = 0.0;
//	if (deposition_strength * sed > streamPower) {
//		deposit = min(sed, (deposition_strength * sed - streamPower) * 0.1);
//		height += deposit;
//	}
////        soiltex = AddRatio(soiltex, streamPower * sed * SoilTexIncomingWeighted(p));
//
//    soiltex = AddRatio(soiltex, 0.05 * deposit * SoilTexIncomingWeighted(p, streamPower, sed));
//    sed = max(0., sed - deposit);
////
////    soiltex = AddRatio(soiltex, 0.1 * sed * SoilTexIncomingWeighted(p));
////    soiltex.b -= 0.0001 * sed * soiltex.b; // clay hardening??
//
//    // Detach
//    sed += 0.1 * streamPower;
////    soiltex = AddRatio(soiltex, min(0.0f, -0.001f * streamPower) * calcDetachRatio(soiltex, streamPower, sed));

    // write udpated values
//    out_terrain[id] = height;
//    out
////    out_stream[id] = stream;
//    out_sed[id] = sed;
//    out_silt[id] =
//    out_sand[id] = soiltex.y;
//    out_clay[id] = soiltex.z;
}

#endif
