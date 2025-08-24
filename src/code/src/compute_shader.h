//
// Created by User on 2025/08/19.
//

#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "GL/glew.h"

struct ComputeBuffer {
    GLuint id = 0;  //!< Buffer ID
    GLuint temp_buffer = 0; //!< Temporary buffer ID
    size_t size = 0;    //!< Size of the buffer in bytes
    ComputeBuffer();
    ~ComputeBuffer();
};

struct ComputeShader {
    std::unordered_map<std::string, ComputeBuffer> buffers;
    GLuint program = 0;  //!< Compute shader program ID
    explicit ComputeShader(const char* filepath);
    ~ComputeShader();
};

void init_shader(ComputeShader& shader, const char* vertex_shader, const char* fragment_shader);
void add_buffer(ComputeShader& shader, const std::string& name, size_t size);
void run_shader(ComputeShader& shader, int steps);

template<typename T>
void buffer_data(ComputeShader& shader, const std::string& name, std::vector<T> data)
{
    if (shader.buffers.find(name) == shader.buffers.end()) {
        ComputeBuffer buffer;
        shader.buffers[name] = buffer;
    }
    ComputeBuffer& buffer = shader.buffers[name];
    if (buffer.id == 0) glGenBuffers(1, &buffer.id);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer.id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(T) * data.size(), &data.front(), GL_STREAM_READ);
}



#endif //COMPUTE_SHADER_H
