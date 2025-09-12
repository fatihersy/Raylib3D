#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform float time;
uniform vec3 viewPos;
uniform vec3 viewDir;
uniform vec2 resolution;

out vec4 finalColor;

#define PI 3.14159265359

// Cloud parameters
#define cld_march_steps 50
#define cld_coverage 0.3125
#define cld_thick 90
#define cld_absorb_coeff 1.
#define cld_base_height 200.0
#define cld_wind_speed 8.0

vec3 cld_sun_dir = normalize(vec3(0.7, 0.35, 0.5));

// Better hash function for noise
vec3 hash3(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

// Improved 3D noise function
float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    
    return mix(mix(mix(dot(hash3(i + vec3(0.0, 0.0, 0.0)), f - vec3(0.0, 0.0, 0.0)),
                       dot(hash3(i + vec3(1.0, 0.0, 0.0)), f - vec3(1.0, 0.0, 0.0)), u.x),
                   mix(dot(hash3(i + vec3(0.0, 1.0, 0.0)), f - vec3(0.0, 1.0, 0.0)),
                       dot(hash3(i + vec3(1.0, 1.0, 0.0)), f - vec3(1.0, 1.0, 0.0)), u.x), u.y),
               mix(mix(dot(hash3(i + vec3(0.0, 0.0, 1.0)), f - vec3(0.0, 0.0, 1.0)),
                       dot(hash3(i + vec3(1.0, 0.0, 1.0)), f - vec3(1.0, 0.0, 1.0)), u.x),
                   mix(dot(hash3(i + vec3(0.0, 1.0, 1.0)), f - vec3(0.0, 1.0, 1.0)),
                       dot(hash3(i + vec3(1.0, 1.0, 1.0)), f - vec3(1.0, 1.0, 1.0)), u.x), u.y), u.z);
}

// Worley noise for cloud-like cellular patterns
float worley(vec3 p, float scale) {
    p *= scale;
    vec3 id = floor(p);
    vec3 fd = fract(p);
    
    float minDist = 1.0;
    
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                vec3 neighbor = vec3(float(x), float(y), float(z));
                vec3 cellPoint = hash3(id + neighbor) * 0.5 + 0.5;
                vec3 diff = neighbor + cellPoint - fd;
                float dist = length(diff);
                minDist = min(minDist, dist);
            }
        }
    }
    
    return 1.0 - minDist;
}

// Advanced FBM specifically for clouds
float cloud_fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.6;
    float frequency = 0.5;
    
    // Main cloud shape with Worley noise
    value += worley(p, frequency) * amplitude;
    amplitude *= 0.5;
    frequency *= 2.2;
    
    // Add detail layers with Perlin noise
    for (int i = 0; i < 4; i++) {
        value += abs(noise(p * frequency)) * amplitude;
        amplitude *= 0.48;
        frequency *= 2.1;
    }
    
    // Additional high-frequency detail
    value += noise(p * 16.0) * 0.03;
    
    return value;
}

