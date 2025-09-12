#include "raylib.h"

#include "raymath.h"
#include "rlgl.h"
#include <math.h> // Required for: tanf()

#include "defines.h"

#include <core/fmemory.h>

#define GLSL_VERSION 330
#define RESOURCE_FILE "../app/custom_resources/"

typedef struct raymarch_locs {
  unsigned int camPos;
  unsigned int camDir;
  unsigned int screenCenter;
  unsigned int time;
} raymarch_locs;

typedef struct map_obj_locs {
  unsigned int time;
  unsigned int viewPos;
  unsigned int viewCenter;
  unsigned int resolution;
} map_obj_locs;

typedef struct cloud_cube_locs {
  unsigned int time;
  unsigned int viewPos;
  unsigned int viewDir;
  unsigned int resolution;
  unsigned int uNoise;
} cloud_cube_locs;

typedef struct main_system_state {
	Model guide_plane;
	Model cloud_cube;
} main_system_state;
static main_system_state * state = nullptr;

// Load custom render texture, create a writable depth texture buffer
static RenderTexture2D LoadRenderTextureDepthTex(int width, int height);
static void UnloadRenderTextureDepthTex(RenderTexture2D target);
void draw_guide_plane(void);
const char * rsrc(const char * file_name);

int main(void) {
	memory_system_initialize();
	state = (main_system_state*)allocate_memory_linear(sizeof(main_system_state), true);

  const Vector2 resolution = Vector2 { 1280.f, 720.f };
  InitWindow(resolution.x, resolution.y, "raylib [shaders] example - hybrid render");

	Image checker_image = GenImageChecked(1024, 1024, 512, 512, Color {128, 142, 155, 255}, Color {72, 84, 96, 255});
	Texture checker_texture = LoadTextureFromImage(checker_image);
  //Image noise_img = GenImagePerlinNoise(1024, 1024, 25, 25, 5.f);
  //Texture noise_tex = LoadTextureFromImage(noise_img);

	Shader shdr_map_obj = LoadShader(0, TextFormat(rsrc("map_objects.fs"), GLSL_VERSION));
  map_obj_locs map_obj_shdr_locs;
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

  Shader shdrCloud = LoadShader(0, TextFormat(rsrc("cloud_cube.fs"), GLSL_VERSION));
  cloud_cube_locs cloud_shdr_locs;
  Vector3 cloud_cube_dims = Vector3 {20.f, 1.f, 20.f};
  {
	  Mesh cloud_cube = GenMeshCube(cloud_cube_dims.x, cloud_cube_dims.y, cloud_cube_dims.z);
	  state->cloud_cube = LoadModelFromMesh(cloud_cube);
    cloud_shdr_locs.time = GetShaderLocation(shdrCloud, "time");
    cloud_shdr_locs.viewPos = GetShaderLocation(shdrCloud, "viewPos");
    cloud_shdr_locs.viewDir = GetShaderLocation(shdrCloud, "viewDir");
    cloud_shdr_locs.resolution = GetShaderLocation(shdrCloud, "resolution");
    cloud_shdr_locs.uNoise = GetShaderLocation(shdrCloud, "uNoise");
    
	  state->cloud_cube.materials[0].shader = shdrCloud;
	  state->cloud_cube.materials[0].maps[0].texture = checker_texture;
    SetShaderValue(shdrCloud, cloud_shdr_locs.resolution, &(resolution), RL_SHADER_UNIFORM_VEC2);
    //SetShaderValueTexture(shdrCloud, cloud_shdr_locs.uNoise, noise_tex);
  }

  Shader shdrRaymarch = LoadShader(0, TextFormat(rsrc("raymarch.fs"), GLSL_VERSION));
  raymarch_locs marchLocs;
  {
    // This Shader calculates pixel depth and color using raymarch
    marchLocs.camPos = GetShaderLocation(shdrRaymarch, "camPos");
    marchLocs.camDir = GetShaderLocation(shdrRaymarch, "camDir");
    marchLocs.screenCenter = GetShaderLocation(shdrRaymarch, "screenCenter");
    marchLocs.time = GetShaderLocation(shdrRaymarch, "time");

    // Transfer screenCenter position to shader. Which is used to calculate ray
    // direction
    Vector2 screen_center = Vector2 {resolution.x * .5f, resolution.y * .5f};
    SetShaderValue(shdrRaymarch, marchLocs.screenCenter, &screen_center, SHADER_UNIFORM_VEC2);
  }

  SetTargetFPS(60); 
	DisableCursor();

  // Use Customized function to create writable depth texture buffer
  RenderTexture2D target = LoadRenderTextureDepthTex(resolution.x, resolution.y);
  // Define the camera to look into our 3d world
  Camera camera = { Vector3{0.5f, 1.0f, 1.5f}, Vector3{0.0f, 0.5f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE};

	[[__maybe_unused__]] f32 delta_time = 0.f;
  [[__maybe_unused__]] Vector4 cloud_pos = Vector4(0.f, 10.f, 0.f);
	f32 elapsed_time = 0.f;
	Vector3 viewPos = Vector3 {0.f, 1.f, -3.f};
	Vector3 viewCenter = Vector3 {0.f, 0.f, 0.f};

  while (!WindowShouldClose())
  {
    UpdateCamera(&camera, CAMERA_FREE);
		delta_time = GetFrameTime();
		elapsed_time = GetTime();
		viewPos = Vector3 { camera.position.x, camera.position.y, camera.position.z };
		viewCenter = Vector3 { camera.target.x, camera.target.y, camera.target.z };

    // Camera FOV is pre-calculated in the camera distance
    f32 camDist = 1.0f / (tanf(camera.fovy * 0.5f * DEG2RAD));
    // Update Camera Looking Vector. Vector length determines FOV
    Vector3 viewDir = Vector3Scale(Vector3Normalize(Vector3Subtract(camera.target, camera.position)), camDist);

    SetShaderValue(shdrRaymarch, marchLocs.camPos, &(viewPos), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrRaymarch, marchLocs.camDir, &(viewDir), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrRaymarch, marchLocs.time, &(elapsed_time), RL_SHADER_UNIFORM_FLOAT);

    SetShaderValue(shdrCloud, cloud_shdr_locs.viewPos, &(viewPos), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrCloud, cloud_shdr_locs.viewDir, &(viewDir), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrCloud, cloud_shdr_locs.time, 	 &(elapsed_time), RL_SHADER_UNIFORM_FLOAT);
    
    SetShaderValue(shdr_map_obj, map_obj_shdr_locs.viewPos, &(viewPos), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdr_map_obj, map_obj_shdr_locs.viewCenter, &(viewCenter), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdr_map_obj, map_obj_shdr_locs.time, 	 &(elapsed_time), RL_SHADER_UNIFORM_FLOAT);

    //----------------------------------------------------------------------------------
    BeginTextureMode(target);
		{
    	ClearBackground(WHITE);
    	rlEnableDepthTest();

    	BeginShaderMode(shdrCloud); 
      {
				DrawRectangleRec(Rectangle{0.f, 0.f,  resolution.x, resolution.y}, WHITE);
			}
    	EndShaderMode();

    	BeginMode3D(camera);
			{
				//draw_guide_plane();

        
    	  //BeginShaderMode(shdr_map_obj); 
        //{
        //  //DrawCubeWires(Vector3 {cube_pos.x, cube_pos.y, cube_pos.z}, cloud_cube_rad * 2.f, cloud_cube_rad * 2.f, cloud_cube_rad * 2.f, BLACK);
			  //}
    	  //EndShaderMode();

    	  //BeginShaderMode(shdrCloud); 
        //{
        //  //SetShaderValueTexture(shdrCloud, cloud_shdr_locs.uNoise, noise_tex);
        //  DrawModel(state->cloud_cube, Vector3 {cloud_pos.x, cloud_pos.y, cloud_pos.z, }, 1.f, WHITE);
			  //}
    	  //EndShaderMode();
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
  UnloadShader(shdrRaymarch);
  UnloadShader(shdrCloud);
  UnloadShader(shdrTiling);
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
  return TextFormat("%s%s", RESOURCE_FILE, file_name);
}

//BeginShaderMode(shdr_ray_obj); 
//{
//}
//EndShaderMode();
