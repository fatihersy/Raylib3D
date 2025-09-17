#version 330

// Input vertex attributes
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform vec3 viewPos;
uniform vec3 viewTarget;
uniform vec2 resolution;

out vec4 finalColor;

#define PI 3.14159265359

// Cloud parameters - based on working example
#define cld_march_steps 50
#define cld_coverage 0.25
#define cld_thick 90.0
#define cld_absorb_coeff 1.0
#define cld_base_height 100.0

// Fog (exponential squared) density (tweak this)
const float FOG_DENSITY = 0.0009;

// Sun direction - animated like in the example
vec3 cld_sun_dir = normalize(vec3(0, 0.5, -1));
vec3 cld_wind_dir = vec3(0, 0, -time * 0.05);

// ============================================================================
// NOISE FUNCTIONS - From the working example
// ============================================================================

// (all your hash, noise_iq, snoise, fbm_clouds etc... unchanged)
float hash(float n) {
    return fract(sin(n) * 753.5453123);
}

float noise_iq(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);

    float n = p.x + p.y * 157.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
                   mix(hash(n + 157.0), hash(n + 158.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 270.0), hash(n + 271.0), f.x), f.y), f.z);
}

vec3 mod289(vec3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
    return mod289(((x * 34.0) + 1.0) * x);
}

vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v) {
    const vec2 C = vec2(1.0/6.0, 1.0/3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);

    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy;
    vec3 x3 = x0 - D.yyy;

    i = mod289(i);
    vec4 p = permute(permute(permute(
                 i.z + vec4(0.0, i1.z, i2.z, 1.0))
               + i.y + vec4(0.0, i1.y, i2.y, 1.0))
               + i.x + vec4(0.0, i1.x, i2.x, 1.0));

    float n_ = 0.142857142857;
    vec3 ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_);

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.xy, y.xy);
    vec4 b1 = vec4(x.zw, y.zw);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 42.0 * dot(m*m, vec4(dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3)));
}

float fbm_clouds(vec3 pos, float lacunarity, float init_gain, float gain) {
    vec3 p = pos;
    float H = init_gain;
    float t = 0.0;

    for (int i = 0; i < 5; i++) {
        t += abs(snoise(p)) * H;  // abs() for billowy clouds
        p *= lacunarity;
        H *= gain;
    }
    return t;
}

// ============================================================================
// VOLUME SAMPLING - Same structure as example
// ============================================================================

struct volume_sampler_t {
    vec3 origin;
    vec3 pos;
    float height;
    float coeff_absorb;
    float T; // transmittance
    vec3 C;  // color
    float alpha;
};

volume_sampler_t begin_volume(vec3 origin, float coeff_absorb) {
    volume_sampler_t v;
    v.origin = origin;
    v.pos = origin;
    v.height = 0.0;
    v.coeff_absorb = coeff_absorb;
    v.T = 1.0;
    v.C = vec3(0.0);
    v.alpha = 0.0;
    return v;
}

// Simple lighting like the example
float illuminate_volume(volume_sampler_t cloud) {
    return exp(cloud.height) / 1.95;
}

void integrate_volume(inout volume_sampler_t vol, vec3 V, vec3 L, float density, float dt, out float scalar_contrib) {
    // Beer-Lambert law
    float T_i = exp(-vol.coeff_absorb * density * dt);
    vol.T *= T_i;

    // Integrate color (scalar factor used for weighting distance later)
    float contrib = vol.T * illuminate_volume(vol) * density * dt;
    scalar_contrib = contrib;

    vol.C += contrib * vec3(1.0); // contribute to color (illumination numerics already in contrib)
    // Accumulate alpha
    vol.alpha += (1.0 - T_i) * (1.0 - vol.alpha);
}

// Cloud density function - exactly like the working example
float density_func(vec3 pos, float h) {
    vec3 p = pos * 0.001 + cld_wind_dir;
    float dens = fbm_clouds(p * 2.032, 2.6434, 0.5, 0.5);

    // IMPORTANT: This smoothstep creates the cloud coverage pattern
    dens *= smoothstep(cld_coverage, cld_coverage + 0.035, dens);

    return dens;
}

// ============================================================================
// CLOUD RENDERING - returns vec4(rgb=color, a=alpha) and out avg distance
vec4 render_clouds(vec3 ray_origin, vec3 ray_direction, out float out_avg_dist) {
    const int steps = cld_march_steps;
    const float march_step = cld_thick / float(steps);

    // Project ray to march through cloud layer
    vec3 projection = ray_direction / ray_direction.y;
    vec3 iter = projection * march_step;

    float cutoff = dot(ray_direction, vec3(0, 1, 0));

    volume_sampler_t cloud = begin_volume(
        ray_origin + projection * cld_base_height,
        cld_absorb_coeff);

    float weight_sum = 0.0;
    float weighted_dist = 0.0;

    for (int i = 0; i < steps; i++) {
        cloud.height = (cloud.pos.y - cloud.origin.y) / cld_thick;
        float dens = density_func(cloud.pos, cloud.height);

        float contrib = 0.0;
        integrate_volume(cloud, ray_direction, cld_sun_dir, dens, march_step, contrib);

        // compute sample distance along the ray to this sample point
        float sample_dist = length(cloud.pos - ray_origin);

        weight_sum += contrib;
        weighted_dist += contrib * sample_dist;

        cloud.pos += iter;

        if (cloud.alpha > 0.999) break;
    }

    // Average distance where the volumetric color was formed
    out_avg_dist = (weight_sum > 1e-6) ? (weighted_dist / weight_sum) : length(cloud.origin - ray_origin);

    // final alpha modulation by cutoff (same as before)
    float final_alpha = cloud.alpha * smoothstep(0.0, 0.2, cutoff);

    return vec4(cloud.C, final_alpha);
}