// Remap function for better cloud shaping
float remap(float value, float low1, float high1, float low2, float high2) {
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

// Volume sampler structure
struct volume_sampler_t {
    vec3 origin;
    vec3 pos;
    float height;
    float coeff_absorb;
    float T;        // transmittance
    vec3 C;         // color
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

// Improved cloud lighting with multiple scattering approximation
vec3 cloud_lighting(volume_sampler_t cloud, vec3 V, vec3 L, float density) {
    // Base cloud color - white/light gray
    vec3 cloudColor = vec3(1.0, 1.0, 1.0);
    
    // Ambient light from sky
    vec3 ambientSky = vec3(0.4, 0.52, 0.7) * 0.7;
    vec3 ambientGround = vec3(0.25, 0.3, 0.2) * 0.3;
    vec3 ambient = mix(ambientGround, ambientSky, cloud.height);
    
    // Direct sunlight
    float sundot = max(dot(V, L), 0.0);
    vec3 sunColor = vec3(1.0, 0.95, 0.8);
    float sunIntensity = pow(sundot, 2.0) * 0.6 + 0.4;
    
    // Silver lining effect (forward scattering)
    float forwardScatter = pow(max(dot(V, -L), 0.0), 3.0);
    vec3 silverLining = sunColor * forwardScatter * 0.6;
    
    // Height gradient - darker at bottom
    float heightGradient = pow(cloud.height, 0.7);
    
    // Density affects light absorption
    float absorption = exp(-density * 2.0);
    
    // Combine lighting
    vec3 finalLight = ambient + (sunColor * sunIntensity + silverLining) * absorption;
    finalLight *= cloudColor * (0.5 + 0.5 * heightGradient);
    
    // Add subtle warm/cool variation
    float warmth = noise(cloud.pos * 0.002) * 0.1;
    finalLight *= vec3(1.0 + warmth * 0.1, 1.0, 1.0 - warmth * 0.1);
    
    return finalLight;
}

void integrate_volume(inout volume_sampler_t vol, vec3 V, vec3 L, float density, float dt) {
    float T_i = exp(-vol.coeff_absorb * density * dt);
    vol.T *= T_i;
    
    // Use improved lighting
    vec3 lightColor = cloud_lighting(vol, V, L, density);
    vol.C += vol.T * lightColor * density * dt * 0.5;
    
    vol.alpha += (1.0 - T_i) * (1.0 - vol.alpha);
}

// Better cloud density with more realistic shapes
float density_func(vec3 pos, float h) {
    // Wind animation
    vec3 wind = vec3(time * cld_wind_speed, time * 2.0, time * cld_wind_speed * 0.5);
    vec3 p = pos * 0.0008 + wind * 0.0001;
    
    // Base cloud shape using FBM
    float base_cloud = cloud_fbm(p);
    
    // Large scale Worley for cloud cells
    float large_worley = worley(p, 0.3);
    
    // Combine for cloud structure
    float cloud_shape = remap(base_cloud, 0.0, 1.0, 0.0, 1.0);
    cloud_shape *= large_worley;
    
    // Add small scale detail
    float detail = cloud_fbm(p * 3.5) * 0.35;
    cloud_shape = cloud_shape * 0.7 + detail * 0.3;
    
    // Coverage adjustment
    cloud_shape = remap(cloud_shape, cld_coverage, 1.0, 0.0, 1.0);
    cloud_shape = clamp(cloud_shape, 0.0, 1.0);
    
    // Vertical profile for realistic cloud shape
    float bottom_fade = smoothstep(0.0, 0.15, h);
    float top_fade = 1.0 - smoothstep(0.6, 1.0, h);
    float mid_bulge = 1.0 + sin(h * PI) * 0.4; // Clouds are fuller in middle
    
    cloud_shape *= bottom_fade * top_fade * mid_bulge;
    
    // Edge softening
    cloud_shape = pow(cloud_shape, 0.85);
    
    return cloud_shape * 2.0; // Increase overall density
}

// Beautiful sky gradient with realistic colors
vec3 render_sky_color(vec3 eye_dir) {
    float y = eye_dir.y;
    
    // Sky gradient colors
    vec3 horizonColor = vec3(0.75, 0.82, 0.93);  // Light blue-gray at horizon
    vec3 zenithColor = vec3(0.15, 0.35, 0.65);   // Deep blue at zenith
    vec3 midSkyColor = vec3(0.35, 0.55, 0.85);   // Medium blue
    
    // Multi-layer gradient for more realistic sky
    vec3 sky;
    if (y < 0.0) {
        // Below horizon - ground fog color
        sky = vec3(0.5, 0.52, 0.55);
    } else if (y < 0.1) {
        // Near horizon
        float t = y / 0.1;
        sky = mix(horizonColor, midSkyColor, t);
    } else {
        // Above horizon
        float t = pow(y, 0.35);
        sky = mix(midSkyColor, zenithColor, t);
    }
    
    // Add atmospheric scattering effect
    vec3 sunsetColor = vec3(1.0, 0.7, 0.4);
    float sunInfluence = pow(max(dot(eye_dir, cld_sun_dir), 0.0), 3.0);
    sky = mix(sky, sunsetColor, sunInfluence * 0.2);
    
    // Sun disc and glow
    vec3 sunColor = vec3(1.0, 0.98, 0.95);
    float sundot = max(dot(eye_dir, cld_sun_dir), 0.0);
    
    // Bright sun disc
    float sunDisc = smoothstep(0.9998, 0.9999, sundot);
    sky = mix(sky, sunColor, sunDisc);
    
    // Sun glow
    float sunGlow = pow(sundot, 128.0);
    sky += sunColor * sunGlow * 0.5;
    
    // Larger sun halo
    float sunHalo = pow(sundot, 8.0);
    sky += vec3(1.0, 0.9, 0.7) * sunHalo * 0.15;
    
    // Add some atmospheric haze near horizon
    float haze = 1.0 - abs(y);
    sky = mix(sky, vec3(0.85, 0.88, 0.95), haze * haze * 0.3);
    
    return sky;
}

vec4 render_clouds(vec3 ray_origin, vec3 ray_direction) {
    // Early exit for rays pointing down
    if (ray_direction.y <= -0.5) {
        return vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    // Calculate intersection with cloud layer
    float t_enter = max(0.0, (cld_base_height - ray_origin.y) / ray_direction.y);
    float t_exit = (cld_base_height + cld_thick - ray_origin.y) / ray_direction.y;
    
    // If we're inside the cloud layer
    if (ray_origin.y >= cld_base_height && ray_origin.y <= cld_base_height + cld_thick) {
        t_enter = 0.0;
    }
    
    // Skip if no intersection
    if (t_exit < t_enter || t_exit < 0.0) {
        return vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    float march_distance = min(t_exit - t_enter, 800.0);
    float march_step = march_distance / float(cld_march_steps);
    
    vec3 start_pos = ray_origin + ray_direction * t_enter;
    volume_sampler_t cloud = begin_volume(start_pos, cld_absorb_coeff);
    
    // Adaptive sampling
    float lod = smoothstep(0.0, 500.0, t_enter) * 0.5;
    
    for (int i = 0; i < cld_march_steps; i++) {
        cloud.height = (cloud.pos.y - cld_base_height) / cld_thick;
        
        if (cloud.height >= 0.0 && cloud.height <= 1.0) {
            float dens = density_func(cloud.pos, cloud.height);
            
            if (dens > 0.01) {
                integrate_volume(cloud, ray_direction, cld_sun_dir, dens, march_step);
            }
        }
        
        cloud.pos += ray_direction * march_step * (1.0 + lod);
        
        if (cloud.alpha > 0.99) break;
    }
    
    // Fade clouds near horizon
    float horizon_fade = smoothstep(-0.1, 0.15, ray_direction.y);
    
    return vec4(cloud.C, cloud.alpha * horizon_fade);
}

// Improved ground with better colors

vec3 render_ground(vec3 ro, vec3 rd) {
    float ground_y = 0.0;

    if (rd.y >= 0.0) return vec3(0.0); // Ray going up, no ground hit

    float t = (ground_y - ro.y) / rd.y;

    if (t < 0.0) return vec3(0.0);

    vec3 pos = ro + rd * t;

    // Checkerboard pattern

    float checker = mod(floor(pos.x * 0.1) + floor(pos.z * 0.1), 2.0);

    vec3 ground_color = mix(vec3(0.2, 0.4, 0.2), vec3(0.3, 0.5, 0.3), checker);

    // Fog based on distance

    float fog = exp(-t * 0.001);

    ground_color *= fog;

    // Simple lighting

    ground_color *= 0.6 + 0.4 * max(dot(vec3(0, 1, 0), cld_sun_dir), 0.0);

    return ground_color;

}

void main() {
    vec2 uv = (gl_FragCoord.xy - resolution.xy * 0.5) / resolution.y;
    
    // Ray setup
    vec3 ro = viewPos;
    
    // Create camera matrix from view direction
    vec3 forward = normalize(viewDir);
    vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up = cross(right, forward);
    
    // Ray direction with slightly wider FOV
    vec3 rd = normalize(forward + right * uv.x * 1.2 + up * uv.y * 1.2);
    
    // Render sky
    vec3 sky = render_sky_color(rd);
    vec3 result = sky;
    
    // Render ground
    vec3 ground = render_ground(ro, rd);
    if (length(ground) > 0.01) {
        result = ground;
    } else {
        // Render clouds
        vec4 cld = render_clouds(ro, rd);
        result = mix(sky, cld.rgb, cld.a);
    }
    
    // Additional sun glare
    float sundot = clamp(dot(rd, cld_sun_dir), 0.0, 1.0);
    result += vec3(1.0, 0.95, 0.8) * pow(sundot, 32.0) * 0.3;
    
    // Color grading for more vibrant look
    result = pow(result, vec3(0.95)); // Slight brightness boost
    result = mix(result, result * result, 0.1); // Slight contrast
    
    // Tone mapping and gamma correction
    result = vec3(1.0) - exp(-result * 1.2); // Exposure tone mapping
    result = pow(result, vec3(1.0 / 2.2));   // Gamma correction
    
    // Subtle vignette
    float vignette = 1.0 - length(uv) * 0.3;
    result *= vignette;
    
    finalColor = vec4(result, 1.0);
}
