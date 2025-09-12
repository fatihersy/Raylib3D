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
uniform sampler2D uNoise;

out vec4 finalColor;

mat3 m = mat3( 0.00,  0.80,  0.60, -0.80,  0.36, -0.48, -0.60, -0.48,  0.64 );
float hash( float n ) {
  return fract(sin(n)*43758.5453);
}

float noise( in vec3 x ) {
  vec3 p = floor(x);
  vec3 f = fract(x);

  f = f * f * (3.0-2.0*f);

  float n = p.x + p.y * 57.0 + 113.0 * p.z;

  float res = mix(
    mix( mix( hash(n+  0.0), hash(n+  1.0),f.x), mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
    mix( mix( hash(n+113.0), hash(n+114.0),f.x), mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z
  );

  return res;
}

float fbm( vec3 p ) {
  float f;
  f  = 0.5000*noise( p ); p = m*p*2.02;
  f += 0.2500*noise( p ); p = m*p*2.03;
  f += 0.12500*noise( p ); p = m*p*2.01;
  f += 0.06250*noise( p );
  return f;
}

float map( in vec3 p ) {
	vec3 q = p - vec3(0.0,0.5,1.0) * time;
  float f = fbm(q);
	return 1.0 - length(p * vec3(0.1, 1.0, 0.2)) + f * 2.5;
}

float jitter;
#define MAX_STEPS 48
#define SHADOW_STEPS 8
#define VOLUME_LENGTH 30.
#define SHADOW_LENGTH 2.

// Reference
// https://shaderbits.com/blog/creating-volumetric-ray-marcher
vec4 cloudMarch(vec3 p, vec3 ray) {
  float density = 0.;

  float stepLength = VOLUME_LENGTH / float(MAX_STEPS);
  float shadowStepLength = SHADOW_LENGTH / float(SHADOW_STEPS);
  vec3 light = normalize(vec3(1.0, 2.0, 1.0));

  vec4 sum = vec4(0., 0., 0., 1.);
  
  vec3 pos = p + ray * jitter * stepLength;
  
  for (int i = 0; i < MAX_STEPS; i++)
  {
    if (sum.a < 0.1) { break; }
    float d = map(pos);
    
    if( d > 0.001) {
      vec3 lpos = pos + light * jitter * shadowStepLength;
      float shadow = 0.;

      for (int s = 0; s < SHADOW_STEPS; s++) {
        lpos += light * shadowStepLength;
        float lsample = map(lpos);
        shadow += lsample;
      }

      density = clamp((d / float(MAX_STEPS)) * 20.0, 0.0, 1.0);
      float s = exp((-shadow / float(SHADOW_STEPS)) * 3.);
      sum.rgb += vec3(s * density) * vec3(1.1, 0.9, .5) * sum.a;
      sum.a *= 1.-density;
      sum.rgb += exp(-map(pos + vec3(0,0.25,0.0)) * .2) * density * vec3(0.15, 0.45, 1.1) * sum.a;
    }
    pos += ray * stepLength;
  }

  return sum;
}

mat3 setCamera() {
  vec3 cw = normalize(viewDir);
  vec3 cp = vec3(0.0, 1.0 ,0.0);
  vec3 cu = normalize(cross(cw,cp));
  vec3 cv =          (cross(cu,cw));
  return mat3(cu, cv, cw);
}

void main()
{
  vec2 uv = (gl_FragCoord.xy * 2.0 - resolution.xy) / resolution.y;
  uv.x *= resolution.x / resolution.y;

  jitter = hash(uv.x + uv.y * 57.0 + time);
  vec3 ro = vec3(0.0);
  vec3 ta = vec3(0.0, 1., 0.0);
  mat3 c = setCamera();  //camera(ro, ta, 0.0);

  vec3 ray = c * normalize(vec3(uv, 1.75));
  vec4 col = cloudMarch(ro + viewPos, ray);
  vec3 result = col.rgb + mix(vec3(0.3, 0.6, 1.0), vec3(0.05, 0.35, 1.0), uv.y + 0.75) * (col.a);
  
  float sundot = clamp(dot(ray,normalize(vec3(1.0, 2.0, 1.0))),0.0,1.0);
  result += 0.4*vec3(1.0,0.7,0.3)*pow( sundot, 4.0 );

  result = pow(result, vec3(1.0/2.2));
  
  finalColor = vec4(result,1.0);
} 


