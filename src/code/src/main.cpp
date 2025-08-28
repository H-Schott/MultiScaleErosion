#include "window.h"
#include "terrainwidget.h"
#include "scalarfield2.h"
#include "gpu_shader.h"
#include "texture.h"
#include "inspectwidget.h"
#include "write_16_png.h"
#include "ImFileDialog.h"
#include <imgui.h>

#include "lodepng.h"
#include "stb_image.h"
#include "stb_image_write.h"
#include "gdal_priv.h"
#include "geotiff.h"
#include "imgui_tex_inspect.h"
#include "render_texture.h"

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
static GPU_HydraulicErosion gpu_he;
static Texture2D albedoTexture;
static int shadingMode;

static GLuint m_satellite_texture = 0;
static GLuint m_input_soil_texture = 0;

static bool m_run_erosion     = false;
static bool m_run_thermal     = false;
static bool m_run_deposition  = false;
static bool m_run_soil_deposition = false;
static bool m_run_hydraulic_erosion = false;

static bool m_init_erosion    = false;
static bool m_init_thermal    = false;
static bool m_init_deposition = false;
static bool m_init_soil_deposition = false;
static bool m_init_hydraulic_erosion = false;

static GLuint m_terrain_buffer = 0;

static InspectWidget inspectwidget;
static GLuint inspect_shader = 0;
static GLuint inspect_texture = 0;
RenderTexture inspect_rt;
static float minColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static float maxColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};



Raster soil_raster;

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

/**
 * Classifies soil texture based on sand, silt, and clay percentages
 * using the USDA soil texture classification system.
 *
 * @param sand Percentage of sand (0-100)
 * @param silt Percentage of silt (0-100)
 * @param clay Percentage of clay (0-100)
 * @return String containing the USDA soil texture class
 */
