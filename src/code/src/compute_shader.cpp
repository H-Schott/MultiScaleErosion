//
// Created by User on 2025/08/19.
//


#include "compute_shader.h"
#include "shader-api.h"


void init_shader(ComputeShader& shader, const char* full_filepath) {
    if (shader.program == 0) {
        shader.program = read_program(full_filepath);
    }
    if (shader.program == 0) {
        fprintf(stderr, "Failed to load compute shader from %s\n", full_filepath);
        return;
    }
    glUseProgram(shader.program);
    GLint num_buffers = 0;
    glGetProgramInterfaceiv(shader.program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &num_buffers);

    std::vector<GLchar> name_data(256);
    std::vector<GLenum> properties;
    properties.push_back(GL_NAME_LENGTH);
    properties.push_back(GL_TYPE);
    properties.push_back(GL_ARRAY_SIZE);
    properties.push_back(GL_BUFFER_BINDING);
    properties.push_back(GL_BUFFER_DATA_SIZE);
    std::vector<GLint> values(properties.size());
    for (int buf = 0; buf < num_buffers; ++buf) {
        glGetProgramResourceiv(shader.program, GL_SHADER_STORAGE_BLOCK, buf, properties.size(), properties.data(), values.size(), nullptr, values.data());

        name_data.resize(values[0]);
        glGetProgramResourceName(shader.program, GL_SHADER_STORAGE_BLOCK, buf, name_data.size(), nullptr, name_data.data());

        ComputeBuffer buffer;
        buffer.id = values[3];
        buffer.size = values[4];
    }
}

// void buffer_data(ComputeShader &shader, const std::string &name, )
// {
//
// }
