#ifndef __GPU_SHADER__
#define __GPU_SHADER__

#include <GL/glew.h>
#include <vector>
#include <string>

#include "scalarfield2.h"
#include "shader-api.h"

class GPU_Erosion {
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
public:
	GPU_Erosion() {};
	~GPU_Erosion();

	void Init(const ScalarField2& hf, GLuint t_buffer);
	void Step(int n);

	void SetHardness(const ScalarField2& hardness) const;
	void GetData(ScalarField2& sf);
	void GetDataStream(ScalarField2& sf);
	GLuint GetTerrainGLuint() const { return bedrockBuffer; };
};


class GPU_Thermal {
private:
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
};


class GPU_Deposition {
protected:
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
	std::vector<float> tmpData;			        //!< Temporary array for retreiving GPU data
public:
	GPU_Deposition() {};
	~GPU_Deposition();

	void Init(const ScalarField2&, GLuint t_buffer);
	void Step(int n);

	void GetData(ScalarField2& sf);
	GLuint GetTerrainGLuint() const { return bedrockBuffer; };
};

class GPU_SoilDeposition : public GPU_Deposition {
	GLuint siltBuffer = 0;
	GLuint tempSiltBuffer = 0;
	GLuint sandBuffer = 0;
	GLuint tempSandBuffer = 0;
	GLuint clayBuffer = 0;
	GLuint tempClayBuffer = 0;
	std::vector<float> tmpSilt;
	std::vector<float> tmpSand;
	std::vector<float> tmpClay;

public:
	~GPU_SoilDeposition();

	void Init(const ScalarField2 &hf, const ScalarField2 &siltf, const ScalarField2 &sandf, const ScalarField2 &clayf, GLuint
	          t_buffer);

	void Step(int n);

	void GetSoilData(ScalarField2 &siltf, ScalarField2 &sandf, ScalarField2 &clayf);
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