#include "gpu_shader.h"


GPU_Erosion::~GPU_Erosion() {
	glDeleteBuffers(1, &bedrockBuffer);
	glDeleteBuffers(1, &tempBedrockBuffer);

	glDeleteBuffers(1, &streamBuffer);
	glDeleteBuffers(1, &tempStreamBuffer);

	glDeleteBuffers(1, &hardnessBuffer);

	release_program(simulationShader);
}

void GPU_Erosion::Init(const ScalarField2& hf) {
	// Prepare data for first step
	nx = hf.GetSizeX();
	ny = hf.GetSizeY();
	totalBufferSize = hf.VertexSize();
	dispatchSize = (max(nx, ny) / 8) + 1;

	tmpData.resize(totalBufferSize);
	for (int i = 0; i < totalBufferSize; i++)
		tmpData[i] = hf.at(i);

	std::vector<float> tmpZeros(totalBufferSize, 0.);

	// Prepare shader & Init buffer - Just done once
	std::string fullPath = std::string(RESOURCE_DIR) + "/shaders/erosion.glsl";
	simulationShader = read_program(fullPath.c_str());

	if (bedrockBuffer == 0) glGenBuffers(1, &bedrockBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bedrockBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpData.front(), GL_STREAM_READ);

	if (tempBedrockBuffer == 0) glGenBuffers(1, &tempBedrockBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempBedrockBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (streamBuffer == 0) glGenBuffers(1, &streamBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, streamBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (tempStreamBuffer == 0) glGenBuffers(1, &tempStreamBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, tempStreamBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * totalBufferSize, &tmpZeros.front(), GL_STREAM_READ);

	if (hardnessBuffer == 0) glGenBuffers(1, &hardnessBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, hardnessBuffer);
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

void GPU_Erosion::Step(int n) {

	for (int i = 0; i < n; i++) {

		glUseProgram(simulationShader);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, tempBedrockBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, streamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, tempStreamBuffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, hardnessBuffer);

		glDispatchCompute(dispatchSize, dispatchSize, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// dual buffering
		std::swap(bedrockBuffer, tempBedrockBuffer);
		std::swap(streamBuffer, tempStreamBuffer);
	}

	glUseProgram(0);
}


void GPU_Erosion::SetHardness(const ScalarField2& hardness) const {
	glUseProgram(simulationShader);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, hardnessBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * hardness.VertexSize(), &hardness.GetFloatData()[0], GL_STREAM_READ);

	glUseProgram(0);
}

void GPU_Erosion::GetData(ScalarField2& sf) {
	glGetNamedBufferSubData(bedrockBuffer, 0, sizeof(float) * totalBufferSize, tmpData.data());

	for (int i = 0; i < totalBufferSize; i++)
		sf[i] = double(tmpData[i]);
}

void GPU_Erosion::GetDataStream(ScalarField2& sf) {
	glGetNamedBufferSubData(streamBuffer, 0, sizeof(float) * totalBufferSize, tmpData.data());

	for (int i = 0; i < totalBufferSize; i++)
		sf[i] = std::pow(double(tmpData[i]), 0.2);

	double s_min, s_max;
	sf.GetRange(s_min, s_max);
	std::cout << "Drainage area values go from " << s_min << " to " << s_max << "." << std::endl;
}
