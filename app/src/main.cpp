#include "raylib.h"

#include "raymath.h"
#include "rlgl.h"
#include <math.h> // Required for: tanf()

#include "defines.h"

#include <core/fmemory.h>

#define GLSL_VERSION 330
#define SHADER_FILE "../app/custom_resources/"
#define TERRAIN_FILE "./terrain/"

typedef struct raymarch_locs {
  u32 camPos;
  u32 camDir;
  u32 screenCenter;
  u32 time;
} raymarch_locs;

typedef struct map_obj_locs {
  u32 time;
  u32 viewPos;
  u32 viewCenter;
  u32 resolution;
} map_obj_locs;

typedef struct atmosphere_locs {
  u32 time;
  u32 viewPos;
  u32 viewTarget;
  u32 resolution;
} atmosphere_locs;

typedef struct terrain_locs {
  u32 viewPos;
  u32 viewTarget;
  u32 resolution;
  u32 time;
} terrain_locs;

typedef struct main_system_state {
	Model guide_plane;
	Model terrain;
} main_system_state;
static main_system_state * state = nullptr;

// Load custom render texture, create a writable depth texture buffer
static RenderTexture2D LoadRenderTextureDepthTex(int width, int height);
static void UnloadRenderTextureDepthTex(RenderTexture2D target);
void draw_guide_plane(void);
const char * rsrc(const char * file_name);
const char * rterr(const char * file_name);

