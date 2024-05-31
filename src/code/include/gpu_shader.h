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

	void Init(const ScalarField2&);
	void Step(int n);

	void GetData(ScalarField2& sf);
};


class GPU_Deposition {
private:
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

	void Init(const ScalarField2&);
	void Step(int n);

	void SetHardness(const ScalarField2& uplift) const;
	void GetData(ScalarField2& sf);
};


#endif