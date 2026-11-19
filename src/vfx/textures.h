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

void loadTextures(){
    vector<string> files = getTextureFiles("assets/textures");

    stbi_set_flip_vertically_on_load(true);

    // Set the texture parameters
    unsigned char *data;
    int width, height, num_channels;
    unsigned int texture;
    int layerCount = files.size();

    // Create and bind the texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    // Allocate the storage for the texture
    int mipLevelCount = 1;
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

    // Set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Bind the texture array to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
}

#endif