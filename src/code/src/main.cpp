#include "window.h"
#include "terrainwidget.h"
#include "scalarfield2.h"
#include "gpu_shader.h"
#include "texture.h"
#include "write_16_png.h"
#include <imgui.h>

static Window* window;
static TerrainRaytracingWidget* widget;
static ScalarField2 hf;
static ScalarField2 gpu_drainage;
static GPU_Erosion gpu_e;
static GPU_Thermal gpu_t;
static GPU_Deposition gpu_d;
static Texture2D albedoTexture;
static int shadingMode;


static bool m_run_erosion     = false;
static bool m_run_thermal     = false;
static bool m_run_deposition  = false;

static bool m_init_erosion    = false;
static bool m_init_thermal    = false;
static bool m_init_deposition = false;

static GLuint m_terrain_buffer = 0;


/*!
\brief Reset the camera around a given 3D box.
*/
static void ResetCamera()
{
	Box2 box = hf.GetBox();
	Vector2 v = 0.5 * box.Diagonal();
	Camera cam = Camera(box.Center().ToVector(0.0) - (2.0 * v).ToVector(-Norm(v)), box.Center().ToVector(0), Vector(0.0, 0.0, 1.0), 1.0, 1.0, 5.0, 10000);
	widget->SetCamera(cam);
}

/*!
\brief Load the scalarfield hf on the gpu.
*/
static void LoadTerrain() {
	widget->SetHeightField(&hf);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_terrain_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * (hf.GetSizeX() * hf.GetSizeY()), hf.GetFloatData().data(), GL_STREAM_READ);
	widget->SetTerrainBuffer(m_terrain_buffer);
}

/*!
\brief Get the gpu data on the cpu hf.
*/
static void GetTerrain() {
	std::vector<float> tmpData;
	tmpData.resize(hf.GetSizeX() * hf.GetSizeY());
	glGetNamedBufferSubData(m_terrain_buffer, 0, sizeof(float) * (hf.GetSizeX() * hf.GetSizeY()), tmpData.data());
	for (int i = 0; i < hf.GetSizeX() * hf.GetSizeY(); i++)
		hf[i] = double(tmpData[i]);
}

/*!
\brief Premade calls to erosion/deposition/stabilization + subdivision calls for presets.
*/
static void PredefinedErosion() {
	// 256
	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(3000);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(600);

	gpu_d.Init(hf, gpu_t.GetTerrainGLuint());
	gpu_d.Step(2000);

	// 512
	GetTerrain();
	hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);
	LoadTerrain();

	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(1500);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(1000);

	gpu_d.Init(hf, gpu_t.GetTerrainGLuint());
	gpu_d.Step(700);

	// 1024
	GetTerrain();
	hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);
	LoadTerrain();

	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(700);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(2000);

	gpu_d.Init(hf, gpu_t.GetTerrainGLuint());
	gpu_d.Step(200);

	// 2048
	GetTerrain();
	hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);
	LoadTerrain();

	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(400);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(6000);

	gpu_d.Init(hf, gpu_t.GetTerrainGLuint());
	gpu_d.Step(150);
}

/*!
\brief User interface for the application.
*/
static void GUI()
{
	// Menu bar
	static bool newScene = false;
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save heightfield"))
			{
				//write_16_png("saved.png");
				hf.Save("saved.png");
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::Begin("Menu", 0, ImGuiWindowFlags_AlwaysAutoResize);
	{
		// Hardcoded examples
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Base terrains");
			if (ImGui::Button("Noise")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/noise.png", 0., 3000.);
				LoadTerrain();
				widget->initializeGL();
				ResetCamera();
			}
			ImGui::SameLine();
			if (ImGui::Button("Mountains")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/mountains.png", 0., 3000.);
				LoadTerrain();
				widget->initializeGL();
				ResetCamera();
			}
			ImGui::SameLine();
			if (ImGui::Button("New Zealand")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/new_zealand.png", 0., 3200.);
				LoadTerrain();
				widget->initializeGL();
				ResetCamera();
			}
		}

		// Actions
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Results");
			if (ImGui::Button("Result #1")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/noise.png", 0., 3000.);
				LoadTerrain();

				PredefinedErosion();

				GetTerrain();
				widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
				widget->initializeGL();

				ResetCamera();
			}
			ImGui::SameLine();

			if (ImGui::Button("Result #2")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/mountains.png", 0., 3000.);
				LoadTerrain();

				PredefinedErosion();

				GetTerrain();
				widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
				widget->initializeGL();

				ResetCamera();
			}
			ImGui::SameLine();

			if (ImGui::Button("Result #3")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/new_zealand.png", 0., 3200.);
				LoadTerrain();

				PredefinedErosion();

				GetTerrain();
				widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
				widget->initializeGL();

				ResetCamera();
			}
			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		}

		{
			ImGui::Spacing(); ImGui::Spacing();
			if (ImGui::Button("Reset Camera"))
				ResetCamera();
		}

		{
			ImGui::Text("Erosion");
			ImGui::Checkbox("Run E", &m_run_erosion);
		}
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		{
			ImGui::Text("Thermal");
			ImGui::Checkbox("Run T", &m_run_thermal);
		}
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		{
			ImGui::Text("Deposition");
			ImGui::Checkbox("Run D", &m_run_deposition);
		}
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		{
			if (ImGui::Button("x2 upsampling")) {

				// x2 upsampling
				GetTerrain();
				hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);
				LoadTerrain();

				widget->initializeGL();

				ResetCamera();

				m_init_erosion = false;
				m_init_thermal = false;
				m_init_deposition = false;
			}
		}
		

		// Simulation statistics
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Statistics");
			ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / float(ImGui::GetIO().Framerate), float(ImGui::GetIO().Framerate));
			std::string size_stat = std::string("Current terrain size: ") + std::to_string(hf.GetSizeX()) + " x " + std::to_string(hf.GetSizeY());
			ImGui::Text(size_stat.c_str());
		}
	}
	ImGui::End();
}

int main()
{
	// Init
	window = new Window("Multi Scale Erosion", 1920, 1080);
	widget = new TerrainRaytracingWidget();
	window->SetUICallback(GUI);

	// buffer init
	glGenBuffers(1, &m_terrain_buffer);

	hf = ScalarField2(Box2(Vector2::Null, 15 * 1000), "heightfields/noise.png", 0.0, 4000.0);
	LoadTerrain();
	window->SetWidget(widget);

	ResetCamera();


	// Main loop
	while (!window->Exit()) {
		if (m_run_erosion) {
			if (!m_init_erosion) gpu_e.Init(hf, m_terrain_buffer);
			m_init_erosion = true;
			m_init_thermal = false;
			m_init_deposition = false;

			gpu_e.Step(100);

			widget->SetTerrainBuffer(gpu_e.GetTerrainGLuint());
			widget->UpdateInternal();

		} else if (m_run_thermal) {
			if (!m_init_thermal) gpu_t.Init(hf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = true;
			m_init_deposition = false;

			gpu_t.Step(200);

			widget->SetTerrainBuffer(gpu_t.GetTerrainGLuint());
			widget->UpdateInternal();

		} else if (m_run_deposition) {
			if (!m_init_deposition) gpu_d.Init(hf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = false;
			m_init_deposition = true;

			gpu_d.Step(50);

			widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
			widget->UpdateInternal();
		}


		window->Update();
	}

	return 0;
}
