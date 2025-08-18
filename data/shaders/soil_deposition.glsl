#version 450 core

#ifdef COMPUTE_SHADER

// terrain
layout(binding = 0, std430) readonly  buffer InTerrain  { float in_terrain[]; };
layout(binding = 1, std430) writeonly buffer OutTerrain { float out_terrain[]; };

// stream
layout(binding = 2, std430) readonly  buffer InStream   { float in_stream[]; };
layout(binding = 3, std430) writeonly buffer OutStream  { float out_stream[]; };

// sediment
layout(binding = 4, std430) readonly  buffer InSed      { float in_sed[]; };
layout(binding = 5, std430) writeonly buffer OutSed     { float out_sed[]; };

struct SoilTexture {
    float silt;
    float sand;
    float clay;
};

// soil texture

layout(binding = 6, std430) readonly  buffer InSoilTex { SoilTexture in_soiltex[]; };
layout(binding = 7, std430) writeonly buffer OutSoilTex { SoilTexture out_soiltex[]; };

struct SedimentClass {
    float p_silt; // primary silt
    float p_sand; // primary sand
    float p_clay; // primary clay
    float small_aggregate;
    float large_aggregate;
};

// sediment texture
layout(binding = 8, std430) readonly  buffer InSedTex { SoilTexture in_sedtex[]; };
layout(binding = 9, std430) writeonly buffer OutSedTex { SoilTexture out_sedtex[]; };


//
//// silt
//layout(binding = 6, std430) readonly  buffer InSilt      { float in_silt[]; };
//layout(binding = 7, std430) writeonly buffer OutSilt     { float out_silt[]; };
//
//// sand
//layout(binding = 8, std430) readonly  buffer InSand      { float in_sand[]; };
//layout(binding = 9, std430) writeonly buffer OutSand     { float out_sand[]; };
//
//// clay
//layout(binding = 10, std430) readonly  buffer InClay      { float in_clay[]; };
//layout(binding = 11, std430) writeonly buffer OutClay     { float out_clay[]; };
//

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

float Sed(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return 0.0f;
    int index_p = ToIndex1D(p.x, p.y);
    return in_sed[index_p];
}

vec3 SoilTex(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return vec3(0.0f);
    int index_p = ToIndex1D(p.x, p.y);
    return vec3(in_soiltex[index_p].silt, in_soiltex[index_p].sand, in_soiltex[index_p].clay);
}

