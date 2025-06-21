#include "window.h"
#include "terrainwidget.h"
#include "scalarfield2.h"
#include "gpu_shader.h"
#include "texture.h"
#include "write_16_png.h"
#include "ImFileDialog.h"
#include <imgui.h>

#include "lodepng.h"
#include "stb_image.h"
#include "stb_image_write.h"

static Window* window;
static TerrainRaytracingWidget* widget;
static ScalarField2 hf;
static ScalarField2 siltf;
static ScalarField2 sandf;
static ScalarField2 clayf;
static ScalarField2 depthf;
static ScalarField2 gpu_drainage;
static GPU_Erosion gpu_e;
static GPU_Thermal gpu_t;
static GPU_Deposition gpu_d;
static GPU_SoilDeposition gpu_ds;
static Texture2D albedoTexture;
static int shadingMode;

static GLuint m_satellite_texture = 0;
static GLuint m_input_soil_texture = 0;

static bool m_run_erosion     = false;
static bool m_run_thermal     = false;
static bool m_run_deposition  = false;
static bool m_run_soil_deposition = false;

static bool m_init_erosion    = false;
static bool m_init_thermal    = false;
static bool m_init_deposition = false;
static bool m_init_soil_deposition = false;

static GLuint m_terrain_buffer = 0;

void get_soil_texture(bool fetch = false);
void load_soil();
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

