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

void main()
{
  vec2 screen_pixel_coord = (gl_FragCoord.xy * 2.0 - resolution.xy) / resolution.y;
  vec4 tex_col = texture(texture0, fragTexCoord);

  finalColor = vec4(0.0, 0.0, 0.0, 1);
} 
