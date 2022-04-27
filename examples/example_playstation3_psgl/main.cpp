
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_playstation3.h"
#include "imgui/backends/imgui_impl_psgl.h"

#include <math.h>
#include <stdio.h>
#include <PSGL/psgl.h>

#include <sys/spu_initialize.h>
#include <sys/paths.h>

#include <sysutil/sysutil_sysparam.h>  // used for cellVideoOutGetResolutionAvailability() and videoOutIsReady()

void initGraphics(PSGLdevice* device)
{
   // get render target buffer dimensions and set viewport
   GLuint renderWidth, renderHeight;
   psglGetRenderBufferDimensions(device, &renderWidth, &renderHeight);

   glViewport(0, 0, renderWidth, renderHeight);

   // get display aspect ratio (width / height) and set projection
   // (it is important to use this value and NOT renderWidth/renderHeight since
   // pixel ratios do not necessarily match the 16/9 or 4/3 display aspect ratios)
   GLfloat aspectRatio = psglGetDeviceAspectRatio(device);

   float l = aspectRatio, r = -l, b = -1, t = 1;
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glOrthof(l, r, b, t, 0, 1);

   glClearColor(0.f, 0.f, 0.f, 1.f);
   glDisable(GL_CULL_FACE);

   // PSGL doesn't clear the screen on startup, so let's do that here.
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
   psglSwap();
}

// Given an priority ordered array of desired resolutions (from most desired to least), chooses
// the first mode that is available on the current display device.
//
// The list of modes are chosen from the following standard modes:
//   
//   CELL_VIDEO_OUT_RESOLUTION_480   (720x480)
//   CELL_VIDEO_OUT_RESOLUTION_576   (720x576)
//   CELL_VIDEO_OUT_RESOLUTION_720   (1280x720)
//   CELL_VIDEO_OUT_RESOLUTION_1080  (1920x1080)
//
// or these modes that allow a lower resolution buffer to automatically be 
// upscaled (in hardware) to the 1920x1080 standard:
//
//   CELL_VIDEO_OUT_RESOLUTION_1600x1080
//   CELL_VIDEO_OUT_RESOLUTION_1440x1080
//   CELL_VIDEO_OUT_RESOLUTION_1280x1080
//   CELL_VIDEO_OUT_RESOLUTION_960x1080
//
// if none of the desired resolutions are available, 0 is returned
//
// example, choose 1920x1080 if possible, but otherwise try for 1280x720:
//
//   unsigned int resolutions[] = { CELL_VIDEO_OUT_RESOLUTION_1080, CELL_VIDEO_OUT_RESOLUTION_720 };
//   int numResolutions = 2;
static int chooseBestResolution(const unsigned int* resolutions, unsigned int numResolutions)
{
   unsigned int bestResolution = 0;
   for (unsigned int i = 0; bestResolution == 0 && i < numResolutions; i++)
      if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resolutions[i], CELL_VIDEO_OUT_ASPECT_AUTO, 0))
         bestResolution = resolutions[i];
   return bestResolution;
}

// Given one of the valid video resolution IDs, assigns the associated dimensions in w and h.
// If the video resolution ID is invalid, 0 is returned, 1 if valid
static int getResolutionWidthHeight(const unsigned int resolutionId, unsigned int& w, unsigned int& h)
{
   switch (resolutionId)
   {
   case CELL_VIDEO_OUT_RESOLUTION_480: w = 720;  h = 480;  return(1);
   case CELL_VIDEO_OUT_RESOLUTION_576: w = 720;  h = 576;  return(1);
   case CELL_VIDEO_OUT_RESOLUTION_720: w = 1280; h = 720;  return(1);
   case CELL_VIDEO_OUT_RESOLUTION_1080: w = 1920; h = 1080; return(1);
   case CELL_VIDEO_OUT_RESOLUTION_1600x1080: w = 1600; h = 1080; return(1);
   case CELL_VIDEO_OUT_RESOLUTION_1440x1080: w = 1440; h = 1080; return(1);
   case CELL_VIDEO_OUT_RESOLUTION_1280x1080: w = 1280; h = 1080; return(1);
   case CELL_VIDEO_OUT_RESOLUTION_960x1080: w = 960;  h = 1080; return(1);
   };
   printf("getResolutionWidthHeight: resolutionId %d not a valid video mode\n", resolutionId);
   return(0);
}

// Checks if the video output device is ready for initialization by psglInit.
// Call this before calling psglInit, until it returns true. This is mainly used to make
// sure HDMI devices are turned on and connected before calling psglInit. psglInit
// will busy wait until the device is ready, so repeatedly calling this allows 
// processing while waiting. For non-HDMI devices, this routine always returns true.
bool videoOutIsReady()
{
	CellVideoOutState videoState;
	cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);
	return(videoState.state == CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED);
}

/**
 * This is our sysutil callback function, that responds to exit requests from
 * the system.  In this implementation, the userdata pointer points to a boolean
 * which will be flagged as true to indicate that the application needs to exit.
 * We need to register this function and poll for callbacks periodically.
 */