int main(void) {
	memory_system_initialize();
	state = (main_system_state*)allocate_memory_linear(sizeof(main_system_state), true);

  const Vector2 resolution = Vector2 { 1280.f, 720.f };
  InitWindow(resolution.x, resolution.y, "Raylib3D");

	Image checker_image = GenImageChecked(1024, 1024, 512, 512, Color {128, 142, 155, 255}, Color {72, 84, 96, 255});
	Texture checker_texture = LoadTextureFromImage(checker_image);

	Shader shdr_map_obj = LoadShader(0, TextFormat(rsrc("map_objects.fs"), GLSL_VERSION));
  map_obj_locs map_obj_shdr_locs = {};
  map_obj_shdr_locs.time = GetShaderLocation(shdr_map_obj, "time");
  map_obj_shdr_locs.viewPos = GetShaderLocation(shdr_map_obj, "viewPos");
  map_obj_shdr_locs.viewCenter = GetShaderLocation(shdr_map_obj, "viewCenter");
  map_obj_shdr_locs.resolution = GetShaderLocation(shdr_map_obj, "resolution");

  SetShaderValue(shdr_map_obj, map_obj_shdr_locs.resolution, &(resolution), RL_SHADER_UNIFORM_VEC2);

	std::array<f32, 2> tiling = std::array<f32, 2>({ 10.0f, 10.0f });
	Shader shdrTiling = LoadShader(0, TextFormat(rsrc("tiling.fs"), GLSL_VERSION));
	SetShaderValue(shdrTiling, GetShaderLocation(shdrTiling, "tiling"), tiling.data(), SHADER_UNIFORM_VEC2);
	Mesh plane_mesh = GenMeshPlane(20.f, 20.f, 1.f, 1.f);
	state->guide_plane = LoadModelFromMesh(plane_mesh);
	state->guide_plane.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = checker_texture;
  state->guide_plane.materials[0].shader = shdrTiling;

  Shader shdrAtmosphere = LoadShader(0, TextFormat(rsrc("atmosphere.fs"), GLSL_VERSION));
  atmosphere_locs atmosphere_shdr_locs = atmosphere_locs {
    .time = static_cast<u32>(GetShaderLocation(shdrAtmosphere, "time")),
    .viewPos = static_cast<u32>(GetShaderLocation(shdrAtmosphere, "viewPos")),
    .viewTarget = static_cast<u32>(GetShaderLocation(shdrAtmosphere, "viewTarget")),
    .resolution = static_cast<u32>(GetShaderLocation(shdrAtmosphere, "resolution")),
  };

  SetShaderValue(shdrAtmosphere, atmosphere_shdr_locs.resolution, &(resolution), RL_SHADER_UNIFORM_VEC2);

  // Create terrain
  [[__maybe_unused__]] Texture2D rocks_tex = LoadTexture(rterr("mntn_gray_d.jpg"));
  [[__maybe_unused__]] Texture2D grass_tex = LoadTexture(rterr("grass_green_d.jpg"));
  [[__maybe_unused__]] Texture2D snow_tex = LoadTexture(rterr("snow1_d.jpg"));

  Shader shdrTerrain = LoadShader(0, rsrc("terrain.fs"));
  terrain_locs terrain_shdr_locs = {};
  terrain_shdr_locs.viewPos    = GetShaderLocation(shdrTerrain, "viewPos");
  terrain_shdr_locs.viewTarget = GetShaderLocation(shdrTerrain, "viewTarget");
  terrain_shdr_locs.resolution = GetShaderLocation(shdrTerrain, "resolution");
  terrain_shdr_locs.time = GetShaderLocation(shdrTerrain, "time");

  SetShaderValue(shdrTerrain, terrain_shdr_locs.resolution, &(resolution), RL_SHADER_UNIFORM_VEC2);

  // Generate heightmap image for terrain
  Image heightmap_img = GenImagePerlinNoise(256, 256, 0, 0, 5.0f);    // Load heightmap image (RAM)
  Texture2D height_texture = LoadTextureFromImage(heightmap_img);    
  
  // Create terrain mesh from heightmap
  Mesh terrain_mesh = GenMeshHeightmap(heightmap_img, Vector3{100.0f, 10.0f, 100.0f});
  state->terrain = LoadModelFromMesh(terrain_mesh);
  
  // Set terrain shader and texture
  state->terrain.materials[0].shader = shdrTerrain;
  state->terrain.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = height_texture;
  state->terrain.materials[0].maps[MATERIAL_MAP_METALNESS].texture = rocks_tex;
  state->terrain.materials[0].maps[MATERIAL_MAP_EMISSION].texture = grass_tex;
  state->terrain.materials[0].maps[MATERIAL_MAP_OCCLUSION].texture = snow_tex;
  shdrTerrain.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(shdrTerrain, "texture1");
  shdrTerrain.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shdrTerrain, "texture2");
  shdrTerrain.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(shdrTerrain, "texture3");
  
  // Clean up
  UnloadImage(heightmap_img);
  
  SetTargetFPS(60); 
	DisableCursor();

  // Use Customized function to create writable depth texture buffer
  RenderTexture2D target = LoadRenderTextureDepthTex(resolution.x, resolution.y);
  // Define the camera to look into our 3d world
  Camera camera = Camera { 
    Vector3{0.5f, 1.0f, 1.5f}, // POSITION
    Vector3{0.0f, 0.0f, 0.7f}, // TARGET
    Vector3{0.0f, 1.0f, 0.0f}, // UP
    90.0f, // FOV
    CAMERA_PERSPECTIVE
  };

	[[__maybe_unused__]] f32 delta_time = 0.f;
  [[__maybe_unused__]] Vector4 cloud_pos = Vector4(0.f, 10.f, 0.f);
	f32 elapsed_time = 0.f;
	Vector3 viewPos = Vector3 {0.f, 0.f, 0.f};
	Vector3 viewTarget = Vector3 {0.f, 0.f, 0.f};

  while (!WindowShouldClose())
  {
    UpdateCamera(&camera, CAMERA_FREE);
		delta_time = GetFrameTime();
		elapsed_time = GetTime();
		viewPos = Vector3 { camera.position.x, camera.position.y, camera.position.z };
		viewTarget = Vector3 { camera.target.x, camera.target.y, camera.target.z };

    // Camera FOV is pre-calculated in the camera distance
    f32 camDist = 1.0f / (tanf(camera.fovy * 0.5f * DEG2RAD));
    // Update Camera Looking Vector. Vector length determines FOV
    [[__maybe_unused__]] Vector3 viewDir = Vector3Scale(Vector3Normalize(Vector3Subtract(camera.target, camera.position)), camDist);

    SetShaderValue(shdrAtmosphere, atmosphere_shdr_locs.viewPos,    &(viewPos), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrAtmosphere, atmosphere_shdr_locs.viewTarget, &(viewTarget), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrAtmosphere, atmosphere_shdr_locs.time, 	    &(elapsed_time), RL_SHADER_UNIFORM_FLOAT);
    
    // Update terrain shader uniforms
    SetShaderValue(shdrTerrain, terrain_shdr_locs.viewPos, &(viewPos), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrTerrain, terrain_shdr_locs.viewTarget, &(viewTarget), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrTerrain, terrain_shdr_locs.time, &(elapsed_time), RL_SHADER_UNIFORM_FLOAT);
    
    SetShaderValue(shdr_map_obj, map_obj_shdr_locs.viewPos, &(viewPos), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdr_map_obj, map_obj_shdr_locs.viewCenter, &(viewTarget), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdr_map_obj, map_obj_shdr_locs.time, 	 &(elapsed_time), RL_SHADER_UNIFORM_FLOAT);

    //----------------------------------------------------------------------------------
    BeginTextureMode(target);
		{
    	ClearBackground(WHITE);
    	rlEnableDepthTest();

    	BeginShaderMode(shdrAtmosphere); 
      {
				DrawRectangleRec(Rectangle{0.f, 0.f,  resolution.x, resolution.y}, WHITE);
			}
    	EndShaderMode();

    	BeginMode3D(camera);
			{
				//draw_guide_plane();
  
        // Draw the terrain
        DrawModel(state->terrain, Vector3{0.0f, -5.0f, 0.0f}, 1.0f, WHITE);
			}
    	EndMode3D();
      
		}
    EndTextureMode();
    //----------------------------------------------------------------------------------

    BeginDrawing();
      ClearBackground(RAYWHITE);
      DrawTextureRec(target.texture, Rectangle{0, 0, resolution.x, -resolution.y}, Vector2{0.f, 0.f}, WHITE);
      DrawFPS(10, 10);
    EndDrawing();
  }

  UnloadRenderTextureDepthTex(target);
  UnloadShader(shdrAtmosphere);
  UnloadShader(shdrTiling);
  UnloadShader(shdrTerrain);
  UnloadModel(state->terrain);
  CloseWindow();
  return 0;
}

