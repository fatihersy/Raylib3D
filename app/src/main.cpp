#include "raylib.h"

#include "raymath.h"
#include "rlgl.h"
#include <math.h> // Required for: tanf()

#include "defines.h"

#include <core/fmemory.h>

#define GLSL_VERSION 330

#define ZEROVEC3 (Vector3 {0.f, 0.f, 0.f})

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef struct {
  unsigned int camPos;
  unsigned int camDir;
  unsigned int screenCenter;
  unsigned int delta_time;
} raymarch_locs;

typedef struct main_system_state {
	Model guide_plane;
	Model cloud_cube;
} main_system_state;

static main_system_state * state = nullptr;

//------------------------------------------------------------------------------------
// Module Functions Declaration
//------------------------------------------------------------------------------------
// Load custom render texture, create a writable depth texture buffer
static RenderTexture2D LoadRenderTextureDepthTex(int width, int height);
// Unload render texture from GPU memory (VRAM)
static void UnloadRenderTextureDepthTex(RenderTexture2D target);

void draw_guide_plane(void);

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
	memory_system_initialize();

	state = (main_system_state*)allocate_memory_linear(sizeof(main_system_state), true);

  const int screenWidth = 1280;
  const int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "raylib [shaders] example - hybrid render");

  // This Shader calculates pixel depth and color using raymarch
  Shader shdrRaymarch = LoadShader(0, TextFormat("resources/shaders/glsl%i/hybrid_raymarch.fs", GLSL_VERSION));
	Image checker_image = GenImageChecked(1024, 1024, 512, 512, Color {128, 142, 155, 255}, Color {72, 84, 96, 255});
	Texture checker_texture = LoadTextureFromImage(checker_image);

	Mesh plane_mesh = GenMeshPlane(20.f, 20.f, 1.f, 1.f);
	state->guide_plane = LoadModelFromMesh(plane_mesh);

	Mesh cloud_cube = GenMeshCube(2.f, 2.f, 2.f);
	state->cloud_cube = LoadModelFromMesh(cloud_cube);

	state->guide_plane.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = checker_texture;

  // This Shader is a standard rasterization fragment shader with the addition
  // of depth writing You are required to write depth for all shaders if one
  // shader does it
  Shader shdrRaster = LoadShader(0, TextFormat("resources/shaders/glsl%i/hybrid_raster.fs", GLSL_VERSION));
	Shader shdrTiling = LoadShader(0, TextFormat("resources/shaders/glsl%i/tiling.fs", GLSL_VERSION));

	std::array<f32, 2> tiling = std::array<f32, 2>({ 10.0f, 10.0f });
	SetShaderValue(shdrTiling, GetShaderLocation(shdrTiling, "tiling"), tiling.data(), SHADER_UNIFORM_VEC2);
  state->guide_plane.materials[0].shader = shdrTiling;

  // Declare Struct used to store camera locs
  raymarch_locs marchLocs = {};

  // Fill the struct with shader locs
  marchLocs.camPos = GetShaderLocation(shdrRaymarch, "camPos");
  marchLocs.camDir = GetShaderLocation(shdrRaymarch, "camDir");
  marchLocs.screenCenter = GetShaderLocation(shdrRaymarch, "screenCenter");
  marchLocs.delta_time = GetShaderLocation(shdrRaymarch, "delta_time");

  // Transfer screenCenter position to shader. Which is used to calculate ray
  // direction
  Vector2 screenCenter = {.x = screenWidth / 2.0f, .y = screenHeight / 2.0f};
  SetShaderValue(shdrRaymarch, marchLocs.screenCenter, &screenCenter, SHADER_UNIFORM_VEC2);

  // Use Customized function to create writable depth texture buffer
  RenderTexture2D target = LoadRenderTextureDepthTex(screenWidth, screenHeight);

  // Define the camera to look into our 3d world
  Camera camera = {
    .position = Vector3{0.5f, 1.0f, 1.5f}, // Camera position
    .target = Vector3{0.0f, 0.5f, 0.0f},   // Camera looking at point
    .up = Vector3{0.0f, 1.0f, 0.0f}, // Camera up vector (rotation towards target)
    .fovy = 45.0f,       // Camera field-of-view Y
    .projection = CAMERA_PERSPECTIVE // Camera projection type
  };

  // Camera FOV is pre-calculated in the camera distance
  float camDist = 1.0f / (tanf(camera.fovy * 0.5f * DEG2RAD));

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

	DisableCursor();

	f32 delta_time = 0.f;

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    UpdateCamera(&camera, CAMERA_FREE);
		delta_time = GetFrameTime();

    // Update Camera Postion in the ray march shader
    SetShaderValue(shdrRaymarch, marchLocs.camPos, &(camera.position), RL_SHADER_UNIFORM_VEC3);

    // Update Camera Looking Vector. Vector length determines FOV
    Vector3 camDir = Vector3Scale(Vector3Normalize(Vector3Subtract(camera.target, camera.position)), camDist);
    SetShaderValue(shdrRaymarch, marchLocs.camDir, &(camDir), RL_SHADER_UNIFORM_VEC3);
    SetShaderValue(shdrRaymarch, marchLocs.delta_time, &(delta_time), RL_SHADER_UNIFORM_FLOAT);
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    // Draw into our custom render texture (framebuffer)
    BeginTextureMode(target);
		{
    	ClearBackground(WHITE);

    	// Raymarch Scene
    	rlEnableDepthTest(); // Manually enable Depth Test to handle multiple rendering methods
    	BeginShaderMode(shdrRaymarch);
			{
				DrawRectangleRec(Rectangle{0, 0, (float)screenWidth, (float)screenHeight}, WHITE);
			}
    	EndShaderMode();

    	// Rasterize Scene
    	BeginMode3D(camera);
			{
				BeginShaderMode(shdrRaster);
				{
					DrawModel(state->cloud_cube, Vector3 {0.f, 1.f, 0.f}, 1.f, WHITE);
				}
				EndShaderMode();

				draw_guide_plane();
			}
    	EndMode3D();
		}

    EndTextureMode();

    // Draw into screen our custom render texture
    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawTextureRec(target.texture, Rectangle{0, 0, (float)screenWidth, (float)-screenHeight}, Vector2{0, 0}, WHITE);
    DrawFPS(10, 10);
    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  UnloadRenderTextureDepthTex(target);
  UnloadShader(shdrRaymarch);
  UnloadShader(shdrRaster);

  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}

//------------------------------------------------------------------------------------
// Module Functions Definition
//------------------------------------------------------------------------------------
// Load custom render texture, create a writable depth texture buffer
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
// Unload render texture from GPU memory (VRAM)
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
