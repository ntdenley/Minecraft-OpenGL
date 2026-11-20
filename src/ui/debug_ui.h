#pragma once

class DebugUI {
public:
    void Init();
    void Shutdown();
    void SetFps(int fps);
    void Render(float width, float height);
    void Toggle();

private:
    bool visible = true;
    int fps = 0;
    unsigned int program = 0;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int projectionLocation = -1;
    int colorLocation = -1;
};
