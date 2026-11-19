# Minecraft C++ Game

This project is a Minecraft-like game written in C++ and utilizes OpenGL for rendering. The main focus of this project is to create a highly-optimized chunk-loading system that can render further and faster than typical Minecraft.

## Features

- Minecraft-like gameplay experience
- Optimized chunk-loading system
- Utilizes OpenGL for rendering

## Dependencies

This project depends on the following libraries:

- GLFW 3.3+ (system package; Windows users can keep using the bundled `lib/` / `glfw3.dll`)
- GLAD (vendored under `lib/glad` and `include/glad`)
- GLM and FastNoiseLite (vendored under `include/`)

### Linux build

```bash
# Debian/Ubuntu
sudo apt-get install -y build-essential cmake libglfw3-dev libgl1-mesa-dev

cmake -S . -B build
cmake --build build -j
```

The binary is written to `src/minecraft`. Run it from `src/` so shader and texture paths resolve:

```bash
cd src
./minecraft
```

Requires a working display / OpenGL context (hardware GPU or Mesa llvmpipe).

## License

This project is licensed under the [MIT License](LICENSE).

## OpenGL

This project wouldn't be possible without help from the [Learn OpenGL Online Book](https://learnopengl.com)

## Inspiration

This project is inspired by a similar project by [jdh on YouTube](https://www.youtube.com/@jdh). Check out his project [here](https://github.com/jdah/minecraft-weekend).