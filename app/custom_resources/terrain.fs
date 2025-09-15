#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform float time;

// Output fragment color
out vec4 finalColor;

// Terrain colors
const vec3 GRASS_COLOR = vec3(0.2, 0.5, 0.1);
const vec3 ROCK_COLOR = vec3(0.5, 0.5, 0.5);
const vec3 SNOW_COLOR = vec3(0.9, 0.9, 0.9);
const vec3 SAND_COLOR = vec3(0.76, 0.7, 0.5);
const vec3 WATER_COLOR = vec3(0.0, 0.4, 0.8);

// Atmosphere sun color
const vec3 SUN_COLOR = vec3(1.0, 0.9, 0.7);
const vec3 SKY_COLOR = vec3(0.5, 0.7, 1.0);

void main()
{
    // Calculate lighting (enhanced Phong model with atmosphere-like sun)
    vec3 normal = normalize(fragNormal);
    vec3 viewD = normalize(viewPos - fragPosition);
    
    // Directional light (sun) - synchronized with atmosphere shader
    vec3 lightDir = normalize(vec3(sin(time * 0.1), 0.5, -cos(time * 0.1)));
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular lighting
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewD, reflectDir), 0.0), 32.0);
    float specular = 0.2 * spec;
    
    // Ambient light with sky contribution
    float ambient = 0.2;
    vec3 ambientColor = mix(SKY_COLOR * 0.5, SUN_COLOR, 0.3);
    
    // Height-based coloring with improved transitions
    float height = fragPosition.y;
    vec3 terrainColor;
    
    // Snow on peaks, rock on steep slopes, grass on gentle slopes
    float slope = 1.0 - normal.y; // 0 = flat, 1 = vertical
    
    if (height < -3.0) {
        // Deep water
        terrainColor = WATER_COLOR * 0.7;
    } else if (height < -1.0) {
        // Shallow water
        terrainColor = mix(WATER_COLOR, SAND_COLOR, (height + 3.0) / 2.0);
    } else if (height < 0.0) {
        // Sand/beach
        terrainColor = SAND_COLOR;
    } else if (height > 5.0 || (height > 3.0 && slope < 0.3)) {
        // Snow on high peaks or high flat areas
        terrainColor = mix(ROCK_COLOR, SNOW_COLOR, min(1.0, (height - 3.0) / 2.0));
    } else if (slope > 0.5 || height > 2.0) {
        // Rock on steep slopes or medium heights
        terrainColor = ROCK_COLOR;
    } else {
        // Grass elsewhere
        terrainColor = GRASS_COLOR;
    }
    
    // Apply lighting with sun color
    vec3 sunLight = SUN_COLOR * diff;
    vec3 specLight = SUN_COLOR * specular;
    vec3 ambientLight = ambientColor * ambient;
    
    // Apply atmospheric scattering effect based on height
    float atmosphereInfluence = smoothstep(0.0, 10.0, height);
    vec3 atmosphereColor = mix(SKY_COLOR, SUN_COLOR, 0.5);
    vec3 finalLight = ambientLight + sunLight + specLight;
    
    // Add subtle atmospheric scattering on higher terrain
    terrainColor = mix(terrainColor, atmosphereColor, atmosphereInfluence * 0.2);
    
    // Apply texture if available (optional)
    vec4 texelColor = texture(texture0, fragTexCoord);
    
    // Combine colors
    finalColor = vec4(terrainColor * finalLight, 1.0) * texelColor * colDiffuse;
}