static void PredefinedSoilErosion() {
	// 256
	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(3000);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(600);

	gpu_ds.Init(hf, siltf, sandf, clayf, gpu_t.GetTerrainGLuint());
	gpu_ds.Step(20);

	// 512
	GetTerrain();
	get_soil_texture(true);
	hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);
	siltf = siltf.SetResolution(siltf.GetSizeX() * 2, siltf.GetSizeY() * 2, true);
	sandf = sandf.SetResolution(sandf.GetSizeX() * 2, sandf.GetSizeY() * 2, true);
	clayf = clayf.SetResolution(clayf.GetSizeX() * 2, clayf.GetSizeY() * 2, true);
	LoadTerrain();

	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(1500);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(1000);

	gpu_ds.Init(hf, siltf, sandf, clayf, gpu_t.GetTerrainGLuint());
	gpu_ds.Step(700);

	// 1024
	GetTerrain();
	get_soil_texture(true);
	hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);
	siltf = siltf.SetResolution(siltf.GetSizeX() * 2, siltf.GetSizeY() * 2, true);
	sandf = sandf.SetResolution(sandf.GetSizeX() * 2, sandf.GetSizeY() * 2, true);
	clayf = clayf.SetResolution(clayf.GetSizeX() * 2, clayf.GetSizeY() * 2, true);
	LoadTerrain();

	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(700);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(2000);

	// gpu_d.Init(hf, gpu_t.GetTerrainGLuint());
	gpu_ds.Init(hf, siltf, sandf, clayf, gpu_t.GetTerrainGLuint());
	gpu_ds.Step(200);

	// 2048
	GetTerrain();
	get_soil_texture(true);
	hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);;
	siltf = siltf.SetResolution(siltf.GetSizeX() * 2, siltf.GetSizeY() * 2, true);
	sandf = sandf.SetResolution(sandf.GetSizeX() * 2, sandf.GetSizeY() * 2, true);
	clayf = clayf.SetResolution(clayf.GetSizeX() * 2, clayf.GetSizeY() * 2, true);
	LoadTerrain();

	gpu_e.Init(hf, m_terrain_buffer);
	gpu_e.Step(400);

	gpu_t.Init(hf, gpu_e.GetTerrainGLuint());
	gpu_t.Step(6000);

	// gpu_d.Init(hf, gpu_t.GetTerrainGLuint());
	gpu_ds.Init(hf, siltf, sandf, clayf, gpu_t.GetTerrainGLuint());
	gpu_ds.Step(150);
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
			if (ImGui::MenuItem("Load DEM")) {
				ifd::FileDialog::Instance().Open("LoadDEM_Dialog", "Choose a DEM", "Image file (*.png;*.jpg;*.jpeg;){.png,.jpg,.jpeg,.bmp,.tga},.*", false, RESOURCE_DIR);
			}
			if (ImGui::MenuItem("Save DEM"))
			{
				ifd::FileDialog::Instance().Save("SaveDEM_Dialog", "Save current DEM", "", RESOURCE_DIR);
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
				load_soil();
				widget->initializeGL();
				ResetCamera();
			}
			ImGui::SameLine();
			if (ImGui::Button("Mountains")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/mountains.png", 0., 3000.);
				LoadTerrain();
				load_soil();
				widget->initializeGL();
				ResetCamera();
			}
			ImGui::SameLine();
			if (ImGui::Button("New Zealand")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/new_zealand.png", 0., 3200.);
				LoadTerrain();
				load_soil();
				widget->initializeGL();
				ResetCamera();
			}
			ImGui::SameLine();
			if (ImGui::Button("DEM")) {
				hf = ScalarField2(Box2(Vector2::Null, 128 * 100), "heightfields/dem_test.png", 0., 1280.);
				LoadTerrain();
				load_soil();
				widget->initializeGL();
				ResetCamera();
			}
		}
		if (ImGui::Button("Reset"))
		{

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
				hf = ScalarField2(Box2(Vector2::Null, 64 * 100), "heightfields/dem_test.png", 0., 3280.0 - 1996.0);
				load_soil();
				LoadTerrain();

				PredefinedSoilErosion();

				GetTerrain();
				widget->SetTerrainBuffer(gpu_ds.GetTerrainGLuint());
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
		ImGui::Separator();
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		{
			ImGui::Text("Soil Deposition");
			ImGui::Checkbox("Run SD", &m_run_soil_deposition);
		}
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		{
			if (ImGui::Button("x2 upsampling")) {

				// x2 upsampling
				GetTerrain();
				hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);;
				siltf = siltf.SetResolution(siltf.GetSizeX() * 2, siltf.GetSizeY() * 2, true);
				sandf = sandf.SetResolution(sandf.GetSizeX() * 2, sandf.GetSizeY() * 2, true);
				clayf = clayf.SetResolution(clayf.GetSizeX() * 2, clayf.GetSizeY() * 2, true);
				LoadTerrain();

				widget->initializeGL();

				ResetCamera();

				m_init_erosion = false;
				m_init_thermal = false;
				m_init_deposition = false;
				m_init_soil_deposition = false;
			}
			if (ImGui::Button("Shader mode")) {
				shadingMode = (shadingMode + 1) % 3;
				widget->SetShadingMode(shadingMode);
			}
			if (ImGui::Button("Run 1 step Soil Deposition")) {
				if (!m_init_soil_deposition) {
					gpu_ds.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
					m_init_soil_deposition = true;
					m_init_erosion = false;
					m_init_thermal = false;
					m_init_deposition = false;
				}
				gpu_ds.Step(1);
				get_soil_texture(true);
				widget->SetAlbedo(albedoTexture);

			}

		}
		if (ImGui::BeginTabBar("ImageTabs"))
		{

			if (ImGui::BeginTabItem("Input")) {
				ImGui::Image((ImTextureID) m_input_soil_texture, ImVec2(512, 512));
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Sim result"))
			{
				ImGui::Image((ImTextureID) widget->GetAlbedoID(), ImVec2(512, 512));
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Satellite reference"))
			{
				ImGui::Image((ImTextureID) m_satellite_texture, ImVec2(512, 512));
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		// ImGui::Image((ImTextureID) widget->GetAlbedoID(), ImVec2(512, 512));
		// ImGui::Image((ImTextureID) m_satellite_texture, ImVec2(512, 512));

		// Simulation statistics
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Statistics");
			ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / float(ImGui::GetIO().Framerate), float(ImGui::GetIO().Framerate));
			std::string size_stat = std::string("Current terrain size: ") + std::to_string(hf.GetSizeX()) + " x " + std::to_string(hf.GetSizeY());
			ImGui::Text(size_stat.c_str());
		}
	}
	ImGui::End();

	// File Dialog LOAD
	if (ifd::FileDialog::Instance().IsDone("LoadDEM_Dialog")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			std::string res = ifd::FileDialog::Instance().GetResult().u8string();
			hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), res.c_str(), 0., 3000., true);
			LoadTerrain();
			widget->initializeGL();
			ResetCamera();
		}
		ifd::FileDialog::Instance().Close();
	}

	// File Dialog SAVE
	if (ifd::FileDialog::Instance().IsDone("SaveDEM_Dialog")) {
		if (ifd::FileDialog::Instance().HasResult()) {
			std::string res = ifd::FileDialog::Instance().GetResult().u8string();
			GetTerrain();
			hf.Save((std::string(res) + ".png").c_str());
		}
		ifd::FileDialog::Instance().Close();
	}

}

