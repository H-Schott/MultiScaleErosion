#include "gpu_shader.h"


GPU_Deposition::~GPU_Deposition() {
	glDeleteBuffers(1, &bedrockBuffer);
	glDeleteBuffers(1, &tempBedrockBuffer);

	glDeleteBuffers(1, &streamBuffer);
	glDeleteBuffers(1, &tempStreamBuffer);

	glDeleteBuffers(1, &sedimentBuffer);
	glDeleteBuffers(1, &tempSedimentBuffer);

	release_program(simulationShader);
}

void GPU_Deposition::Init(const ScalarField2& hf, GLuint t_buffer) {
	// Prepare data for first step
	nx = hf.GetSizeX();
	ny = hf.GetSizeY();
	totalBufferSize = hf.VertexSize();
	dispatchSize = (max(nx, ny) / 8) + 1;

	tmpData.resize(totalBufferSize);
	for (int i = 0; i < totalBufferSize; i++)
		tmpData[i] = float(hf.at(i));

	std::vector<float> tmpZeros(totalBufferSize, 0.);

	// Prepare shader & Init buffer - Just done once
	if (simulationShader == 0) {
		std::string fullPath = std::string(RESOURCE_DIR) + "/shaders/deposition.glsl";
		simulationShader = read_program(fullPath.c_str());
	}

	bedrockBuffer = t_buffer;

	if (tempBedrockBuffer == 0) glGenBuffers(1, &tempBedrockBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempBedrockBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (streamBuffer == 0) glGenBuffers(1, &streamBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, streamBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempStreamBuffer == 0) glGenBuffers(1, &tempStreamBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempStreamBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (sedimentBuffer == 0) glGenBuffers(1, &sedimentBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sedimentBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempSedimentBuffer == 0) glGenBuffers(1, &tempSedimentBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempSedimentBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	// Uniforms - just once
	glUseProgram(simulationShader);

	Box2 box = hf.Array2::GetBox();
	Vector2 cellDiag = hf.CellDiagonal();
	glUniform1i(glGetUniformLocation(simulationShader, "nx"), nx);
	glUniform1i(glGetUniformLocation(simulationShader, "ny"), ny);
	glUniform2f(glGetUniformLocation(simulationShader, "cellDiag"), float(cellDiag[0]), float(cellDiag[1]));
	glUniform2f(glGetUniformLocation(simulationShader, "a"), float(box[0][0]), float(box[0][1]));
	glUniform2f(glGetUniformLocation(simulationShader, "b"), float(box[1][0]), float(box[1][1]));

	glUseProgram(0);
}

void GPU_Deposition::Step(int n) {

	for (int i = 0; i < n; i++) {

		glUseProgram(simulationShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, tempBedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, streamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, tempStreamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sedimentBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, tempSedimentBuffer);

		glDispatchCompute(dispatchSize, dispatchSize, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// dual buffering
		std::swap(bedrockBuffer, tempBedrockBuffer);
		std::swap(streamBuffer, tempStreamBuffer);
		std::swap(sedimentBuffer, tempSedimentBuffer);
	}

	glUseProgram(0);
}

void GPU_Deposition::GetData(ScalarField2& sf) {
	glGetNamedBufferSubData(bedrockBuffer, 0, sizeof(float) * totalBufferSize, tmpData.data());

	for (int i = 0; i < totalBufferSize; i++)
		sf[i] = double(tmpData[i]);
}

GPU_SoilDeposition::~GPU_SoilDeposition()
{
	glDeleteBuffers(1, &bedrockBuffer);
	glDeleteBuffers(1, &tempBedrockBuffer);

	glDeleteBuffers(1, &streamBuffer);
	glDeleteBuffers(1, &tempStreamBuffer);

	glDeleteBuffers(1, &sedimentBuffer);
	glDeleteBuffers(1, &tempSedimentBuffer);

	glDeleteBuffers(1, &siltBuffer);
	glDeleteBuffers(1, &tempSiltBuffer);

	glDeleteBuffers(1, &sandBuffer);
	glDeleteBuffers(1, &tempSandBuffer);

	glDeleteBuffers(1, &clayBuffer);
	glDeleteBuffers(1, &tempClayBuffer);

	release_program(simulationShader);
}

void GPU_SoilDeposition::Init(const ScalarField2& hf, const ScalarField2& siltf, const ScalarField2& sandf, const ScalarField2& clayf, GLuint t_buffer) {
	// Prepare data for first step
	std::cout << "Init Soil Deposition" << std::endl;
	nx = hf.GetSizeX();
	ny = hf.GetSizeY();
	totalBufferSize = hf.VertexSize();
	dispatchSize = (max(nx, ny) / 8) + 1;

	tmpData.resize(totalBufferSize);
	tmpSilt.resize(totalBufferSize);
	tmpSand.resize(totalBufferSize);
	tmpClay.resize(totalBufferSize);

	for (int i = 0; i < totalBufferSize; i++) {
		tmpData[i] = float(hf.at(i));
		tmpSilt[i] = float(siltf.at(i));
		tmpSand[i] = float(sandf.at(i));
		tmpClay[i] = float(clayf.at(i));
	}

	std::vector<float> tmpZeros(totalBufferSize, 0.);

	// Prepare shader & Init buffer - Just done once
	if (simulationShader == 0) {
		std::string fullPath = std::string(RESOURCE_DIR) + "/shaders/soil_deposition.glsl";
		simulationShader = read_program(fullPath.c_str());
	}

	bedrockBuffer = t_buffer;

	if (tempBedrockBuffer == 0) glGenBuffers(1, &tempBedrockBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempBedrockBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (streamBuffer == 0) glGenBuffers(1, &streamBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, streamBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempStreamBuffer == 0) glGenBuffers(1, &tempStreamBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempStreamBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (sedimentBuffer == 0) glGenBuffers(1, &sedimentBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sedimentBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempSedimentBuffer == 0) glGenBuffers(1, &tempSedimentBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempSedimentBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (siltBuffer == 0) glGenBuffers(1, &siltBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, siltBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpSilt.front(), GL_STREAM_READ);

	if (tempSiltBuffer == 0) glGenBuffers(1, &tempSiltBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempSiltBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpSilt.front(), GL_STREAM_READ);

	if (sandBuffer == 0) glGenBuffers(1, &sandBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sandBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpSand.front(), GL_STREAM_READ);

	if (tempSandBuffer == 0) glGenBuffers(1, &tempSandBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempSandBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpSand.front(), GL_STREAM_READ);

	if (clayBuffer == 0) glGenBuffers(1, &clayBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, clayBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpClay.front(), GL_STREAM_READ);

	if (tempClayBuffer == 0) glGenBuffers(1, &tempClayBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempClayBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpClay.front(), GL_STREAM_READ);

	// Uniforms - just once
	glUseProgram(simulationShader);

	Box2 box = hf.Array2::GetBox();
	Vector2 cellDiag = hf.CellDiagonal();
	glUniform1i(glGetUniformLocation(simulationShader, "nx"), nx);
	glUniform1i(glGetUniformLocation(simulationShader, "ny"), ny);
	glUniform2f(glGetUniformLocation(simulationShader, "cellDiag"), float(cellDiag[0]), float(cellDiag[1]));
	glUniform2f(glGetUniformLocation(simulationShader, "a"), float(box[0][0]), float(box[0][1]));
	glUniform2f(glGetUniformLocation(simulationShader, "b"), float(box[1][0]), float(box[1][1]));

	glUseProgram(0);
}

void GPU_SoilDeposition::Step(int n) {

	for (int i = 0; i < n; i++) {

		glUseProgram(simulationShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, tempBedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, streamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, tempStreamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sedimentBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, tempSedimentBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, siltBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, tempSiltBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, sandBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, tempSandBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, clayBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, tempClayBuffer);

		glDispatchCompute(dispatchSize, dispatchSize, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// dual buffering
		std::swap(bedrockBuffer, tempBedrockBuffer);
		std::swap(streamBuffer, tempStreamBuffer);
		std::swap(sedimentBuffer, tempSedimentBuffer);
		std::swap(siltBuffer, tempSiltBuffer);
		std::swap(sandBuffer, tempSandBuffer);
		std::swap(clayBuffer, tempClayBuffer);
	}

	glUseProgram(0);
}

void GPU_SoilDeposition::GetSoilData(ScalarField2& siltf, ScalarField2& sandf, ScalarField2& clayf) {
	glGetNamedBufferSubData(siltBuffer, 0, sizeof(float) * totalBufferSize, tmpSilt.data());
	if (glGetError()) {
		return; // Handle error properly
	}

	for (int i = 0; i < totalBufferSize; i++)
		siltf[i] = double(tmpSilt[i]);

	glGetNamedBufferSubData(sandBuffer, 0, sizeof(float) * totalBufferSize, tmpSand.data());

	for (int i = 0; i < totalBufferSize; i++)
		sandf[i] = double(tmpSand[i]);

	glGetNamedBufferSubData(clayBuffer, 0, sizeof(float) * totalBufferSize, tmpClay.data());

	for (int i = 0; i < totalBufferSize; i++)
		clayf[i] = double(tmpClay[i]);
}

GPU_HydraulicErosion::~GPU_HydraulicErosion()
{
	glDeleteBuffers(1, &bedrockBuffer);
	glDeleteBuffers(1, &tempBedrockBuffer);

	glDeleteBuffers(1, &streamBuffer);
	glDeleteBuffers(1, &tempStreamBuffer);

	glDeleteBuffers(1, &sedimentBuffer);
	glDeleteBuffers(1, &tempSedimentBuffer);

	// glDeleteBuffers(1, &siltBuffer);
	// glDeleteBuffers(1, &tempSiltBuffer);
	//
	// glDeleteBuffers(1, &sandBuffer);
	// glDeleteBuffers(1, &tempSandBuffer);
	//
	// glDeleteBuffers(1, &clayBuffer);
	// glDeleteBuffers(1, &tempClayBuffer);
	glDeleteBuffers(1, &soilTexBuffer);
	glDeleteBuffers(1, &tempSoilTexBuffer);

	glDeleteBuffers(1, &waterBuffer);
	glDeleteBuffers(1, &tempWaterBuffer);

	glDeleteBuffers(1, &velocityBuffer);
	glDeleteBuffers(1, &tempVelocityBuffer);

	glDeleteBuffers(1, &fluxBuffer);
	glDeleteBuffers(1, &tempFluxBuffer);

	release_program(simulationShader);
}

void GPU_HydraulicErosion::Init(const ScalarField2& hf, const ScalarField2& siltf, const ScalarField2& sandf, const ScalarField2& clayf, GLuint t_buffer) {
	// Prepare data for first step
	std::cout << "Init Soil Deposition" << std::endl;
	nx = hf.GetSizeX();
	ny = hf.GetSizeY();
	totalBufferSize = hf.VertexSize();
	dispatchSize = (max(nx, ny) / 8) + 1;

	tmpData.resize(totalBufferSize);
	tmpSoilTex3.resize(totalBufferSize * 3);

	for (int i = 0; i < totalBufferSize; i++) {
		tmpData[i] = float(hf.at(i));
		tmpSoilTex3[(i * 3) + 0] = float(siltf.at(i));
		tmpSoilTex3[(i * 3) + 1] = float(sandf.at(i));
		tmpSoilTex3[(i * 3) + 2] = float(clayf.at(i));
	}

	std::vector<float> tmpZeros(totalBufferSize, 0.);
	std::vector<float> tmpZeros2(totalBufferSize*2, 0.);
	std::vector<float> tmpZeros3(totalBufferSize*3, 0.);
	std::vector<float> tmpZeros4(totalBufferSize*4, 0.);

	// Prepare shader & Init buffer - Just done once
	if (simulationShader == 0) {
		std::string fullPath = std::string(RESOURCE_DIR) + "/shaders/hydraulic_erosion.glsl";
		simulationShader = read_program(fullPath.c_str());
	}

	bedrockBuffer = t_buffer;

	if (tempBedrockBuffer == 0) glGenBuffers(1, &tempBedrockBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempBedrockBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	// if (streamBuffer == 0) glGenBuffers(1, &streamBuffer);
	// glBindBuffer(GL_SHADER_STORAGE_BUFFER, streamBuffer);
	// glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);
	//
	// if (tempStreamBuffer == 0) glGenBuffers(1, &tempStreamBuffer);
	// glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempStreamBuffer);
	// glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (sedimentBuffer == 0) glGenBuffers(1, &sedimentBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, sedimentBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempSedimentBuffer == 0) glGenBuffers(1, &tempSedimentBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempSedimentBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (soilTexBuffer == 0) glGenBuffers(1, &soilTexBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, soilTexBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize * 3, &tmpSoilTex3.front(), GL_STREAM_READ);

	if (tempSoilTexBuffer == 0) glGenBuffers(1, &tempSoilTexBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempSoilTexBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize * 3, &tmpSoilTex3.front(), GL_STREAM_READ);

	if (waterBuffer == 0) glGenBuffers(1, &waterBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, waterBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempWaterBuffer == 0) glGenBuffers(1, &tempWaterBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempWaterBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (velocityBuffer == 0) glGenBuffers(1, &velocityBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 2 * totalBufferSize, &tmpZeros2.front(), GL_STREAM_READ);

	if (tempVelocityBuffer == 0) glGenBuffers(1, &tempVelocityBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempVelocityBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 2 * totalBufferSize, &tmpZeros2.front(), GL_STREAM_READ);

	if (fluxBuffer == 0) glGenBuffers(1, &fluxBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, fluxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 4 * totalBufferSize, &tmpZeros4.front(), GL_STREAM_READ);

	if (tempFluxBuffer == 0) glGenBuffers(1, &tempFluxBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempFluxBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * 4 * totalBufferSize, &tmpZeros4.front(), GL_STREAM_READ);

	// Uniforms - just once
	glUseProgram(simulationShader);

	Box2 box = hf.Array2::GetBox();
	Vector2 cellDiag = hf.CellDiagonal();
	glUniform1i(glGetUniformLocation(simulationShader, "nx"), nx);
	glUniform1i(glGetUniformLocation(simulationShader, "ny"), ny);
	glUniform2f(glGetUniformLocation(simulationShader, "cellDiag"), float(cellDiag[0]), float(cellDiag[1]));
	glUniform2f(glGetUniformLocation(simulationShader, "a"), float(box[0][0]), float(box[0][1]));
	glUniform2f(glGetUniformLocation(simulationShader, "b"), float(box[1][0]), float(box[1][1]));

	glUseProgram(0);
}

void GPU_HydraulicErosion::Step(int n) {

	for (int i = 0; i < n; i++) {

		glUseProgram(simulationShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, tempBedrockBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, streamBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, tempStreamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sedimentBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, tempSedimentBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, siltBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, tempSiltBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, sandBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, tempSandBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, clayBuffer);
		// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, tempClayBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, soilTexBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, tempSoilTexBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, waterBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, tempWaterBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, velocityBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, tempVelocityBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, fluxBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, tempFluxBuffer);

		glDispatchCompute(dispatchSize, dispatchSize, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// dual buffering
		std::swap(bedrockBuffer, tempBedrockBuffer);
		std::swap(streamBuffer, tempStreamBuffer);
		std::swap(sedimentBuffer, tempSedimentBuffer);
		// std::swap(siltBuffer, tempSiltBuffer);
		// std::swap(sandBuffer, tempSandBuffer);
		// std::swap(clayBuffer, tempClayBuffer);
		std::swap(soilTexBuffer, tempSoilTexBuffer);
		std::swap(waterBuffer, tempWaterBuffer);
		std::swap(velocityBuffer, tempVelocityBuffer);
		std::swap(fluxBuffer, tempFluxBuffer);
	}

	glUseProgram(0);
}

void GPU_HydraulicErosion::GetSoilData(ScalarField2& siltf, ScalarField2& sandf, ScalarField2& clayf) {
	glGetNamedBufferSubData(soilTexBuffer, 0, sizeof(float) * totalBufferSize * 3, tmpSoilTex3.data());
	for (int i = 0; i < totalBufferSize; i++) {
		siltf[i] = double(tmpSoilTex3[i * 3 + 0]);
		sandf[i] = double(tmpSoilTex3[i * 3 + 1]);
		clayf[i] = double(tmpSoilTex3[i * 3 + 2]);
	}
}
