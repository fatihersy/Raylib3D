# version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform vec4 colDiffuse;

// Custom Input Uniform
uniform vec3 viewPos;
uniform vec3 viewTarget;
uniform vec2 resolution;
uniform float time;

// https://learnopengl.com/Advanced-OpenGL/Depth-testing
float CalcDepth(in vec3 rd, in float Idist, in vec3 cw){
  float local_z = dot(normalize(cw),rd)*Idist;
  return (1.0/(local_z) - 1.0/0.01)/(1.0/1000.0 -1.0/0.01);
}

// Union operation
vec2 opU(vec2 d1, vec2 d2) {
  return (d1.x < d2.x) ? d1 : d2;
}

// Scene mapping function
vec2 map(in vec3 pos) {
  
  return vec2(0.0);
}

// Raycast function
vec2 raycast(in vec3 ro, in vec3 rd) {
  vec2 res = vec2(-1.0, -1.0);
  
  float tmin = 1.0;
  float tmax = 200.0;
  
  float t = tmin;
  for (int i = 0; i < 128; i++) {
    if (t > tmax) break;
    vec2 h = map(ro + rd * t);
    if (abs(h.x) < (0.0001 * t)) {
      res = vec2(t, h.y);
      break;
    }
    t += h.x * 0.8; // Slightly slower marching for terrain
  }
  
  return res;
}

// https://iquilezles.org/articles/rmshadows
float calcSoftshadow(in vec3 ro, in vec3 rd, in float mint, in float tmax) {
  float res = 1.0;
  float t = mint;
  for (int i = 0; i < 32; i++) {
    float h = map(ro + rd * t).x;
    float s = clamp(8.0 * h / t, 0.0, 1.0);
    res = min(res, s);
    t += clamp(h, 0.02, 0.5);
    if (res < 0.004 || t > tmax) break;
  }
  res = clamp(res, 0.0, 1.0);
  return res * res * (3.0 - 2.0 * res);
}

// https://iquilezles.org/articles/normalsSDF
vec3 calcNormal(in vec3 pos) {
  vec2 e = vec2(1.0, -1.0) * 0.5773 * 0.001;
  return normalize(e.xyy * map(pos + e.xyy).x + 
                   e.yyx * map(pos + e.yyx).x + 
                   e.yxy * map(pos + e.yxy).x + 
                   e.xxx * map(pos + e.xxx).x);
}

// https://iquilezles.org/articles/nvscene2008/rwwtt.pdf
float calcAO(in vec3 pos, in vec3 nor) {
  float occ = 0.0;
  float sca = 1.0;
  for (int i = 0; i < 5; i++) {
    float h = 0.01 + 0.15 * float(i) / 4.0;
    float d = map(pos + h * nor).x;
    occ += (h - d) * sca;
    sca *= 0.95;
    if (occ > 0.35) break;
  }
  return clamp(1.0 - 3.0 * occ, 0.0, 1.0) * (0.5 + 0.5 * nor.y);
}

// Main rendering function
vec4 render(in vec3 ro, in vec3 rd) {
  // Background - sky gradient
  vec3 col = vec3(0.);
  
  // Raycast scene
  vec2 res = raycast(ro, rd);
  float t = res.x;
  float m = res.y;
  
  if (m > -0.5) {
    vec3 pos = ro + t * rd;
    vec3 nor = calcNormal(pos);
    vec3 ref = reflect(rd, nor);
    
    // Lighting
    float occ = calcAO(pos, nor);
    vec3 lin = vec3(0.0);
    
    // Sun lighting
    {
      vec3 lig = normalize(vec3(-0.6, 0.6, -0.4));
      vec3 hal = normalize(lig - rd);
      float dif = clamp(dot(nor, lig), 0.0, 1.0);
      dif *= calcSoftshadow(pos, lig, 0.02, 50.0);
      float spe = pow(clamp(dot(nor, hal), 0.0, 1.0), 16.0);
      spe *= dif;
      spe *= 0.04 + 0.96 * pow(clamp(1.0 - dot(hal, lig), 0.0, 1.0), 5.0);
      
      lin += col * 2.5 * dif * vec3(1.3, 1.1, 0.9);
      lin += 4.0 * spe * vec3(1.3, 1.1, 0.9);
    }
    
    // Sky lighting
    {
      float dif = sqrt(clamp(0.5 + 0.5 * nor.y, 0.0, 1.0));
      dif *= occ;
      float spe = smoothstep(-0.2, 0.2, ref.y);
      spe *= dif;
      spe *= 0.04 + 0.96 * pow(clamp(1.0 + dot(nor, rd), 0.0, 1.0), 5.0);
      spe *= calcSoftshadow(pos, ref, 0.02, 50.0);
      
      lin += col * 0.7 * dif * vec3(0.4, 0.6, 1.0);
      lin += 1.5 * spe * vec3(0.4, 0.6, 1.0);
    }
    
    // Back lighting
    {
      float dif = clamp(dot(nor, normalize(vec3(0.5, 0.0, 0.6))), 0.0, 1.0);
      dif *= occ;
      lin += col * 0.4 * dif * vec3(0.25, 0.25, 0.25);
    }
    
    // Subsurface scattering
    {
      float dif = pow(clamp(1.0 + dot(nor, rd), 0.0, 1.0), 2.0);
      dif *= occ;
      lin += col * 0.2 * dif * vec3(1.0, 1.0, 1.0);
    }
    
    col = lin;
    
    // Atmospheric scattering
    col = mix(col, vec3(0.5, 0.7, 1.0), 1.0 - exp(-0.00005 * t * t));
  }
  
  return vec4(clamp(col, 0.0, 1.0), t);
}

mat3 setCamera(in vec3 ro, in vec3 ta, in vec3 cw, float cr)
{
    vec3 cp = vec3(sin(cr), cos(cr),0.0);
    vec3 cu = normalize(cross(cw,cp));
    vec3 cv = normalize(cross(cu,cw));
    return mat3(cu, cv, cw);
}

void main() {
    vec2 aspect_ratio = vec2(resolution.x / resolution.y, 1.0);
    float FOV = 0.9;
    vec4 noise       = texture(texture0, fragTexCoord);
    vec4 layer1_diff = texture(texture1, fragTexCoord);
    vec4 layer2_diff = texture(texture2, fragTexCoord);
    vec4 layer3_diff = texture(texture3, fragTexCoord);

    //vec3 fin_col = vec3(0.0, 0.0, 1.0);

    vec2 point_ndc = gl_FragCoord.xy / resolution.xy;
    //vec3 p = vec3((2.0 * point_ndc - 1.0) * aspect_ratio * FOV, -1.0);
    vec2 p = (-resolution.xy + 2.0*gl_FragCoord.xy)/resolution.y;

    // Camera setup
    vec3 ro = viewPos;
    vec3 ta = viewTarget;
    vec3 cw = normalize(ta-ro);

    // Create ray (orthonormal basis)
    //mat3 ca = setCamera(ro, ta, cw, 0.0);

    // include z component properly
    //vec3 ray_direction = ca * normalize(vec3(p.xy, 2.0));
    //float depth = gl_FragCoord.z;
    //{
    //    vec4 res = render(ro, ray_direction);
    //    fin_col = res.xyz;
    //    depth = CalcDepth(ray_direction, res.w, cw);
    //}
  
    //gl_FragColor = vec4(fin_col, 1.0);
    gl_FragColor = mix(layer1_diff, noise, 0.5);
    //gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    //gl_FragDepth = depth;
}