// ============================================================================
// SKY COLOR - Like the example
vec3 render_sky_color(vec3 eye_dir) {
    const vec3 sun_color = vec3(1.0, 0.7, 0.55);
    float sun_amount = max(dot(eye_dir, cld_sun_dir), 0.0);

    vec3 sky = mix(vec3(0.0, 0.1, 0.4), vec3(0.3, 0.6, 0.8), 1.0 - eye_dir.y);
    sky += sun_color * min(pow(sun_amount, 1500.0) * 5.0, 1.0);
    sky += sun_color * min(pow(sun_amount, 10.0) * 0.6, 1.0);

    return sky;
}

// ============================================================================
// GROUND  - returns color and out distance
vec3 render_ground(vec3 ro, vec3 rd, out float out_dist) {
    float ground_y = 0.0;

    if (rd.y >= 0.0) {
        out_dist = 0.0;
        return vec3(0.0); // Ray going up, no ground hit
    }

    float t = (ground_y - ro.y) / rd.y;
    if (t < 0.0) {
        out_dist = 0.0;
        return vec3(0.0);
    }

    out_dist = t;

    vec3 pos = ro + rd * t;

    // Checkerboard pattern
    float checker = mod(floor(pos.x * 0.1) + floor(pos.z * 0.1), 2.0);
    vec3 ground_color = mix(vec3(0.2, 0.4, 0.2), vec3(0.3, 0.5, 0.3), checker);

    // Simple lighting
    ground_color *= 0.6 + 0.4 * max(dot(vec3(0, 1, 0), cld_sun_dir), 0.0);
    return ground_color;
}

// ============================================================================
// FOG helper - exponential squared
float fog_exp2(float distance, float density) {
    float d = distance * density;
    return exp(-(d * d));
}

// Blend scene color toward fog color using exp^2 transmittance
vec3 apply_fog_exp2(vec3 sceneColor, vec3 fogColor, float distance, float density) {
    float trans = fog_exp2(distance, density); // transmittance (1.0 = no fog)
    trans = clamp(trans, 0.0, 1.0);
    return mix(fogColor, sceneColor, trans);
}

mat3 setCamera(in vec3 ro, in vec3 ta, float cr)
{
    vec3 cw = normalize(ta-ro);
    vec3 cp = vec3(sin(cr), cos(cr),0.0);
    vec3 cu = normalize(cross(cw,cp));
    vec3 cv = normalize(cross(cu,cw));
    return mat3(cu, cv, cw);
}

// ============================================================================
// MAIN FUNCTION
void main() {
    vec2 aspect_ratio = vec2(resolution.x / resolution.y, 1.0);
    float FOV = 0.9;
    vec3 fin_col = vec3(0.0);

    vec2 point_ndc = gl_FragCoord.xy / resolution.xy;
    //vec3 p = vec3((2.0 * point_ndc - 1.0) * aspect_ratio * FOV, -1.0);
    vec2 p = (-resolution.xy + 2.0*gl_FragCoord.xy)/resolution.y;

    // Camera setup
    vec3 ro = viewPos;
    vec3 ta = viewTarget;

    // Create ray (orthonormal basis)
    mat3 ca = setCamera(ro, ta, 0.0);

    // include z component properly
    vec3 ray_direction = ca * normalize(vec3(p.xy, 2.0));

    // Precompute fogColor as sky in direction (gives natural tint)
    vec3 fogColor = render_sky_color(ray_direction);

    //if (dot(ray_direction, vec3(0, 1, 0)) >= 0.00) {
        // Sky + clouds
        vec3 sky = render_sky_color(ray_direction);

        float cloud_dist = 0.0;
        vec4 cld = render_clouds(ro, ray_direction, cloud_dist);

        // first composite clouds over sky (cloud alpha already in cld.a)
        vec3 scene_color = mix(sky, cld.rgb, cld.a);

        // choose distance: if clouds contributed use cloud distance, else use large distance
        float distance_for_fog = (cld.a > 0.0001) ? cloud_dist : 6000.0; // large far distance for clear sky

        fin_col = apply_fog_exp2(scene_color, fogColor, distance_for_fog, FOG_DENSITY);
    //}
    //else {
    //    // Ground path
    //    float ground_dist = 0.0;
    //    vec3 ground_color = render_ground(viewPos, ray_direction, ground_dist);
    
    //    // apply fog based on ground_dist
    //    fin_col = apply_fog_exp2(ground_color, fogColor, ground_dist, FOG_DENSITY);
    //}

    // gamma correction
    fin_col = pow(fin_col, vec3(1.0 / 2.2));

    finalColor = vec4(fin_col, 1.0);
}
