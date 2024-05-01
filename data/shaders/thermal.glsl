#version 450 core

#ifdef COMPUTE_SHADER

layout(binding = 0, std430) readonly  buffer InTerrain      { float in_terrain[];    };
layout(binding = 1, std430) writeonly buffer OutTerrain     { float out_terrain[];   };

// Heightfield data
uniform vec2 a;
uniform vec2 b;
uniform int nx;
uniform int ny;
uniform vec2 cell_diag;

// Thermal erosion parameters
uniform float eps = 0.00005f;
uniform float tanThresholdAngle = 0.57f;
uniform bool noisifiedAngle = true;
uniform float noise_min = 0.9f;
uniform float noise_max = 1.4f;
uniform float noiseWavelength = 0.0023f;
uniform bool use_threshold_map = false;


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

vec2 ArrayPoint(ivec2 p) {
	return a.xy + vec2(p) * cell_diag;
}

float Height(ivec2 p) {
	return in_terrain[ToIndex1D(p.x, p.y)];
}

vec3 Point(ivec2 p) {
	return vec3(ArrayPoint(p), Height(p));
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
	int x = int(gl_GlobalInvocationID.x);
    int y = int(gl_GlobalInvocationID.y);	
	if (x < 0) return;
	if (y < 0) return;
	if (x >= nx) return;
	if (y >= ny) return;

	int id = ToIndex1D(x, y);
	vec2 p = ArrayPoint(ivec2(x, y));

	// Sample a 3x3 grid around the pixel
	float distances[9];
	float samples[9];
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			ivec2 tapUV = (ivec2(x, y) + ivec2(i, j) - ivec2(1, 1) + ivec2(nx, ny)) % ivec2(nx, ny);
			samples[i * 3 + j] = Height(tapUV);
			distances[i * 3 + j] = length(p - ArrayPoint(tapUV));
		}
	}

	// Threshold angle from noise between noise_min and noise_max
	float tanAngle = tanThresholdAngle;
	if (noisifiedAngle) {
		vec3 p = Point(ivec2(x, y));
		float t = snoise(p * noiseWavelength) * 0.5f + 0.5f;
		tanAngle = mix(noise_min, noise_max, t);
	}

	// Check stability with all neighbours
	float z = Height(ivec2(x, y));
	float receiveMul = 0.0f;
	float distributeMul = 0.0f;
	for (int i = 0; i < 9; i++) {
		float d = distances[i];
		float zd = samples[i] - z;
		if (zd / d > tanAngle)
			receiveMul += 1.0f;

		zd = z - samples[i];
		if (zd / d > tanAngle)
			distributeMul += 1.0f;
	}

	// Add/Remove matter if necessary
	float matter = eps * cell_diag.x * cell_diag.y;
	out_terrain[id] = in_terrain[id] + matter * (receiveMul - distributeMul);
}

#endif
