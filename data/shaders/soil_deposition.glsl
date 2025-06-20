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

bool CheckPit(ivec2 p) {
    for (int i = 0; i < 8; i++) {
        float slope = Slope(p + next8[i], p);
        if (slope > 0.0f) return false;
    }
    return true;
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


    float steepest_slope = SteepestSlope(p);

	// Modify water & sediment values
	if (!CheckPit(p)) {
		sed = 0.;
	}

	// Add sediment and water
	stream = rain * cellArea + StreamIncomingWeighted(p);
    sed += SedIncomingWeighted(p);


	float speed = clamp(pow(steepest_slope, 2.), 0., 1.);
    float streamPower = pow(stream, 0.3) * speed;

	// Deposit
	if (deposition_strength * sed > streamPower) {
		float deposit = min(sed, (deposition_strength * sed - streamPower) * 0.1);
		height += deposit;
        sed = max(0., sed - deposit);
	}

    sed += 0.1 * streamPower;

    // write udpated values
    out_terrain[id] = height;
    out_stream[id] = stream;
    out_sed[id] = sed;
}

#endif