static RenderTexture2D LoadRenderTextureDepthTex(int width, int height) {
  RenderTexture2D target = {};

  target.id = rlLoadFramebuffer(); // Load an empty framebuffer

  if (target.id > 0) {
    rlEnableFramebuffer(target.id);

    // Create color texture (default to RGBA)
    target.texture.id = rlLoadTexture(0, width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    target.texture.mipmaps = 1;

    // Create depth texture buffer (instead of raylib default renderbuffer)
    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19; // DEPTH_COMPONENT_24BIT?
    target.depth.mipmaps = 1;

    // Attach color texture and depth texture to FBO
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    // Check if fbo is complete with attachments (valid)
    if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

    rlDisableFramebuffer();
  } else
  
	TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

  return target;
}
static void UnloadRenderTextureDepthTex(RenderTexture2D target) {
  if (target.id > 0) {
    // Color texture attached to FBO is deleted
    rlUnloadTexture(target.texture.id);
    rlUnloadTexture(target.depth.id);

    // NOTE: Depth texture is automatically
    // queried and deleted before deleting framebuffer
    rlUnloadFramebuffer(target.id);
  }
}
void draw_guide_plane(void) {
	DrawModel(state->guide_plane, Vector3 {0.f, 0.f, 0.f}, 2.f, WHITE);

	// X Axis RED
	DrawLine3D(Vector3 { 20.f, 0.f, 0.f}, Vector3 {-20.f, 0.f, 0.f}, RED);

	// Z Axis GREEN
	DrawLine3D(Vector3 { 0.f, 0.f, 20.f}, Vector3 {0.f, 0.f, -20.f}, GREEN);
}

const char * rsrc(const char * file_name) {
  return TextFormat("%s%s", SHADER_FILE, file_name);
}

const char * rterr(const char * file_name) {
  return TextFormat("%s%s", TERRAIN_FILE, file_name);
}