void get_soil_texture(bool fetch)
{
	if (fetch) {
		gpu_ds.GetSoilData(siltf, sandf, clayf);
	}
	auto siltimg = siltf.CreateImage();
	auto sandimg = sandf.CreateImage();
	auto clayimg = clayf.CreateImage();

	int nx = siltimg.GetSizeX();
	int ny = siltimg.GetSizeY();
	auto colors = std::vector<Color8>(nx*ny);

	for (int i = 0; i < nx*ny; i++) {
		colors[i] = Color8(siltimg.Data()[i].r, sandimg.Data()[i].g, clayimg.Data()[i].b, 255);
	}
	albedoTexture = Texture2D(colors, nx, ny);
	widget->SetAlbedo(albedoTexture);
}

void load_soil()
{
	int n, nx, ny;
	std::string fullpath = std::string(RESOURCE_DIR) + "/heightfields/dem_test_sscd.png";
	unsigned short* raw_data = stbi_load_16(fullpath.c_str(), &nx, &ny, &n, 4);

	if (!raw_data) {
		std::cout << "Failed to load image: " << fullpath << std::endl;
		return;
	}

	std::vector<double> siltfield(nx*ny);
	std::vector<double> sandfield(nx*ny);
	std::vector<double> clayfield(nx*ny);
	std::vector<double> depthfield(nx*ny);

	// for (int row = 0; row < 2; row++) {
	// 	for (int col = 0; col < 2; col++) {
	// 		int pixel_index = (row * nx + col) * n;  // Base index for this pixel
	//
	// 		std::cout << "Pixel (" << row << "," << col << "): "
	// 				  << "Silt=" << raw_data[pixel_index + 0] << ", "
	// 				  << "Sand=" << raw_data[pixel_index + 1] << ", "
	// 				  << "Clay=" << raw_data[pixel_index + 2] << ", "
	// 				  << "Depth=" << raw_data[pixel_index + 3] << std::endl;
	// 	}
	// }
	std::cout << "Loaded " << nx << "x" << ny << " image with " << n << " channels" << std::endl;
	for (int i = 0; i < nx*ny; i++) {
		// Don't normalize! Pass raw percentage values if that's what the data contains
		// or normalize to the range that ScalarField2(box, nx, ny, vector) expects

		double silt = double(raw_data[i*n + 0]);

		// PNG stores 0-100 percentages in 16-bit format:
		siltfield[i] = double(raw_data[i*n + 0]) / 100.0;
		sandfield[i] = double(raw_data[i*n + 1]) / 100.0;
		clayfield[i] = double(raw_data[i*n + 2]) / 100.0;
		depthfield[i] = double(raw_data[i*n + 3]);

	}

	stbi_image_free(raw_data);

	siltf = ScalarField2(hf.GetBox(), nx, ny, siltfield);
	siltf = siltf.SetResolution(hf.GetSizeX(), hf.GetSizeY(), true);
	sandf = ScalarField2(hf.GetBox(), nx, ny, sandfield);
	sandf = sandf.SetResolution(hf.GetSizeX(), hf.GetSizeY(), true);
	clayf = ScalarField2(hf.GetBox(), nx, ny, clayfield);
	clayf = clayf.SetResolution(hf.GetSizeX(), hf.GetSizeY(), true);
	depthf = ScalarField2(hf.GetBox(), nx, ny, depthfield);
	depthf = depthf.SetResolution(hf.GetSizeX(), hf.GetSizeY(), true);

	std::cout << "Silt: " << siltf.GetSizeX() << ", " << siltf.GetSizeY() << std::endl;

	auto siltimg = siltf.CreateImage();
	auto sandimg = sandf.CreateImage();
	auto clayimg = clayf.CreateImage();

	auto colors = std::vector<Color8>(nx*ny);

	for (int i = 0; i < nx*ny; i++) {
		colors[i] = Color8(siltimg.Data()[i].r, sandimg.Data()[i].g, clayimg.Data()[i].b, 255);
	}
	albedoTexture = Texture2D(colors, nx, ny);
	std::string outpath = std::string(RESOURCE_DIR) + "/test.png";

	auto outcols = std::vector<unsigned char>(nx*ny*3);
	for (int i = 0; i < nx*ny; i++) {
		// outcols[i*3 + 0] = colors[i].r;
		outcols[i*3 + 0] = colors[i].r;
		outcols[i*3 + 1] = colors[i].g;
		outcols[i*3 + 2] = colors[i].b;
	}
	stbi_write_png(outpath.c_str(), nx, ny, 3, outcols.data(), 0);
	shadingMode = 1;
	widget->SetShadingMode(shadingMode);
}

