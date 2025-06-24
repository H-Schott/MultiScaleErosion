//
// Created by User on 2025/06/23.
//

#include "inspectwidget.h"
#include <iostream>
#include <GL/glew.h> // or your OpenGL loader
#include <imgui.h>

// Vertex shader source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 vertexColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    vertexColor = aColor;
}
)";

// Fragment shader source
const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vertexColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vertexColor, 1.0);
}
)";

InspectWidget::InspectWidget() : FBO(0), texture_id(0), RBO(0), VAO(0), VBO(0), shaderProgram(0), width(800), height(600), initialized(false)
{
    // OpenGL objects will be created in init()
}

InspectWidget::~InspectWidget()
{
    cleanup();
}

void InspectWidget::init()
{
    if (initialized) return;

    create_framebuffer();
    setup_triangle();
    initialized = true;
}

void InspectWidget::create_framebuffer()
{
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void InspectWidget::setup_triangle()
{
    // Triangle vertices with positions and colors (RGB triangle)
    float vertices[] = {
        // Positions        // Colors
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // Top vertex - Red
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // Bottom left - Green
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // Bottom right - Blue
    };

    // Create and compile shaders
    shaderProgram = create_shader_program();

    // Generate and bind VAO
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Generate and bind VBO
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

GLuint InspectWidget::compile_shader(const char* source, GLenum shader_type)
{
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

GLuint InspectWidget::create_shader_program()
{
    GLuint vertexShader = compile_shader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compile_shader(fragmentShaderSource, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check for linking errors
    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    // Clean up individual shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

// here we bind our framebuffer
void InspectWidget::bind_framebuffer()
{
    // Save current viewport
    glGetIntegerv(GL_VIEWPORT, saved_viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glViewport(0, 0, width, height);
}

// here we unbind our framebuffer
void InspectWidget::unbind_framebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore original viewport
    glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
}

// and we rescale the buffer, so we're able to resize the window
void InspectWidget::rescale_framebuffer(float new_width, float new_height)
{
    width = new_width;
    height = new_height;

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
}

void InspectWidget::Draw()
{
    // Initialize OpenGL objects if not done yet
    if (!initialized) {
        init();
    }

    // Bind our framebuffer to render to it
    bind_framebuffer();

    // Clear the framebuffer
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Use our shader program
    glUseProgram(shaderProgram);

    // Bind VAO and draw triangle
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // Unbind framebuffer
    unbind_framebuffer();
}

void InspectWidget::render_imgui()
{
    // Initialize if not done yet
    if (!initialized) {
        init();
    }

    // Get the available space for the image
    ImVec2 available_size = ImGui::GetContentRegionAvail();

    // Resize framebuffer if needed
    if (available_size.x != width || available_size.y != height) {
        rescale_framebuffer(available_size.x, available_size.y);
        Draw(); // Re-render with new size
    }

    // Display the framebuffer texture in ImGui
    // Note: ImGui expects textures to be flipped vertically
    ImVec2 image_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<void*>(texture_id),
                 ImVec2(width, height),
                 ImVec2(0, 1), ImVec2(1, 0)); // Flip UV coordinates

    // Add labels if they are set
    add_labels_to_triangle(image_pos, ImVec2(width, height));
}

void InspectWidget::render_imgui(float display_width, float display_height)
{
    // Initialize if not done yet
    if (!initialized) {
        init();
    }

    // Resize framebuffer if needed
    if (display_width != width || display_height != height) {
        rescale_framebuffer(display_width, display_height);
        Draw(); // Re-render with new size
    }

    // Display the framebuffer texture in ImGui with specified size
    ImVec2 image_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<void*>(texture_id),
                 ImVec2(display_width, display_height),
                 ImVec2(0, 1), ImVec2(1, 0)); // Flip UV coordinates

    // Add labels if they are set
    add_labels_to_triangle(image_pos, ImVec2(display_width, display_height));
}

void InspectWidget::set_labels(const std::string& top_label, const std::string& bottom_left_label, const std::string& bottom_right_label)
{
    label_top = top_label;
    label_bottom_left = bottom_left_label;
    label_bottom_right = bottom_right_label;
}

void InspectWidget::add_labels_to_triangle(ImVec2 image_pos, ImVec2 image_size)
{
    if (label_top.empty() && label_bottom_left.empty() && label_bottom_right.empty()) {
        return; // No labels to draw
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 text_color = IM_COL32(255, 255, 255, 255); // White text
    ImU32 outline_color = IM_COL32(0, 0, 0, 200); // Black outline

    // Calculate label positions based on triangle vertices
    // Triangle vertices in normalized coordinates: (0.5, 0.1), (0.1, 0.9), (0.9, 0.9)

    // Top vertex (Red) - center top
    if (!label_top.empty()) {
        ImVec2 pos = ImVec2(image_pos.x + image_size.x * 0.5f,
                           image_pos.y + image_size.y * 0.05f); // Slightly above top vertex

        // Center the text horizontally
        ImVec2 text_size = ImGui::CalcTextSize(label_top.c_str());
        pos.x -= text_size.x * 0.5f;

        // Draw text outline
        draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), outline_color, label_top.c_str());
        draw_list->AddText(ImVec2(pos.x - 1, pos.y - 1), outline_color, label_top.c_str());
        draw_list->AddText(ImVec2(pos.x + 1, pos.y - 1), outline_color, label_top.c_str());
        draw_list->AddText(ImVec2(pos.x - 1, pos.y + 1), outline_color, label_top.c_str());

        // Draw main text
        draw_list->AddText(pos, text_color, label_top.c_str());
    }

    // Bottom-left vertex (Green)
    if (!label_bottom_left.empty()) {
        ImVec2 pos = ImVec2(image_pos.x + image_size.x * 0.05f,
                           image_pos.y + image_size.y * 0.95f); // Slightly below and left of vertex

        // Draw text outline
        draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), outline_color, label_bottom_left.c_str());
        draw_list->AddText(ImVec2(pos.x - 1, pos.y - 1), outline_color, label_bottom_left.c_str());
        draw_list->AddText(ImVec2(pos.x + 1, pos.y - 1), outline_color, label_bottom_left.c_str());
        draw_list->AddText(ImVec2(pos.x - 1, pos.y + 1), outline_color, label_bottom_left.c_str());

        // Draw main text
        draw_list->AddText(pos, text_color, label_bottom_left.c_str());
    }

    // Bottom-right vertex (Blue)
    if (!label_bottom_right.empty()) {
        ImVec2 pos = ImVec2(image_pos.x + image_size.x * 0.95f,
                           image_pos.y + image_size.y * 0.95f); // Slightly below and right of vertex

        // Right-align the text
        ImVec2 text_size = ImGui::CalcTextSize(label_bottom_right.c_str());
        pos.x -= text_size.x;

        // Draw text outline
        draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), outline_color, label_bottom_right.c_str());
        draw_list->AddText(ImVec2(pos.x - 1, pos.y - 1), outline_color, label_bottom_right.c_str());
        draw_list->AddText(ImVec2(pos.x + 1, pos.y - 1), outline_color, label_bottom_right.c_str());
        draw_list->AddText(ImVec2(pos.x - 1, pos.y + 1), outline_color, label_bottom_right.c_str());

        // Draw main text
        draw_list->AddText(pos, text_color, label_bottom_right.c_str());
    }
}

void InspectWidget::cleanup()
{
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (texture_id) glDeleteTextures(1, &texture_id);
    if (RBO) glDeleteRenderbuffers(1, &RBO);
    if (FBO) glDeleteFramebuffers(1, &FBO);
}