void sysutilCallback(uint64_t uiStatus, uint64_t uiParam, void* pvUserData)
{
   // For unused parameter warnings
   (void)uiParam;

   switch (uiStatus)
   {
   case CELL_SYSUTIL_REQUEST_EXITGAME:
      *((bool*)pvUserData) = true;
      break;
   default:
      break;
   }
}

int main()
{
   // Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
   sys_spu_initialize(6, 1);

   // Check if the video output device is ready BEFORE calling psglInit.
   // This will make sure HDMI devices are turned on and connected, and allow
   // background processing while waiting (psglInit will busy wait until ready).
   // Non-HDMI video out is always "ready", so they pass through this loop trivially.
   while (!videoOutIsReady())
   {
      // do your background processing here until the video is ready
      printf("video not ready!\n");
   };
   printf("VIDEO READY!\n");

   PSGLinitOptions initOpts =
   {
     PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
     1,
     false,
     0,
     0,
     0,
     0,
     128 * 1024 * 1024,  // 128 mbs for host memory 
   };

   psglInit(&initOpts);

   // (1) create array of all desired resolutions in priority order (most desired to least).
   //     In this example, we choose 1080p if possible, then 960x1080-to-1920x1080 horizontal scaling,
   //     and in the worst case, 720p.
   const unsigned int resolutions[] = { CELL_VIDEO_OUT_RESOLUTION_1080, CELL_VIDEO_OUT_RESOLUTION_960x1080, CELL_VIDEO_OUT_RESOLUTION_720 };
   const int numResolutions = sizeof(resolutions) / sizeof(resolutions[0]);

   // (2) loop through the modes and grab the first available
   int bestResolution = chooseBestResolution(resolutions, numResolutions);

   // (3) get the chosen video mode's pixel dimensions
   unsigned int deviceWidth = 0, deviceHeight = 0;
   getResolutionWidthHeight(bestResolution, deviceWidth, deviceHeight);

   // (3) if desired resolution is available, create the PSGL device and context
   if (bestResolution)
   {
      printf("%d x %d is available...\n", deviceWidth, deviceHeight);

      // (4) create the PSGL device based on the selected resolution mode
      PSGLdeviceParameters params;
      params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT | PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT | PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
      params.colorFormat = GL_ARGB_SCE;
      params.depthFormat = GL_DEPTH_COMPONENT24;
      params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

      params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
      params.width = deviceWidth;
      params.height = deviceHeight;

      PSGLdevice* device = psglCreateDeviceExtended(&params);

      // (5) create context
      PSGLcontext* context = psglCreateContext();
      psglMakeCurrent(context, device);
      psglResetCurrentContext();

      // Init PSGL and draw
      initGraphics(device);




      // Setup Dear ImGui context
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO& io = ImGui::GetIO(); (void)io;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
      io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
      io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
      //io.ConfigViewportsNoAutoMerge = true;
      //io.ConfigViewportsNoTaskBarIcon = true;

      // Setup Dear ImGui style
      ImGui::StyleColorsDark();
      //ImGui::StyleColorsClassic();

      // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
      ImGuiStyle& style = ImGui::GetStyle();
      if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
      {
         style.WindowRounding = 0.0f;
         style.Colors[ImGuiCol_WindowBg].w = 1.0f;
      }

      // Setup Platform/Renderer backends
      ImGui_ImplPlaystation3_Init();
      ImGui_ImplPSGL_Init();

      // Load Fonts
      // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
      // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
      // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
      // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
      // - Read 'docs/FONTS.md' for more instructions and details.
      // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
      //io.Fonts->AddFontDefault();
      //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
      //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
      //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
      //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
      //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
      //IM_ASSERT(font != NULL);

      // Our state
      bool show_demo_window = true;
      bool show_another_window = false;
      ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

      // Main loop
      bool done = false;

      int callbackRet = cellSysutilRegisterCallback(0, sysutilCallback, &done);

      while (!done)
      {
         // Poll sysutil to see if it has any callbacks to process
         callbackRet = cellSysutilCheckCallback();

         if (callbackRet < CELL_OK)
         {
            fprintf(stderr, "Error checking for sysutil callbacks: %i\n", callbackRet);
            exit(-1);
            break;
         }

         // Start the Dear ImGui frame
         ImGui_ImplPSGL_NewFrame();
         ImGui_ImplPlaystation3_NewFrame();
         ImGui::NewFrame();

         // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
         if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

         // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
         {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
               counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
         }

         // 3. Show another simple window.
         if (show_another_window)
         {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
               show_another_window = false;
            ImGui::End();
         }

         // Rendering
         ImGui::Render();
         glViewport(0, 0, deviceWidth, deviceHeight);
         glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
         glClear(GL_COLOR_BUFFER_BIT);
         ImGui_ImplPSGL_RenderDrawData(ImGui::GetDrawData());

         psglSwap();
      }


      // Cleanup
      ImGui_ImplPSGL_Shutdown();
      ImGui_ImplPlaystation3_Shutdown();
      ImGui::DestroyContext();

      // Destroy the context, then the device (before psglExit)
      psglDestroyContext(context);
      psglDestroyDevice(device);
   }
   else
   {
      printf("%d x %d is NOT available...\n", deviceWidth, deviceHeight);
   }

   psglExit();
}