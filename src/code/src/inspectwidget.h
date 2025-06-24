//
// inspectwidget.h
//

#ifndef INSPECTWIDGET_H
#define INSPECTWIDGET_H

#include <GL/glew.h> // or your OpenGL loader (like glad)
#include <iostream>
#include <string>

struct ImVec2; // Forward declaration

class InspectWidget
{
public:
    InspectWidget();
    ~InspectWidget();

    void init(); // Call this after OpenGL context is available
    void create_framebuffer();
    void bind_framebuffer();
    void unbind_framebuffer();
    void rescale_framebuffer(float width, float height);

    void Draw();
    void render_imgui(); // Fill available content region
    void render_imgui(float width, float height); // Specific size

    // Label management for color legend
    void set_labels(const std::string& top_label, const std::string& bottom_left_label, const std::string& bottom_right_label);

    // Getter for the texture ID (useful for ImGui)
    GLuint get_texture_id() const { return texture_id; }

private:
    // Framebuffer objects
    GLuint FBO;         // Framebuffer Object
    GLuint texture_id;  // Color texture
    GLuint RBO;         // Renderbuffer Object (depth/stencil)

    // Triangle rendering objects
    GLuint VAO;         // Vertex Array Object
    GLuint VBO;         // Vertex Buffer Object
    GLuint shaderProgram; // Shader program

    // Framebuffer dimensions
    float width, height;

    // Viewport state saving
    GLint saved_viewport[4];

    // Initialization flag
    bool initialized;

    // Labels for color legend
    std::string label_top;
    std::string label_bottom_left;
    std::string label_bottom_right;

    // Helper methods
    void setup_triangle();
    GLuint compile_shader(const char* source, GLenum shader_type);
    GLuint create_shader_program();
    void add_labels_to_triangle(ImVec2 image_pos, ImVec2 image_size);
    void cleanup();
};

#endif // INSPECTWIDGET_H