// this should really be a lookup table
static std::string classifySoilTexture(double sand, double silt, double clay) {
    // Validate input - percentages should sum to approximately 100
    double total = sand + silt + clay;
    if (std::abs(total - 100.0) > 2.0) {
        // return "ERROR: Sand + Silt + Clay must sum to ~100%";
    	sand = sand/total * 100.0;
    	silt = silt/total * 100.0;
    	clay = clay/total * 100.0;
    	// return std::string("sand, silt, clay percentages adjusted to sum to 100%: ") +
			  //  "Sand: " + std::to_string(sand) + "%, " +
			  //  "Silt: " + std::to_string(silt) + "%, " +
			  //  "Clay: " + std::to_string(clay) + "%";
		// if (std::abs(sand + silt + clay - 100.0) > 2.0) {
		// 	return "error: sand + silt + clay must sum to ~100%";
		// }
    }

    // Ensure all values are non-negative
    if (sand < 0 || silt < 0 || clay < 0) {
        return "ERROR: All percentages must be non-negative";
    }

    // USDA Soil Texture Classification Logic
    // Based on the standard USDA soil texture triangle

	// catch weird ones early:
	if (sand >= 70.0 && clay <= 15.0) {
		if (sand >= 85.0 && clay <= 5.0) {
			return "Sand";
		}
		return "Loamy Sand";
	}

    if (clay >= 40.0) {
	    // SaCl, Cl, SiCL
    	if (sand >= 45.0) {
    		return "Sandy Clay";
    	}
    	if (silt >= 40.0) {
			return "Silty Clay";
		}
    	return "Clay";
    } else if (clay >= 27.0) {
		if (sand >= 45.0) {
			if (clay <= 35.0) {
				return "Sandy Clay Loam";
			}
			return "Sandy Clay";
		}
    	if (sand >= 20.0) {
    		return "Clay Loam";
    	}
    	return "Silty Clay Loam";
    }
    else if (clay >= 20.0) {
	    // SaClLo, Lo, SiLo
    	if (silt >= 50.0) return "Silty Loam";
    	if (silt >= 27.0) return "Loam";
    	return "Sandy Clay Loam";
	} else if (clay >= 12.0) {
	    // SaLo, Lo, SiLo
		if (sand >= 52.0) return "Sandy Loam";
		if (silt >= 50.0) return "Silty Loam";
		return "Loam";
	} else {
	    // LoSa, SaLo, Lo, SiLo
		if (sand >= 52.0 && clay <= 8.0) return "Sandy Loam";
		if (silt >= 80.0) return "Silt";
		if (silt >= 50.0) return "Silty Loam";
		return "Loam";
	}
	return "?????????????";
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
	gpu_ds.Step(2000);

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
	gpu_ds.Step(1000);

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

static void ShowSoilTooltip()
{
	ImVec2 startpos = ImGui::GetItemRectMin();
	ImVec2 size = ImGui::GetItemRectSize();
	if (ImGui::IsItemHovered() && ImGui::BeginItemTooltip()) {
		ImVec2 mousepos = ImGui::GetMousePos();
		ImVec2 coords = ImVec2(mousepos.x - startpos.x, mousepos.y - startpos.y);
		coords.x = coords.x / size.x * hf.GetSizeX();
		coords.y = coords.y / size.y * hf.GetSizeY();
		int xpos = int(coords.x);
		int ypos = int(coords.y);
		double height = hf.at(xpos, ypos);
		double silt = siltf.at(xpos, ypos) * 100.0;
		double sand = sandf.at(xpos, ypos) * 100.0;
		double clay = clayf.at(xpos, ypos) * 100.0;
		std::string soilTexture = classifySoilTexture(sand, silt, clay);
		ImGui::SetTooltip("coords: %d, %d\nheight: %.2f\nsilt: %.2f%\nsand: %.2f%\nclay: %.2f%\ntexture class: %s", int(coords.x), int(coords.y), height, silt, sand, clay, soilTexture.c_str());



		// ImGui::SetTooltip("%d, %d", int(mousepos.x-startpos.x), int(mousepos.y-startpos.y));
		ImGui::EndTooltip();
	}
}

void get_soil_texture_hydro(bool cond);

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
		// {
		// 	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Results");
		// 	if (ImGui::Button("Result #1")) {
		// 		hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/noise.png", 0., 3000.);
		// 		LoadTerrain();
		//
		// 		PredefinedErosion();
		//
		// 		GetTerrain();
		// 		widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
		// 		widget->initializeGL();
		//
		// 		ResetCamera();
		// 	}
		// 	ImGui::SameLine();
		//
		// 	if (ImGui::Button("Result #2")) {
		// 		hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/mountains.png", 0., 3000.);
		// 		LoadTerrain();
		//
		// 		PredefinedErosion();
		//
		// 		GetTerrain();
		// 		widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
		// 		widget->initializeGL();
		//
		// 		ResetCamera();
		// 	}
		// 	ImGui::SameLine();
		//
		// 	if (ImGui::Button("Result #3")) {
		// 		hf = ScalarField2(Box2(Vector2::Null, 64 * 100), "heightfields/dem_test.png", 0., 3280.0 - 1996.0);
		// 		load_soil();
		// 		LoadTerrain();
		//
		// 		PredefinedSoilErosion();
		//
		// 		GetTerrain();
		// 		widget->SetTerrainBuffer(gpu_ds.GetTerrainGLuint());
		// 		widget->initializeGL();
		// 		get_soil_texture(true);
		//
		// 		ResetCamera();
		// 	}
		// 	ImGui::Separator();
		// 	ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// }
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Results");
			if (ImGui::Button("Result #1 (Mesa Verde)")) {
				Raster elv_raster;
				read_geotiff(RESOURCE_DIR "/tifs/mesaverde.tif", elv_raster);
				load_raster_to_field(elv_raster, hf, "elevation");

				double prop_translate[4] = {0.0, 100.0, 0.0, 1.0};
				read_geotiff(RESOURCE_DIR "/tifs/mesaverde.tif", soil_raster, prop_translate);

				load_raster_to_field(soil_raster, siltf, "silttotal");
				load_raster_to_field(soil_raster, sandf, "sandtotal");
				load_raster_to_field(soil_raster, clayf, "claytotal");

				gpu_ds.Init(hf, siltf, sandf, clayf, m_terrain_buffer);

				LoadTerrain();

				PredefinedSoilErosion();

				GetTerrain();
				get_soil_texture(true);
				widget->SetTerrainBuffer(gpu_ds.GetTerrainGLuint());
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
				get_soil_texture(true);

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

		// {
		// 	ImGui::Text("Erosion");
		// 	ImGui::Checkbox("Run E", &m_run_erosion);
		// }
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// ImGui::Separator();
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// {
		// 	ImGui::Text("Thermal");
		// 	ImGui::Checkbox("Run T", &m_run_thermal);
		// }
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// ImGui::Separator();
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// {
		// 	ImGui::Text("Deposition");
		// 	ImGui::Checkbox("Run D", &m_run_deposition);
		// }
		// ImGui::Separator();
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// {
		// 	ImGui::Text("Soil Deposition");
		// 	ImGui::Checkbox("Run SD", &m_run_soil_deposition);
		// }
		// ImGui::Separator();
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// {
		// 	ImGui::Text("Hydraulic Erosion");
		// 	ImGui::Checkbox("Run HE", &m_run_hydraulic_erosion);
		// }
		// ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		// ImGui::Separator();
		static int erosion_steps = 3000;
		static int thermal_steps = 600;
		static int deposition_steps = 2000;
		static int soil_deposition_steps = 2000;
		ImGui::SliderInt("Erosion steps", &erosion_steps, 1, 10000);
		ImGui::SameLine();
		if (ImGui::Button("Run Erosion")) {
			if (!m_init_erosion) {
				gpu_e.Init(hf, m_terrain_buffer);
				m_init_erosion = true;
				m_init_thermal = false;
				m_init_deposition = false;
				m_init_soil_deposition = false;
				m_init_hydraulic_erosion = false;
			}
			gpu_e.Step(erosion_steps);
			widget->SetTerrainBuffer(gpu_e.GetTerrainGLuint());
			widget->UpdateInternal();
		}
		ImGui::SliderInt("Thermal erosion steps", &thermal_steps, 1, 10000);
		ImGui::SameLine();
		if (ImGui::Button("Run Thermal")) {
			if (!m_init_thermal) {
				gpu_t.Init(hf, m_terrain_buffer);
				m_init_erosion = false;
				m_init_thermal = true;
				m_init_deposition = false;
				m_init_soil_deposition = false;
				m_init_hydraulic_erosion = false;
			}
			gpu_t.Step(thermal_steps);
			widget->SetTerrainBuffer(gpu_t.GetTerrainGLuint());
			widget->UpdateInternal();
		}
		ImGui::SliderInt("Deposition steps", &deposition_steps, 1, 10000);
		ImGui::SameLine();
		if (ImGui::Button("Run Deposition")) {
			if (!m_init_deposition) {
				gpu_d.Init(hf, m_terrain_buffer);
				m_init_erosion = false;
				m_init_thermal = false;
				m_init_deposition = true;
				m_init_soil_deposition = false;
				m_init_hydraulic_erosion = false;
			}
			gpu_d.Step(deposition_steps);
			widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
			widget->UpdateInternal();
		}
		ImGui::SliderInt("Soil Deposition steps", &soil_deposition_steps, 1, 10000);
		ImGui::SameLine();
		if (ImGui::Button("Run Soil Deposition")) {
			if (!m_init_soil_deposition) {
				gpu_ds.Init(hf,  siltf, sandf, clayf, m_terrain_buffer);
				m_init_erosion = false;
				m_init_thermal = false;
				m_init_deposition = false;
				m_init_soil_deposition = true;
				m_init_hydraulic_erosion = false;
			}
			gpu_ds.Step(soil_deposition_steps);
			widget->SetTerrainBuffer(gpu_ds.GetTerrainGLuint());
			widget->UpdateInternal();
		}
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
				gpu_ds.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
				gpu_he.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
				get_soil_texture(true);
				widget->SetAlbedo(albedoTexture);

				ResetCamera();

				m_init_erosion = false;
				m_init_thermal = false;
				m_init_deposition = false;
				m_init_soil_deposition = false;
				m_init_hydraulic_erosion = false;
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
			if (ImGui::Button("Run 1 step Hydraulic Erosion")) {
				if (!m_init_hydraulic_erosion) {
					gpu_he.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
					m_init_hydraulic_erosion = true;
					m_init_soil_deposition = false;
					m_init_erosion = false;
					m_init_thermal = false;
					m_init_deposition = false;
				}
				gpu_he.Step(1);
				get_soil_texture_hydro(true);
				widget->SetAlbedo(albedoTexture);

			}

		}
		if (ImGui::BeginTabBar("ImageTabs"))
		{
			static int imgsz = 512;
			static bool hovered = false; // use for all tabs
			static ImVec2 tooltip_coords = ImVec2(0, 0);
			if (ImGui::BeginTabItem("Input")) {
				ImGui::Image((ImTextureID) m_input_soil_texture, ImVec2(imgsz, imgsz));
				ShowSoilTooltip();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Sim result"))
			{
				ImGui::Image((ImTextureID) widget->GetAlbedoID(), ImVec2(imgsz, imgsz));
				ShowSoilTooltip();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Satellite reference"))
			{
				ImGui::Image((ImTextureID) m_satellite_texture, ImVec2(imgsz, imgsz));
				ShowSoilTooltip();
				ImGui::EndTabItem();
			}
			// inspectwidget.render_imgui();


			ImGui::EndTabBar();
		}
		// ImGui::Image((ImTextureID) widget->GetAlbedoID(), ImVec2(512, 512));
		// ImGui::Image((ImTextureID) m_satellite_texture, ImVec2(512, 512));
		if (ImGui::Button("Save terrain")) {
			GetTerrain();
			save_field_to_raster(hf, soil_raster, "elevation", RESOURCE_DIR "/out/terrain_out.tif");
			get_soil_texture(true);
			double prop_translate[4] = {0.0, 1.0, 0.0, 100.0};
			save_field_to_raster(siltf, soil_raster, "silt", RESOURCE_DIR "/out/silt_out.tif", prop_translate);
			save_field_to_raster(sandf, soil_raster, "sand", RESOURCE_DIR "/out/sand_out.tif", prop_translate);
			save_field_to_raster(clayf, soil_raster, "clay", RESOURCE_DIR "/out/clay_out.tif", prop_translate);
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
	ImGui::Begin("Inspector", 0, 0);
	static GLuint active_buffer = 0;
	ScalarField2& inspect_field = hf;

	static int current_selection = 0;

	IInspect* inspect = nullptr;
	if (m_init_soil_deposition) {
		inspect = dynamic_cast<IInspect*>(static_cast<GPU_Deposition*>(&gpu_ds));
	} else if (m_init_erosion) {
		inspect = &gpu_e;
	} else if (m_init_thermal) {
		inspect = &gpu_t;
	} else if (m_init_deposition) {
		inspect = &gpu_d;
	}

	std::vector<BufferDescriptor> buffers = {
		BufferDescriptor {
			"height",
			m_terrain_buffer,
			hf.GetSizeX(), hf.GetSizeY(),
			1
		}
	};
	if (inspect) {
		auto new_buffers = inspect->GetBuffers();
		buffers.insert(buffers.end(), new_buffers.begin(), new_buffers.end());
	}
	{
		int i = 0;
		const char* item_names[10] = {};
		for (const auto& buffer : buffers) {
			item_names[i] = buffer.name;
			if (buffer.id == active_buffer) {
				current_selection = i;
			}
			i++;
		}


		ImGui::Combo("Inspect Buffer", &current_selection, item_names, i);
		current_selection = std::min(current_selection, int(buffers.size() - 1));
		BufferDescriptor selection = buffers[current_selection];

		ImGui::Text(selection.name);
		ImGui::Text("number of bands: %d", selection.n_bands);
		ImGui::Text("active buffer: %d", selection.id);
		ImGui::Text("size: %d x %d", selection.nx, selection.ny);
		ImGui::Text("range [%.2f, %.2f]", selection.zmin, selection.zmax);

		ImGui::ColorEdit3("Min color", minColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
		ImGui::SameLine();
		ImGui::ColorEdit3("Max color", maxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);

		set_render_texture_uniform3f(inspect_rt, "minColor", minColor[0], minColor[1], minColor[2]);
		set_render_texture_uniform3f(inspect_rt, "maxColor", maxColor[0], maxColor[1], maxColor[2]);

		set_render_texture_uniform2f(inspect_rt, "zRange", selection.zmin, selection.zmax);

		active_buffer = selection.id;
		render_to_texture(inspect_rt, selection);

		ImGuiTexInspect::BeginInspectorPanel("Inspector", (ImTextureID) inspect_rt.texture, ImVec2(selection.nx,selection.ny),
				ImGuiTexInspect::InspectorFlags_FillVertical |
				ImGuiTexInspect::InspectorFlags_FillHorizontal |
				ImGuiTexInspect::InspectorFlags_NoGrid);


		ImGuiTexInspect::DrawAnnotations(ImGuiTexInspect::ValueText(ImGuiTexInspect::ValueText::Floats));
		ImGuiTexInspect::EndInspectorPanel();
	}
	// } else {
	// 	ImGui::Text("no buffers to inspect");
	// }
	std::stringstream cam_strings;
	cam_strings << widget->GetCamera();
	std::string cam_string = cam_strings.str();

	ImGui::Text("%s", cam_string.c_str());

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
	auto siltimg = siltf.GetFloatData();
	auto sandimg = sandf.GetFloatData();
	auto clayimg = clayf.GetFloatData();

	int nx = siltf.GetSizeX();
	int ny = siltf.GetSizeY();
	auto colors = std::vector<Color8>(nx*ny);

	for (int i = 0; i < nx*ny; i++) {
		colors[i] = Color8(
			static_cast<unsigned char>(siltimg[i] * 255),
			static_cast<unsigned char>(sandimg[i] * 255),
			static_cast<unsigned char>(clayimg[i] * 255),
			255); // alpha channel set to 255
	}
	albedoTexture = Texture2D(colors, nx, ny);
	widget->SetAlbedo(albedoTexture);
}

void get_soil_texture_hydro(bool fetch)
{
	if (fetch) {
		gpu_he.GetSoilData(siltf, sandf, clayf);
	}
	auto siltimg = siltf.GetFloatData();
	auto sandimg = sandf.GetFloatData();
	auto clayimg = clayf.GetFloatData();

	int nx = siltf.GetSizeX();
	int ny = siltf.GetSizeY();
	auto colors = std::vector<Color8>(nx*ny);

	for (int i = 0; i < nx*ny; i++) {
		colors[i] = Color8(
			static_cast<unsigned char>(siltimg[i] * 255),
			static_cast<unsigned char>(sandimg[i] * 255),
			static_cast<unsigned char>(clayimg[i] * 255),
			255); // alpha channel set to 255
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

	std::cout << "Loaded " << nx << "x" << ny << " image with " << n << " channels" << std::endl;
	for (int i = 0; i < nx*ny; i++) {
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

	auto siltimg = siltf.GetFloatData();
	auto sandimg = sandf.GetFloatData();
	auto clayimg = clayf.GetFloatData();

	auto colors = std::vector<Color8>(nx*ny);

	for (int i = 0; i < nx*ny; i++) {
		colors[i] = Color8(
			static_cast<unsigned char>(siltimg[i] * 255),
			static_cast<unsigned char>(sandimg[i] * 255),
			static_cast<unsigned char>(clayimg[i] * 255),
			255); // alpha channel set to 255
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

	std::string outpath = std::string(RESOURCE_DIR) + "/test.png";

	std::cout << siltf.GetData().size() << ", " << siltf.GetSizeX() << ", " << siltf.GetSizeY() << std::endl;

	// buffer init
	glGenBuffers(1, &m_terrain_buffer);

	Raster raster;
	// double height_translate[4] = {0.0, 100.0, 0.0, 1.0};
	read_geotiff(RESOURCE_DIR "/tifs/mesaverde.tif", raster);

	std::cout << "Raster size: " << raster.nx << " x " << raster.ny << std::endl;

	// hf = ScalarField2(Box2(Vector2::Null, 64 * 100), "heightfields/dem_test.png", 0.0, 3280.0 - 1996.0);
	// hf = ScalarField2(Box2(Vector2::Null, double(nXSize)*pixel_x_size/2), nXSize, nYSize, data);
	load_raster_to_field(raster, hf, "elevation");
	std::cout << "post load hf size: " << hf.GetSizeX() << ", " << hf.GetSizeY() << std::endl;
	// hf.SetRange(0,1000);

	double prop_translate[4] = {0.0, 100.0, 0.0, 1.0};
	read_geotiff(RESOURCE_DIR "/tifs/mesaverde.tif", soil_raster, prop_translate);
	double prop_range[2] = {0.0,1.0};
	// map_raster_range(0, 100, 0, 1, soil_raster, 2);
	// map_raster_range(0,100,0,1,soil_raster,0);
	// map_raster_range(0,100,0,1,soil_raster,4);

	load_raster_to_field(soil_raster, siltf, "silttotal");
	load_raster_to_field(soil_raster, sandf, "sandtotal");
	load_raster_to_field(soil_raster, clayf, "claytotal");

	// siltf = hf;
	// sandf = hf;
	// clayf = hf;
	// depthf = hf;
	LoadTerrain();
	// load_soil();
	window->SetWidget(widget);

	inspect_rt = create_render_texture(hf.CellSizeX(), hf.CellSizeY(), RESOURCE_DIR "/shaders/inspect_shader.glsl");

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
	// gpu_he.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
	// gpu_ds.Step(2);
	std::cout << "hf test height (0,127): "<< hf.at(127,127) << std::endl;


	// std::cout << widget->GetCamera() << std::endl;
	// Main loop
	while (!window->Exit()) {
		if (m_run_erosion) {
			if (!m_init_erosion) gpu_e.Init(hf, m_terrain_buffer);
			m_init_erosion = true;
			m_init_thermal = false;
			m_init_soil_deposition = false;
			m_init_deposition = false;
			m_init_hydraulic_erosion = false;

			gpu_e.Step(100);

			widget->SetTerrainBuffer(gpu_e.GetTerrainGLuint());
			widget->UpdateInternal();

		} else if (m_run_thermal) {
			if (!m_init_thermal) gpu_t.Init(hf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = true;
			m_init_soil_deposition = false;
			m_init_deposition = false;
			m_init_hydraulic_erosion = false;

			gpu_t.Step(200);

			widget->SetTerrainBuffer(gpu_t.GetTerrainGLuint());
			widget->UpdateInternal();

		} else if (m_run_deposition) {
			if (!m_init_deposition) gpu_d.Init(hf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = false;
			m_init_soil_deposition = false;
			m_init_deposition = true;
			m_init_hydraulic_erosion = false;

			gpu_d.Step(50);

			widget->SetTerrainBuffer(gpu_d.GetTerrainGLuint());
			widget->UpdateInternal();
		} else if (m_run_soil_deposition) {
			if (!m_init_soil_deposition) gpu_ds.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = false;
			m_init_deposition = false;
			m_init_soil_deposition = true;
			m_init_hydraulic_erosion = false;

			gpu_ds.Step(50);

			get_soil_texture(true);
			widget->SetTerrainBuffer(gpu_ds.GetTerrainGLuint());
			widget->UpdateInternal();
		} else if (m_run_hydraulic_erosion) {
			if (!m_init_hydraulic_erosion) gpu_he.Init(hf, siltf, sandf, clayf, m_terrain_buffer);
			m_init_erosion = false;
			m_init_thermal = false;
			m_init_deposition = false;
			m_init_soil_deposition = false;
			m_init_hydraulic_erosion = true;

			gpu_he.Step(50);

			get_soil_texture_hydro(true);
			widget->SetTerrainBuffer(gpu_he.GetTerrainGLuint());
			widget->UpdateInternal();
		}


		window->Update();
	}

	return 0;
}
