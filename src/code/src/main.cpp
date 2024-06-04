#include "window.h"
#include "terrainwidget.h"
#include "scalarfield2.h"
#include "gpu_shader.h"
#include "texture.h"
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
\brief Compute the intersection between a plane and a ray.
The intersection depth is returned if intersection occurs.
\param ray The ray.
\param t Intersection depth.
*/
static bool PlaneIntersect(const Ray& ray, double& t)
{
	double epsilon = 1e-4f;
	double x = Vector::Z * ray.Direction();
	if ((x < epsilon) && (x > -epsilon))
		return false;
	double c = Vector::Null * Vector::Z;
	double y = c - (Vector::Z * ray.Origin());
	t = y / x;
	return true;
}

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
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Preset terrain");
			if (ImGui::Button("Terrain 1")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/hfTest2.png", 0.0, 3000.0);
				widget->SetHeightField(&hf);
			}
			ImGui::SameLine();
			if (ImGui::Button("Terrain 2")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/hfTest3.png", 0.0, 2500.0);
				widget->SetHeightField(&hf);
			}
			ImGui::SameLine();
			if (ImGui::Button("Terrain 3")) {
				hf = ScalarField2(Box2(Vector2::Null, 10 * 1000), "heightfields/hfTest2.png", 0.0, 2000.0);
				widget->SetHeightField(&hf);
			}
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		}
		
		// Shading
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Shading");
			ImGui::RadioButton("Normal", &shadingMode, 0);
			ImGui::RadioButton("Drainage", &shadingMode, 1);
			if (shadingMode == 1) {
				ScalarField2 stream = hf;
				gpu_e.GetDataStream(stream);
				Texture2D texture = stream.CreateImage();
				widget->SetAlbedo(texture);
				widget->SetShadingMode(1);
			}
			else widget->SetShadingMode(shadingMode);
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
		}

		// Actions
		{
			// Reset camera
			if (ImGui::Button("Reset Camera"))
				ResetCamera();

			// Brushes
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "CTRL + Left Click to draw mountains");
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
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
				// get data on cpu heightfield
				std::vector<float> tmpData;
				tmpData.resize(hf.GetSizeX() * hf.GetSizeY());
				glGetNamedBufferSubData(m_terrain_buffer, 0, sizeof(float) * (hf.GetSizeX() * hf.GetSizeY()), tmpData.data());
				for (int i = 0; i < hf.GetSizeX() * hf.GetSizeY(); i++)
					hf[i] = double(tmpData[i]);

				// x2 rez
				hf = hf.SetResolution(hf.GetSizeX() * 2, hf.GetSizeY() * 2, true);

				widget->SetHeightField(&hf);

				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_terrain_buffer);
				glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * (hf.GetSizeX() * hf.GetSizeY()), hf.GetFloatData().data(), GL_STREAM_READ);
				widget->SetTerrainBuffer(m_terrain_buffer);

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

	/*ImGui::Begin("Help panel");
	{
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Controls");
		ImGui::TextWrapped("Camera: left click + mouse movements, and scrolling for zoom.");
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Examples");
		ImGui::TextWrapped("Simple examples are provided. Simply click on the associated buttons.");
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Editing brushes");
		ImGui::TextWrapped("Using ctrl + left/right click, you can add or remove elevation on the terrain. Brush size and strength can be modified using the left panel.");
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Other functionalities");
		ImGui::TextWrapped("A new scene can be created using the File > New Scene menu.");
		ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Issues");
		ImGui::TextWrapped("Contact me at axel(dot)paris69(at)gmail(dot)com, or report an issue on github.");
	}
	ImGui::End();*/
}

int main()
{
	// Init
	window = new Window("Stream Power Erosion", 1920, 1080);
	widget = new TerrainRaytracingWidget();
	hf = ScalarField2(Box2(Vector2::Null, 15*1000), "heightfields/hfTest2.png", 0.0, 4000.0);
	widget->SetHeightField(&hf);
	window->SetWidget(widget);
	window->SetUICallback(GUI);

	// buffer init
	glGenBuffers(1, &m_terrain_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_terrain_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * (hf.GetSizeX() * hf.GetSizeY()), hf.GetFloatData().data(), GL_STREAM_READ);

	widget->SetTerrainBuffer(m_terrain_buffer);

	albedoTexture = Texture2D(hf.GetSizeX(), hf.GetSizeY());
	albedoTexture.Fill(Color8(225, 225, 225, 255));
	widget->SetAlbedo(albedoTexture);

	ResetCamera();


	// Main loop
	while (!window->Exit()) {
		// Heightfield editing
		bool leftMouse = window->GetMousePressed(GLFW_MOUSE_BUTTON_LEFT);
		bool rightMouse = window->GetMousePressed(GLFW_MOUSE_BUTTON_RIGHT);
		bool mouseOverGUI = window->MouseOverGUI();
		if (!mouseOverGUI && (leftMouse) && window->GetKey(GLFW_KEY_LEFT_CONTROL)) {
			Camera cam = widget->GetCamera();
			double xpos, ypos;
			glfwGetCursorPos(window->getPointer(), &xpos, &ypos);
			Ray ray = cam.PixelToRay(int(xpos), int(ypos), window->width(), window->height());
			double t;
			if (PlaneIntersect(ray, t)) {
			}
		}
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
