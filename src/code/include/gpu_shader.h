#ifndef __GPU_SHADER__
#define __GPU_SHADER__

#include <GL/glew.h>
#include <vector>
#include <string>

#include "scalarfield2.h"
#include "shader-api.h"

struct BufferDescriptor {
	const char* name = nullptr;  //!< Name of the buffer
	GLuint id = 0;  //!< Buffer ID
	int nx = 0;
	int ny = 0;  //!< Size of the buffer in x and y dimensions
	int n_bands = 1;
	float zmin = 0.0f;
	float zmax = 1.0f;
};

class IInspect {
public:
	IInspect() {}
	virtual ~IInspect() {}
	virtual std::vector<BufferDescriptor> GetBuffers() = 0;
};

class GPU_Erosion : public IInspect {
private:
	GLuint simulationShader = 0;			//!< Compute shader

	GLuint bedrockBuffer = 0;				//!< Bedrock elevation buffer
	GLuint tempBedrockBuffer = 0;			//!< Temporary bedrock elevation buffer

	GLuint streamBuffer = 0;				//!< Stream area buffer
	GLuint tempStreamBuffer = 0;			//!< Temporary stream area buffer

	GLuint hardnessBuffer = 0;		        //!< Hardness buffer

	int nx = 0;
	int ny = 0;
	int totalBufferSize = 0;					//!< Total buffer size defined as nx * ny
	int dispatchSize = 0;						//!< Single dispatch size
	std::vector<float> tmpData;			        //!< Temporary array for retreiving GPU data
	double bedrockzmin;
	double bedrockzmax;
public:
	GPU_Erosion() {};
	~GPU_Erosion();

	void Init(const ScalarField2& hf, GLuint t_buffer);
	void Step(int n);

	void SetHardness(const ScalarField2& hardness) const;
	void GetData(ScalarField2& sf);
	void GetDataStream(ScalarField2& sf);
	GLuint GetTerrainGLuint() const { return bedrockBuffer; };
	std::vector<BufferDescriptor> GetBuffers();
};


class GPU_Thermal : public IInspect {
public:
	GLuint simulationShader = 0;			//!< Compute shader

	GLuint bedrockBuffer = 0;				//!< Bedrock elevation buffer
	GLuint tempBedrockBuffer = 0;			//!< Temporary bedrock elevation buffer

	GLuint streamBuffer = 0;				//!< Stream area buffer
	GLuint tempStreamBuffer = 0;			//!< Temporary stream area buffer

	int nx = 0;
	int ny = 0;
	int totalBufferSize = 0;					//!< Total buffer size defined as nx * ny
	int dispatchSize = 0;						//!< Single dispatch size
	std::vector<float> tmpData;			        //!< Temporary array for retreiving GPU data
public:
	GPU_Thermal() {};
	~GPU_Thermal();

	void Init(const ScalarField2&, GLuint t_buffer);
	void Step(int n);

	void GetData(ScalarField2& sf);
	GLuint GetTerrainGLuint() const { return bedrockBuffer; };
	std::vector<BufferDescriptor> GetBuffers();
};


class GPU_Deposition : public IInspect {
public:
	GLuint simulationShader = 0;			//!< Compute shader

	GLuint bedrockBuffer = 0;				//!< Bedrock elevation buffer
	GLuint tempBedrockBuffer = 0;			//!< Temporary bedrock elevation buffer

	GLuint streamBuffer = 0;				//!< Stream area buffer
	GLuint tempStreamBuffer = 0;			//!< Temporary stream area buffer

	GLuint sedimentBuffer = 0;				//!< Suspended sediment buffer
	GLuint tempSedimentBuffer = 0;			//!< Temporary suspended sediment buffer

	int nx = 0;
	int ny = 0;
	int totalBufferSize = 0;					//!< Total buffer size defined as nx * ny
	int dispatchSize = 0;						//!< Single dispatch size

	float bedrockZmin = 0.0f;				//!< Minimum bedrock elevation
	float bedrockZmax = 1.0f;				//!< Maximum bedrock elevation

	std::vector<float> tmpData;			        //!< Temporary array for retreiving GPU data
public:
	GPU_Deposition() {};
	~GPU_Deposition();

	void Init(const ScalarField2&, GLuint t_buffer);
	void Step(int n);

	void GetData(ScalarField2& sf);
	GLuint GetTerrainGLuint() const { return bedrockBuffer; };
	std::vector<BufferDescriptor> GetBuffers();
};

class GPU_SoilDeposition : public GPU_Deposition, IInspect {


	std::vector<float> tmpSoiltex;
	std::vector<float> tmpSilt;
	std::vector<float> tmpSand;
	std::vector<float> tmpClay;

public:
	GLuint soiltexBuffer = 0;
	GLuint tempSoiltexBuffer = 0;
	GLuint sedtexBuffer = 0;
	GLuint tempSedtexBuffer = 0;
	~GPU_SoilDeposition();

	void Init(const ScalarField2 &hf, const ScalarField2 &siltf, const ScalarField2 &sandf, const ScalarField2 &clayf, GLuint
	          t_buffer);

	void Step(int n);

	void GetSoilData(ScalarField2 &siltf, ScalarField2 &sandf, ScalarField2 &clayf);
	std::vector<BufferDescriptor> GetBuffers();
};

class GPU_HydraulicErosion : public GPU_Deposition {
	// GLuint siltBuffer = 0;
	// GLuint tempSiltBuffer = 0;
	// GLuint sandBuffer = 0;
	// GLuint tempSandBuffer = 0;
	// GLuint clayBuffer = 0;
	// GLuint tempClayBuffer = 0;
	GLuint soilTexBuffer = 0;
	GLuint tempSoilTexBuffer = 0;
	GLuint waterBuffer = 0;
	GLuint tempWaterBuffer = 0;
	GLuint velocityBuffer = 0;
	GLuint tempVelocityBuffer = 0;
	GLuint fluxBuffer = 0;
	GLuint tempFluxBuffer = 0;
	// std::vector<float> tmpSilt;
	// std::vector<float> tmpSand;
	// std::vector<float> tmpClay;
	std::vector<float> tmpSoilTex3;

public:
	~GPU_HydraulicErosion();

	void Init(const ScalarField2 &hf, const ScalarField2 &siltf, const ScalarField2 &sandf, const ScalarField2 &clayf, GLuint
			  t_buffer);

	void Step(int n);

	void GetSoilData(ScalarField2 &siltf, ScalarField2 &sandf, ScalarField2 &clayf);
};





#endif