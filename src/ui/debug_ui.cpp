#include <ui/debug_ui.h>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

const char* kVertexShader = R"(
#version 460 core
layout (location = 0) in vec2 aPos;
uniform mat4 projection;
void main()
{
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

const char* kFragmentShader = R"(
#version 460 core
out vec4 FragColor;
uniform vec4 color;
void main()
{
    FragColor = color;
}
)";

// 5x7 glyphs. Bit 4 is the leftmost column.
const unsigned char* GlyphRows(char c) {
    static const unsigned char k0[] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const unsigned char k1[] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const unsigned char k2[] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const unsigned char k3[] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    static const unsigned char k4[] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const unsigned char k5[] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    static const unsigned char k6[] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const unsigned char k7[] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const unsigned char k8[] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const unsigned char k9[] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    static const unsigned char kF[] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const unsigned char kP[] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const unsigned char kS[] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const unsigned char kColon[] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};

    switch (c) {
        case '0': return k0;
        case '1': return k1;
        case '2': return k2;
        case '3': return k3;
        case '4': return k4;
        case '5': return k5;
        case '6': return k6;
        case '7': return k7;
        case '8': return k8;
        case '9': return k9;
        case 'F': return kF;
        case 'P': return kP;
        case 'S': return kS;
        case ':': return kColon;
        default: return nullptr;
    }
}

unsigned int CompileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "Debug UI shader compile error:\n%s\n", infoLog);
    }
    return shader;
}

void AppendQuad(std::vector<float>& vertices, float x, float y, float w, float h) {
    float x1 = x + w;
    float y1 = y + h;
    float quad[] = {
        x,  y,
        x1, y,
        x1, y1,
        x,  y,
        x1, y1,
        x,  y1
    };
    vertices.insert(vertices.end(), quad, quad + 12);
}

void AppendText(std::vector<float>& vertices, const char* text, float originX, float originY, float scale) {
    const float advance = 6.0f * scale;
    float cursorX = originX;

    for (const char* c = text; *c != '\0'; ++c) {
        const unsigned char* rows = GlyphRows(*c);
        if (rows != nullptr) {
            for (int row = 0; row < 7; ++row) {
                for (int col = 0; col < 5; ++col) {
                    if (rows[row] & (1u << (4 - col)))
                        AppendQuad(vertices, cursorX + col * scale, originY + row * scale, scale, scale);
                }
            }
        }
        cursorX += advance;
    }
}

void DrawVertices(unsigned int vbo, const std::vector<float>& vertices) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 2));
}

} // namespace

void DebugUI::Init() {
    unsigned int vertex = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    unsigned int fragment = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);

    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "Debug UI program link error:\n%s\n", infoLog);
        glDeleteProgram(program);
        program = 0;
        return;
    }

    projectionLocation = glGetUniformLocation(program, "projection");
    colorLocation = glGetUniformLocation(program, "color");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void DebugUI::Shutdown() {
    if (vbo != 0)
        glDeleteBuffers(1, &vbo);
    if (vao != 0)
        glDeleteVertexArrays(1, &vao);
    if (program != 0)
        glDeleteProgram(program);
    vbo = 0;
    vao = 0;
    program = 0;
}

void DebugUI::SetFps(int value) {
    fps = value;
}

void DebugUI::Toggle() {
    visible = !visible;
}

void DebugUI::Render(float width, float height) {
    if (!visible || program == 0 || width <= 0.0f || height <= 0.0f)
        return;

    char label[32];
    std::snprintf(label, sizeof(label), "FPS: %d", fps);

    const float scale = 4.0f;
    const float originX = 18.0f;
    const float originY = 18.0f;
    const float pad = 10.0f;
    const float textWidth = static_cast<float>(std::strlen(label)) * 6.0f * scale;
    const float textHeight = 7.0f * scale;

    std::vector<float> panel;
    std::vector<float> shadow;
    std::vector<float> glyphs;
    AppendQuad(panel, originX - pad, originY - pad, textWidth + pad * 2.0f, textHeight + pad * 2.0f);
    AppendText(shadow, label, originX + 2.0f, originY + 2.0f, scale);
    AppendText(glyphs, label, originX, originY, scale);

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    glGetIntegerv(GL_POLYGON_MODE, polygonMode);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(program);
    glm::mat4 projection = glm::ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));
    glBindVertexArray(vao);

    glUniform4f(colorLocation, 0.0f, 0.0f, 0.0f, 0.45f);
    DrawVertices(vbo, panel);
    glUniform4f(colorLocation, 0.0f, 0.0f, 0.0f, 1.0f);
    DrawVertices(vbo, shadow);
    glUniform4f(colorLocation, 1.0f, 1.0f, 1.0f, 1.0f);
    DrawVertices(vbo, glyphs);

    glBindVertexArray(0);

    if (depthEnabled)
        glEnable(GL_DEPTH_TEST);
    if (cullEnabled)
        glEnable(GL_CULL_FACE);
    if (!blendEnabled)
        glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
}