void load_sat_png()
{
	std::string fullpath = std::string(RESOURCE_DIR) + "/satref.png";
	int nx, ny, n;
	unsigned char* raw_data = stbi_load(fullpath.c_str(), &nx, &ny, &n, 0);
	std::cout << "Loaded satellite texture with channels: " << n << std::endl;
	if (!raw_data) {
		std::cout << "Failed to load image: " << fullpath << std::endl;
		return;
	}


	glGenTextures(1, &m_satellite_texture);
	glBindTexture(GL_TEXTURE_2D, m_satellite_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// load 24 bit RGB data:
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, nx, ny, 0, GL_RGB, GL_UNSIGNED_BYTE, raw_data);

	glBindTexture(GL_TEXTURE_2D, 0);
	stbi_image_free(raw_data);

	std::cout << "Loaded satellite texture with size: " << nx << "x" << ny << std::endl;
}

int main()
{
	// Init
	window = new Window("Multi Scale Erosion", 1920, 1080);
	widget = new TerrainRaytracingWidget();
	window->SetUICallback(GUI);

	load_sat_png();

	// buffer init
	glGenBuffers(1, &m_terrain_buffer);

	hf = ScalarField2(Box2(Vector2::Null, 64 * 100), "heightfields/dem_test.png", 0.0, 3280.0 - 1996.0);

	LoadTerrain();
	load_soil();
	window->SetWidget(widget);


	ResetCamera();

	get_soil_texture();
	widget->SetAlbedo(albedoTexture);
	glGenTextures(1, &m_input_soil_texture);
	glBindTexture(GL_TEXTURE_2D, m_input_soil_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, siltf.GetSizeX(), siltf.GetSizeY(), 0, GL_RGBA, GL_UNSIGNED_BYTE, albedoTexture.Data());


	gpu_ds.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
	// gpu_ds.Step(2);


	// Main loop
	while (!window->Exit()) {
		if (m_run_erosion) {
			if (!m_init_erosion) gpu_e.Init(hf, m_terrain_buffer);
			m_init_erosion = true;
			m_init_thermal = false;
			m_init_soil_deposition = false;
			m_init_deposition = false;

			gpu_e.Step(100);

			widget->SetTerrainBuffer(gpu_e.GetTerrainGLuint());
			widget->UpdateInternal();

		} else if (m_run_thermal) {
			if (!m_init_thermal) gpu_t.Init(hf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = true;
			m_init_soil_deposition = false;
			m_init_deposition = false;

			gpu_t.Step(200);

			widget->SetTerrainBuffer(gpu_t.GetTerrainGLuint());
			widget->UpdateInternal();

		} else if (m_run_deposition) {
			if (!m_init_deposition) gpu_d.Init(hf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = false;
			m_init_soil_deposition = false;
			m_init_deposition = true;

			gpu_d.Step(50);

			widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
			widget->UpdateInternal();
		} else if (m_run_soil_deposition) {
			if (!m_init_soil_deposition) gpu_ds.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = false;
			m_init_deposition = false;
			m_init_soil_deposition = true;

			gpu_ds.Step(50);

			get_soil_texture(true);
			widget->SetTerrainBuffer(gpu_ds.GetTerrainGLuint());
			widget->UpdateInternal();
		}


		window->Update();
	}

	return 0;
}