vec3 SedTex(ivec2 p) {
    if (p.x < 0 || p.x >= nx || p.y < 0 || p.y >= ny) return vec3(0.0f);
    int index_p = ToIndex1D(p.x, p.y);
    return vec3(in_sedtex[index_p].silt, in_sedtex[index_p].sand, in_sedtex[index_p].clay);
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

ivec2 GetFlowShallowest(ivec2 p) {
    ivec2 d = ivec2(0, 0);
    float maxSlope = 0.0f;
    for (int i = 0; i < 8; i++) {
        float ss = Slope(p, p + next8[i]);
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


float ComputeIncomingFlowShallowest(ivec2 p) {
    float stream = 0.0f;
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        ivec2 fd = GetFlowShallowest(q);
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

float calc_k_factor(ivec2 p) {
    float k = 0.0f;
    vec3 soiltex = SoilTex(p);
    float vfs_estimated =  (0.74 - 0.62 * soiltex.b) * soiltex.b;
    float k_tb = 2.1f * pow((soiltex.r*100f + vfs_estimated*100f)*(100.0f - soiltex.b *100f), 1.14f) / 10000f;
    float k_t = k_tb;
    if (soiltex.r + vfs_estimated > 0.68f) {
        float k_t68 = 2.1f * pow(68f*(100.0f - soiltex.b*100f), 1.14f)/10000f;
        k_t = k_tb - 0.67f * pow((k_tb - k_t68), 0.82);
    }
    float k_min = 0.5;
    float k_max = 1.0f;
    k = k_t / 100f;
//    k = k_min + (k_max - k_min) * k;
    return k;
}

float SedIncomingWeighted(ivec2 p) {
    float sed = 0.0f;
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        float sn[8];
        GetFlowWeighted(q, sn);
        float ss = sn[(i + 4) % 8];
        if (ss > 0.0f) {
            float k = calc_k_factor(q);
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
    result.r = max(result.r, 0.0f);
    result.g = max(result.g, 0.0f);
    result.b = max(result.b, 0.0f);
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

const int PRIMARY_SILT = 0;
const int PRIMARY_SAND = 1;
const int PRIMARY_CLAY = 2;
const int SMALL_AGGREGATE = 3;
const int LARGE_AGGREGATE = 4;
//https://www.ars.usda.gov/ARSUserFiles/60600505/rusle/rusle2_science_doc.pdf pg 133
vec3 calcDetachRatio(vec3 soiltex, out float[5] fracMassThisClass, out float[5] fracSiltThisClass, out float[5] fracSandThisClass, out float[5] fracClayThisClass) {
    float primary_clay = soiltex.b * 0.26;
    float primary_sand = soiltex.g * pow(max(1.0 - soiltex.b, 0.0), 5);
    float small_aggregate = calcSmallAggregate(soiltex);

    float primary_silt;
    float large_agg;
    float sum;
    vec3 small_agg_comp;
    vec3 large_agg_comp;
    bool done = false;
    int loops = 0;
    while (!done && loops < 50) {
        primary_silt = soiltex.r - small_aggregate;
        if (primary_silt < 0.0f) {
            primary_silt = 0.0001f;
            small_aggregate = soiltex.r - primary_silt;
            if (small_aggregate < 0.0f) {
                small_aggregate = 0.0f;
            }
        }

        large_agg = (1.0f - primary_sand - primary_clay - primary_silt - small_aggregate);
        if (large_agg < 0.0f) {
            large_agg = 0.0001f;
            float correction = 1.0f / (1.0f + abs(large_agg) + 0.01f);
            primary_silt *= correction;
            primary_sand *= correction;
            primary_clay *= correction;
            small_aggregate *= correction;
            large_agg *= correction;
        }

        small_agg_comp = vec3(soiltex.r / (soiltex.r + soiltex.b),
                              0.00f,
                              soiltex.b / (soiltex.r + soiltex.b));

        float large_agg_clay = (soiltex.b - primary_clay - small_aggregate * small_agg_comp.b) / large_agg;
        float large_agg_silt = (soiltex.r - primary_silt - small_aggregate * small_agg_comp.r) / large_agg;
        large_agg_comp = vec3(large_agg_silt,
                            (soiltex.g - primary_sand) /large_agg,
                            large_agg_clay);

        // large agg clay = 0.5 * %clay = 0.5 * (soiltex.b)
        // = 0.5 * (primary_clay + small_agg_comp.b * small_aggregate)
        // large_agg_clay / 0.5 = primary_clay + small_agg_comp.b * small_aggregate
        // => small_aggregate = (large_agg_clay / 0.5 - primary_clay) / small_agg_comp.b;
        if (large_agg_clay < 0.5f * 0.95f * soiltex.b) {
            small_aggregate = (large_agg_clay / 0.5f - primary_clay) / small_agg_comp.b;
            large_agg_clay = 0.5f * soiltex.b;
            // RUSLE2 small aggregate: (looks weird)
            float f1f2f5 = primary_silt + primary_sand + primary_clay;
            large_agg_clay = 0.5f * soiltex.b;
            small_aggregate = 0.0f;
            if (small_aggregate - large_agg >= 0.0f) {
                small_aggregate = (soiltex.b -large_agg_clay - primary_clay + large_agg_clay * f1f2f5 )
                        / (small_agg_comp.b - large_agg_clay);
            }
        } else {
            done = true;
        }

        loops++;
    }

    sum = primary_silt + primary_sand + primary_clay + small_aggregate + large_agg;
    // out silt:
    for (int i = 0; i < 5; i++) {
        fracMassThisClass[i] = 0.0f;
        fracSiltThisClass[i] = 0.0f;
        fracSandThisClass[i] = 0.0f;
        fracClayThisClass[i] = 0.0f;
    }

    fracMassThisClass[PRIMARY_SILT] = primary_silt;
    fracMassThisClass[PRIMARY_SAND] = primary_sand;
    fracMassThisClass[PRIMARY_CLAY] = primary_clay;
    fracMassThisClass[SMALL_AGGREGATE] = small_aggregate;
    fracMassThisClass[LARGE_AGGREGATE] = large_agg;

    fracSiltThisClass[PRIMARY_SILT] = 1.0f;
    fracSandThisClass[PRIMARY_SAND] = 1.0f;
    fracClayThisClass[PRIMARY_CLAY] = 1.0f;

    fracSiltThisClass[SMALL_AGGREGATE] = small_agg_comp.r;
    fracSandThisClass[SMALL_AGGREGATE] = small_agg_comp.g;
    fracClayThisClass[SMALL_AGGREGATE] = small_agg_comp.b;

    fracSiltThisClass[LARGE_AGGREGATE] = large_agg_comp.r;
    fracSandThisClass[LARGE_AGGREGATE] = large_agg_comp.g;
    fracClayThisClass[LARGE_AGGREGATE] = large_agg_comp.b;

    vec3 detachRatio = vec3(primary_silt, primary_sand, primary_clay);

//    detachRatio = AddRatio(detachRatio, small_aggregate * small_agg_comp);
    detachRatio += small_aggregate * small_agg_comp;

    detachRatio = AddRatio(detachRatio, large_agg * large_agg_comp);

    return detachRatio;
}


float CalcFallVelocity(float specific_gravity, float diameter) {
    return 0.0005f * specific_gravity * (diameter * diameter);
}

vec3 SoilTexIncomingWeighted(ivec2 p, float streamPower, float sedTotal) {
    vec3 soiltex = vec3(0.0f);
    float[5] fracMassThisClass;
    float[5] fracSiltThisClass;
    float[5] fracSandThisClass;
    float[5] fracClayThisClass;

    float cur_surf_area = dot(SoilTex(p), vec3(4.0f, 0.05f, 20.0f));
    for (int i = 0; i < 8; i++) {
        ivec2 q = p + next8[i];
        float sn[8];
        GetFlowWeighted(q, sn);
        float ss = sn[(i + 4) % 8];
        if (ss > 0.0f) {
//            vec3 detach = calcDetachRatio(SedTex(q), fracMassThisClass, fracSiltThisClass, fracSandThisClass, fracClayThisClass);
//            detach.r
//            soiltex += ss * detach;
//            soiltex = AddRatio(soiltex, ss * SoilTex(q));
            vec3 incoming_tex = SedTex(q);
            float incoming_surf_area = dot(incoming_tex, vec3(4.0f, 0.05f, 20.0f));
            float enrichment_ratio = incoming_surf_area/cur_surf_area;
//            vec3 incoming = ss * (Sed(q)/sed_total) * enrichment_ratio * detach;
            vec3 incoming = (ss * Sed(q))/sedTotal * enrichment_ratio * incoming_tex;

            float[5] fracMassThisClass;
            float[5] fracSiltThisClass;
            float[5] fracSandThisClass;
            float[5] fracClayThisClass;

            float[5] fracMassThisClassIncoming;
            float[5] fracSiltThisClassIncoming;
            float[5] fracSandThisClassIncoming;
            float[5] fracClayThisClassIncoming;
            incoming = calcDetachRatio(incoming_tex, fracMassThisClassIncoming, fracSiltThisClassIncoming, fracSandThisClassIncoming, fracClayThisClassIncoming);
//            streamPower = streamPower - sedTotal;
//            if (streamPower > 0.0f) {
//                    fracMassThisClassIncoming[PRIMARY_SILT] = 4.0f / 0.1 * streamPower * (cellDiag.x * cellDiag.y * 0.1 - fracMassThisClassIncoming[PRIMARY_SILT] * sedTotal);
//                    fracMassThisClassIncoming[PRIMARY_SAND] = 20.0f / 0.1 * streamPower * (cellDiag.x * cellDiag.y  * 0.1 - fracMassThisClassIncoming[PRIMARY_SAND] * sedTotal);
//                    fracMassThisClassIncoming[PRIMARY_CLAY] = 0.05f / 0.1 * streamPower * (cellDiag.x * cellDiag.y  * 0.1 - fracMassThisClassIncoming[PRIMARY_CLAY] * sedTotal);
//                    fracMassThisClassIncoming[SMALL_AGGREGATE]= (4.0f * fracMassThisClassIncoming[SMALL_AGGREGATE].r + 20.0f*fracSandThisClassIncoming[SMALL_AGGREGATE] + 0.05f*fracClayThisClassIncoming[SMALL_AGGREGATE]) / 3.0 * 0.1 * streamPower * (cellDiag.x * cellDiag.y * 0.1 - fracMassThisClassIncoming[SMALL_AGGREGATE]* sedTotal);
//                    fracMassThisClassIncoming[LARGE_AGGREGATE]= (4.0f*fracSiltThisClassIncoming[LARGE_AGGREGATE] + 20.0f*fracSandThisClassIncoming[LARGE_AGGREGATE] + 0.05f*fracClayThisClassIncoming[LARGE_AGGREGATE]) / 3.0 * 0.1 * streamPower * (cellDiag.x * cellDiag.y * 0.1 - fracMassThisClassIncoming[LARGE_AGGREGATE]* sedTotal);
//            }

            float[5] specific_gravity_this_class = float[5](2.65f, 2.65f, 2.60f, 1.80f, 1.60f);
            float[5] diameter_this_class = float[5](0.01f, 0.2f, 0.002f, 0.01f, 0.1f);

//            for (int i = 0; i < 5; i++) {
//                if (fracMassThisClassIncoming[i] < 0.0f) {
//                    fracMassThisClassIncoming[i] *= CalcFallVelocity(specific_gravity_this_class[i], diameter_this_class[i]);
//                }
//            }

            incoming = vec3(
                fracMassThisClassIncoming[PRIMARY_SILT] * fracSiltThisClassIncoming[PRIMARY_SILT],
                fracMassThisClassIncoming[PRIMARY_SAND] * fracSandThisClassIncoming[PRIMARY_SAND],
                fracMassThisClassIncoming[PRIMARY_CLAY] * fracClayThisClassIncoming[PRIMARY_CLAY]
            ) + vec3(
                fracMassThisClassIncoming[SMALL_AGGREGATE] * fracSiltThisClassIncoming[SMALL_AGGREGATE],
                fracMassThisClassIncoming[SMALL_AGGREGATE] * fracSandThisClassIncoming[SMALL_AGGREGATE],
                fracMassThisClassIncoming[SMALL_AGGREGATE] * fracClayThisClassIncoming[SMALL_AGGREGATE]
            ) + vec3(
                fracMassThisClassIncoming[LARGE_AGGREGATE] * fracSiltThisClassIncoming[LARGE_AGGREGATE],
                fracMassThisClassIncoming[LARGE_AGGREGATE] * fracSandThisClassIncoming[LARGE_AGGREGATE],
                fracMassThisClassIncoming[LARGE_AGGREGATE] * fracClayThisClassIncoming[LARGE_AGGREGATE]
            );
            incoming /= incoming.r + incoming.g + incoming.b;

//
//            float[5] specific_gravity_this_class = float[5](2.65f, 2.65f, 2.60f, 1.80f, 1.60f);
//            float[5] diameter_this_class = float[5](0.01f, 0.2f, 0.002f, 0.01f, 0.1f); // in mm
//            float[5] fall_velocity_this_class = float[5](CalcFallVelocity(specific_gravity_this_class[PRIMARY_SILT],
//                                                                          diameter_this_class[PRIMARY_SILT]),
//            CalcFallVelocity(specific_gravity_this_class[PRIMARY_SAND], diameter_this_class[PRIMARY_SAND]),
//            CalcFallVelocity(specific_gravity_this_class[PRIMARY_CLAY], diameter_this_class[PRIMARY_CLAY]),
//            CalcFallVelocity(specific_gravity_this_class[SMALL_AGGREGATE], diameter_this_class[SMALL_AGGREGATE]),
//            CalcFallVelocity(specific_gravity_this_class[LARGE_AGGREGATE], diameter_this_class[LARGE_AGGREGATE]));
//            vec3 incoming_sedtex = calcDetachRatio(SedTex(q), fracMassThisClassIncoming, fracSiltThisClassIncoming, fracSandThisClassIncoming, fracClayThisClassIncoming);

//            incoming_sedtex = vec3(fracMassThisClassIncoming[PRIMARY_SILT] * fall_velocity_this_class[PRIMARY_SILT], fracMassThisClassIncoming[PRIMARY_SAND] * fall_velocity_this_class[PRIMARY_SAND], fracMassThisClassIncoming[PRIMARY_CLAY] * fall_velocity_this_class[PRIMARY_CLAY]);
//            incoming_sedtex.r += fracSiltThisClassIncoming[SMALL_AGGREGATE] * fracMassThisClassIncoming[SMALL_AGGREGATE] * fall_velocity_this_class[SMALL_AGGREGATE];
//            incoming_sedtex.g += fracSandThisClassIncoming[SMALL_AGGREGATE] * fracMassThisClassIncoming[SMALL_AGGREGATE] * fall_velocity_this_class[SMALL_AGGREGATE];
//            incoming_sedtex.b += fracClayThisClassIncoming[SMALL_AGGREGATE] * fracMassThisClassIncoming[SMALL_AGGREGATE] * fall_velocity_this_class[SMALL_AGGREGATE];
//
//            incoming_sedtex.r += fracSiltThisClassIncoming[LARGE_AGGREGATE] * fracMassThisClassIncoming[LARGE_AGGREGATE] * fall_velocity_this_class[LARGE_AGGREGATE];
//            incoming_sedtex.g += fracSandThisClassIncoming[LARGE_AGGREGATE] * fracMassThisClassIncoming[LARGE_AGGREGATE] * fall_velocity_this_class[LARGE_AGGREGATE];
//            incoming_sedtex.b += fracClayThisClassIncoming[LARGE_AGGREGATE] * fracMassThisClassIncoming[LARGE_AGGREGATE] * fall_velocity_this_class[LARGE_AGGREGATE];
//            vec3 incoming_sedtex = (ss * Sed(q))/sed_total * enrichment_ratio * incoming_tex;
            soiltex = AddRatio(soiltex, incoming * ss);
//            soiltex += ss * SoilTex(q);
        }
    }
    return soiltex;
}

bool CheckPit(ivec2 p) {
    for (int i = 0; i < 8; i++) {
        float slope = Slope(p + next8[i], p);
        if (slope > 0.0f) return false;
    }
    return true;
}

vec3 SoilTexIncomingSteepest(ivec2 p, float sedTotal, float streamPower) {
    float[5] fracMassThisClass;
    float[5] fracSiltThisClass;
    float[5] fracSandThisClass;
    float[5] fracClayThisClass;

    float[5] fracMassThisClassIncoming;
    float[5] fracSiltThisClassIncoming;
    float[5] fracSandThisClassIncoming;
    float[5] fracClayThisClassIncoming;

    float[5] specific_gravity_this_class = float[5](2.65f, 2.65f, 2.60f, 1.80f, 1.60f);
    float[5] diameter_this_class = float[5](0.01f, 0.2f, 0.002f, 0.01f, 0.1f); // in mm
    float[5] fall_velocity_this_class = float[5](CalcFallVelocity(specific_gravity_this_class[PRIMARY_SILT],
                                                diameter_this_class[PRIMARY_SILT]),
                                               CalcFallVelocity(specific_gravity_this_class[PRIMARY_SAND], diameter_this_class[PRIMARY_SAND]),
                                               CalcFallVelocity(specific_gravity_this_class[PRIMARY_CLAY], diameter_this_class[PRIMARY_CLAY]),
                                               CalcFallVelocity(specific_gravity_this_class[SMALL_AGGREGATE], diameter_this_class[SMALL_AGGREGATE]),
                                               CalcFallVelocity(specific_gravity_this_class[LARGE_AGGREGATE], diameter_this_class[LARGE_AGGREGATE]));

    float cur_surf_area = dot(SedTex(p), vec3(4.0f, 0.05f, 20.0f));
//    float ss = ComputeIncomingFlowSteepest(p);
    ivec2 q = p + GetFlowSteepest(p);
    float ss = ComputeIncomingFlowSteepest(p);
//    float ss = Stream(q);
//    vec3 detach = calcDetachRatio(SoilTex(q), ss, 0.0f);
//    vec3 incoming_sedtex = calcDetachRatio(SoilTex(q), fracMassThisClassIncoming, fracSiltThisClassIncoming, fracSandThisClassIncoming, fracClayThisClassIncoming);
//    vec3 incoming_sedtex = SoilTex(q);

//    vec3 incoming_sedtex = SedTex(q);
    vec3 incoming_sedtex = calcDetachRatio(SoilTex(q), fracMassThisClassIncoming, fracSiltThisClassIncoming, fracSandThisClassIncoming, fracClayThisClassIncoming);

    incoming_sedtex = vec3(fracMassThisClassIncoming[PRIMARY_SILT]  * fall_velocity_this_class[PRIMARY_SILT],
                            fracMassThisClassIncoming[PRIMARY_SAND] * fall_velocity_this_class[PRIMARY_SAND],
                            fracMassThisClassIncoming[PRIMARY_CLAY]) * fall_velocity_this_class[PRIMARY_CLAY];
    incoming_sedtex.r += fracSiltThisClassIncoming[SMALL_AGGREGATE] * fracMassThisClassIncoming[SMALL_AGGREGATE]
     * fall_velocity_this_class[SMALL_AGGREGATE];
    incoming_sedtex.g += fracSandThisClassIncoming[SMALL_AGGREGATE] * fracMassThisClassIncoming[SMALL_AGGREGATE]
     * fall_velocity_this_class[SMALL_AGGREGATE];
    incoming_sedtex.b += fracClayThisClassIncoming[SMALL_AGGREGATE] * fracMassThisClassIncoming[SMALL_AGGREGATE]
     * fall_velocity_this_class[SMALL_AGGREGATE];

    incoming_sedtex.r += fracSiltThisClassIncoming[LARGE_AGGREGATE] * fracMassThisClassIncoming[LARGE_AGGREGATE]
     * fall_velocity_this_class[LARGE_AGGREGATE];
    incoming_sedtex.g += fracSandThisClassIncoming[LARGE_AGGREGATE] * fracMassThisClassIncoming[LARGE_AGGREGATE]
     * fall_velocity_this_class[LARGE_AGGREGATE];
    incoming_sedtex.b += fracClayThisClassIncoming[LARGE_AGGREGATE] * fracMassThisClassIncoming[LARGE_AGGREGATE]
     * fall_velocity_this_class[LARGE_AGGREGATE];

    incoming_sedtex /= incoming_sedtex.r + incoming_sedtex.g + incoming_sedtex.b;

    //account for seperate transport capacities:
    if (streamPower > 0.0f && !CheckPit(p)) {
        fracMassThisClassIncoming[PRIMARY_SILT] = 4.0f / 0.1 * streamPower * (cellDiag.x * cellDiag.y * 0.1 - fracMassThisClassIncoming[PRIMARY_SILT] * sedTotal);
        fracMassThisClassIncoming[PRIMARY_SAND] = 20.0f / 0.1 * streamPower * (cellDiag.x * cellDiag.y  * 0.1 - fracMassThisClassIncoming[PRIMARY_SAND] * sedTotal);
        fracMassThisClassIncoming[PRIMARY_CLAY] = 0.05f / 0.1 * streamPower * (cellDiag.x * cellDiag.y  * 0.1 - fracMassThisClassIncoming[PRIMARY_CLAY] * sedTotal);
        fracMassThisClassIncoming[SMALL_AGGREGATE]= (4.0f*fracMassThisClassIncoming[SMALL_AGGREGATE].r + 20.0f*fracSandThisClassIncoming[SMALL_AGGREGATE] + 0.05f*fracClayThisClassIncoming[SMALL_AGGREGATE]) / 3.0 * 0.1 * streamPower * (cellDiag.x * cellDiag.y * 0.1 - fracMassThisClassIncoming[SMALL_AGGREGATE]* sedTotal);
        fracMassThisClassIncoming[LARGE_AGGREGATE]= (4.0f*fracSiltThisClassIncoming[LARGE_AGGREGATE] + 20.0f*fracSandThisClassIncoming[LARGE_AGGREGATE] + 0.05f*fracClayThisClassIncoming[LARGE_AGGREGATE]) / 3.0 * 0.1 * streamPower * (cellDiag.x * cellDiag.y * 0.1 - fracMassThisClassIncoming[LARGE_AGGREGATE]* sedTotal);
    }

//    incoming_sedtex = vec3(fracMassThisClassIncoming[PRIMARY_SILT] *
//                            fall_velocity_this_class[PRIMARY_SILT],
//                            fracMassThisClassIncoming[PRIMARY_SAND] * fall_velocity_this_class[PRIMARY_SAND],
//                            fracMassThisClassIncoming[PRIMARY_CLAY] * fall_velocity_this_class[PRIMARY_CLAY]);
//
//
//    incoming_sedtex += vec3(fracMassThisClassIncoming[SMALL_AGGREGATE] *                        fracSiltThisClassIncoming[SMALL_AGGREGATE] * fall_velocity_this_class[SMALL_AGGREGATE],
//                            fracMassThisClassIncoming[SMALL_AGGREGATE] * fracSandThisClassIncoming[SMALL_AGGREGATE] * fall_velocity_this_class[SMALL_AGGREGATE],
//                            fracMassThisClassIncoming[SMALL_AGGREGATE] * fracClayThisClassIncoming[SMALL_AGGREGATE] * fall_velocity_this_class[SMALL_AGGREGATE]);
//
//    incoming_sedtex += vec3(fracMassThisClassIncoming[LARGE_AGGREGATE] *                        fracSiltThisClassIncoming[LARGE_AGGREGATE] * fall_velocity_this_class[LARGE_AGGREGATE],
//                            fracMassThisClassIncoming[LARGE_AGGREGATE] * fracSandThisClassIncoming[LARGE_AGGREGATE] * fall_velocity_this_class[LARGE_AGGREGATE],
//                            fracMassThisClassIncoming[LARGE_AGGREGATE] * fracClayThisClassIncoming[LARGE_AGGREGATE] * fall_velocity_this_class[LARGE_AGGREGATE]);
//
//
//    incoming_sedtex /= incoming_sedtex.r + incoming_sedtex.g + incoming_sedtex.b;

//    vec3 current_sedtex = calcDetachRatio(SoilTex(p), fracMassThisClass, fracSiltThisClass, fracSandThisClass, fracClayThisClass);


//    vec3 current_sedtex = SoilTex(p);

    float incoming_surf_area = dot(incoming_sedtex, vec3(4.0f, 0.05f, 20.0f));
    float enrichment_ratio = incoming_surf_area/cur_surf_area;

//    vec3 sedtex = AddRatio(current_sedtex, 1.0f/cellDiag.r * ss * incoming_sedtex * enrichment_ratio);
//    vec3 sedtex = AddRatio(current_sedtex, ss * incoming_sedtex * streamPower * enrichment_ratio);
//    soiltex = AddRatio(soiltex, incoming);
    vec3 sedtex = incoming_sedtex * streamPower * enrichment_ratio;
    return sedtex;
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


    float height = in_terrain[id];
    float stream = in_stream[id];
    float sed = in_sed[id];

    float initial_sed = sed;
    vec3 soiltex = vec3(in_soiltex[id].silt, in_soiltex[id].sand, in_soiltex[id].clay);
    vec3 sedtex = vec3(in_sedtex[id].silt, in_sedtex[id].sand, in_sedtex[id].clay);

    float steepest_slope = SteepestSlope(p);

	// Modify water & sediment values
	if (!CheckPit(p)) {
		sed = 0.;
	}

	// Add sediment and water
	stream = rain * cellArea + StreamIncomingWeighted(p);
    float incoming_sed = SedIncomingWeighted(p);
    sed += incoming_sed;

	float speed = clamp(pow(steepest_slope, 2.), 0., 1.);
    float streamPower = pow(stream, 0.3) * speed;

	// Deposit
    // TODO: sediment classes should be seperated at this point
    float deposit = 0.0;
//    sedtex = SoilTexIncomingSteepest(p, sed, streamPower);
    sedtex = SoilTexIncomingWeighted(p, streamPower, sed);
	if (deposition_strength * sed > streamPower) {
		deposit = min(sed, (deposition_strength * sed - streamPower) * 0.1);
		height += deposit;
        soiltex = AddRatio(soiltex, deposit * sedtex);
        sedtex = AddRatio(sedtex,  -deposit * sedtex);
	}
    float [5] fracMassThisClass;
    float [5] fracSiltThisClass;
    float [5] fracSandThisClass;
    float [5] fracClayThisClass;

    float detach = streamPower * calc_k_factor(p);
    vec3 outsedtex = calcDetachRatio(soiltex, fracMassThisClass, fracSiltThisClass, fracSandThisClass, fracClayThisClass);
//    vec3 outsed = AddRatio(1.0f/cellDiag.r * detach/height * (outsedtex), sedtex);
    vec3 outsed = AddRatio(sedtex, soiltex* detach );
//        soiltex = AddRatio(soiltex, streamPower * sed * SoilTexIncomingWeighted(p));
//
//    soiltex = AddRatio(soiltex, 0.01 * deposit * SoilTexIncomingWeighted(p, streamPower, sed));
//    soiltex = AddRatio(soiltex,  deposit * ComputeIncomingSoilTexSteepest(p));
    sed = max(0., sed - deposit);

    // Detach
    sed += detach;
//    vec3 detachedSedRation = calcDetachRatio(sedtex,


    // write udpated values
    out_terrain[id] = height;
    out_stream[id] = stream;
    out_sed[id] = sed;
//    vec3 dbg_c = calcDetachRatio(vec3(in_soiltex[id].silt, in_soiltex[id].sand, in_soiltex[id].clay), fracMassThisClass, fracSiltThisClass, fracSandThisClass, fracClayThisClass);
//    out_soiltex[id] = SoilTexture(dbg_c.r, dbg_c.g, dbg_c.b);
    out_soiltex[id] = SoilTexture(soiltex.r, soiltex.g, soiltex.b);
    out_sedtex[id] = SoilTexture(outsed.r, outsed.g, outsed.b);
//    out_sedtex[id] = SoilTexture(float( GetFlowShallowest(p).x), float(GetFlowShallowest(p).y), 0.0f);
}

#endif
