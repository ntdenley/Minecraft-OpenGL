#include <world/chunk.h>
#include <FastNoiseLite/FastNoiseLite.h>

// Macro for converting 3D coordinates to a 1D index
#define pos_to_index(x, y, z) static_cast<int>(x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE)

// Cube vertices
float CUBE_VERTS[] = {
    // Position     // Texture Coords
    // x,  y,  z,   u, v
        0, 0, 0,     0, 0, // north face (-z)
        1, 0, 0,     1, 0,
        1, 1, 0,     1, 1,
        0, 1, 0,     0, 1,

        0, 1, 1,     1, 1, // south face (+z)
        1, 1, 1,     0, 1,
        1, 0, 1,     0, 0,
        0, 0, 1,     1, 0, 

        0, 1, 0,     1, 1, // west face (-x)
        0, 1, 1,     0, 1,
        0, 0, 1,     0, 0,
        0, 0, 0,     1, 0, 

        1, 0, 0,     0, 0, // east face (+x)
        1, 0, 1,     1, 0,
        1, 1, 1,     1, 1,
        1, 1, 0,     0, 1,

        0, 0, 0,     0, 0, // bottom face (-y)
        0, 0, 1,     1, 0,
        1, 0, 1,     1, 1,
        1, 0, 0,     0, 1,

        
        1, 1, 0,     0, 1, // top face (+y)
        1, 1, 1,     1, 1,
        0, 1, 1,     1, 0,
        0, 1, 0,     0, 0,
};
// Cube indices
unsigned int CUBE_INDICES[] = { 0,  1,  2,  2,  3,  0 };

Chunk::Chunk(glm::vec3 offset, Shader *shaderProg) : offset(offset), shader(shaderProg) {
    // Initialize the chunk
    worldPos = offset * static_cast<float>(CHUNK_SIZE);

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

    // Generate the block data
    for(float x = 0; x < CHUNK_SIZE; x++)
    for(float y = 0; y < CHUNK_SIZE; y++)
    for(float z = 0; z < CHUNK_SIZE; z++){
        float y_noise = noise.GetNoise(x + worldPos.x, z + worldPos.z) * 10.0f;
        y_noise += pow(2, noise.GetNoise(x + worldPos.x, z + worldPos.z) * 4.0f);
        if(y+worldPos.y < y_noise)
            blockData[pos_to_index(x, y, z)] = BlockType::DIRT;
        else
            blockData[pos_to_index(x, y, z)] = BlockType::AIR;
    }
}

Chunk::~Chunk() {

}

Chunk::BlockType Chunk::GetBlockData(int x, int y, int z){
    if (InBounds(x, y, z))
        return blockData[pos_to_index(x, y, z)];
    return BlockType::AIR;
}

bool Chunk::InBounds(int x, int y, int z) {
    return x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE;
}

void Chunk::Generate() {
    // Generate the chunk
    for(int x = 0; x < CHUNK_SIZE; x++) {
    for(int y = 0; y < CHUNK_SIZE; y++) {
    for(int z = 0; z < CHUNK_SIZE; z++) {
        // Get the position of the block (relative to the chunk)
        glm::vec3 pos = glm::vec3(x, y, z);

        // Check if the block is solid, and if so, add the faces
        if(GetBlockData(x, y, z) != BlockType::AIR){
            // For each face, check adjacent blocks to see if face should be added
            if(!GetBlockData(x, y, z-1))
                AddFace(pos, Direction::NORTH);
            if(!GetBlockData(x, y, z+1))
                AddFace(pos, Direction::SOUTH);
            if(!GetBlockData(x-1, y, z))
                AddFace(pos, Direction::WEST);
            if(!GetBlockData(x+1, y, z))
                AddFace(pos, Direction::EAST);
            if(!GetBlockData(x, y-1, z))
                AddFace(pos, Direction::BOTTOM);
            if(!GetBlockData(x, y+1, z))
                AddFace(pos, Direction::TOP);
        }
    }}}

    generated = true;
}

void Chunk::Render() {
    if(!generated)
        return;

    if(!ready){
        // Generate the VAO, VBO, and EBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Bind the VAO to the GL State Machine
        glBindVertexArray(VAO);

        // Bind the vertex array object
        glBindVertexArray(VAO);

        // Bind the vertex buffer object
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        // Bind the element buffer object
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Tell OpenGL how to interpret the vertex data
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Texture Coord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Pseudo-lighting attribute
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // Texture layer attribute
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);

        ready = true;
    }

    // Bind the vertex array object
    glBindVertexArray(VAO);

    // Shift the chunk to the correct position
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, worldPos);
    shader->setMat4("model", model);

    // Draw the chunk
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

void Chunk::AddFace(glm::ivec3 pos, Direction direction) {

    float color = 1.0f;
    // Lighting
    switch(direction){
        case TOP:
            color = 1.0f;
            break;
        case NORTH:
        case SOUTH:
            color = 0.86f;
            break;
        case EAST:
        case WEST:
            color = 0.8f;
            break;
        case BOTTOM:
            color = 0.6f;
    }

    // Vertices
    int vert_offset = direction * 20;
    for(int i = 0; i < 4; i++) {
        float *ptr = &CUBE_VERTS[vert_offset + i * 5];

        // Position
        vertices.push_back(*ptr++ + pos.x);
        vertices.push_back(*ptr++ + pos.y);
        vertices.push_back(*ptr++ + pos.z);

        // Texture Coords
        vertices.push_back(*ptr++);
        vertices.push_back(*ptr);
        vertices.push_back(color);
        vertices.push_back(1); // Pushing 0 for now, will be used for texture layer later
    }

    // Indices
    for(int i = 0; i < 6; i++) {
        indices.push_back(CUBE_INDICES[i] + vertexCount);
    }
    
    // 4 new vertices and 6 new indices
    indexCount += 6;
    vertexCount += 4;
}