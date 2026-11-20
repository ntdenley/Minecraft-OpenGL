#ifndef TEXTURES_H
#define TEXTURES_H

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <vfx/stb_image.h>
#include <glad/glad.h> // OpenGL functions

#define TEXTURE_SIZE 16

using namespace std;
namespace fs = filesystem;

vector<string> getTextureFiles(const string &dir){
    vector<string> files;
    for(const auto &entry : fs::directory_iterator(dir))
        files.push_back(entry.path().string());

    return files;
}

bool isCoreTexture(const string& path) {
    string name = fs::path(path).filename().string();
    return name == "dirt.png" || name == "grass_side.png" || name == "grass_top.png";
}

void loadTextures(){
    // Fixed order so mesh layers stay stable: 0 dirt, 1 grass side, 2 grass top.
    vector<string> files = {
        "assets\\textures\\dirt.png",
        "assets\\textures\\grass_side.png",
        "assets\\textures\\grass_top.png",
    };
    for (const string& extra : getTextureFiles("assets\\textures")) {
        if (!isCoreTexture(extra))
            files.push_back(extra);
    }

    stbi_set_flip_vertically_on_load(true);

    // Set the texture parameters
    unsigned char *data;
    int width, height, num_channels;
    unsigned int texture;
    int layerCount = files.size();

    // Create and bind the texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    // 16, 8, 4, 2, 1. Immutable storage has to reserve every level up front.
    int mipLevelCount = 1;
    for (int size = TEXTURE_SIZE; size > 1; size /= 2)
        mipLevelCount++;
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipLevelCount, GL_RGBA8, TEXTURE_SIZE, TEXTURE_SIZE, layerCount);

    // Load the textures
    int i = 0;
    for(string file: files){
        data = stbi_load(file.c_str(), &width, &height, &num_channels, 4);
        if(data){
            // Ensure the texture is the correct size
            if (width != TEXTURE_SIZE || height != TEXTURE_SIZE) {
                cout << "Texture size mismatch for file: " << file << endl;
                stbi_image_free(data);
                continue;
            }
            // Load the texture into the texture array
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            i++;
            cout << "Loaded texture: " << file << endl;
        }
        else{
            cout << "Failed to load texture" << endl;
        }
    }

    // Set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT); // set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Build the smaller levels from the 16x16 images, then sample them at distance.
    // Nearest within a level keeps the pixels crisp; blending between levels stops the shimmer.
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Bind the texture array to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
}

#endif