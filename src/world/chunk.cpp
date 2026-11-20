#include <world/chunk.h>
#include <FastNoiseLite/FastNoiseLite.h>

#include <algorithm>
#include <cmath>
#include <limits>

// Macro for converting 3D coordinates to a 1D index
#define pos_to_index(x, y, z) static_cast<int>(x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE)

Chunk::Chunk(glm::vec3 offset, Shader *shaderProg) : offset(offset), shader(shaderProg) {
    // Initialize the chunk
    worldPos = offset * static_cast<float>(CHUNK_SIZE);
}

Chunk::~Chunk() {

}

Chunk::BlockType Chunk::GetBlockData(int x, int y, int z) const {
    if (voxels_ready && InBounds(x, y, z))
        return blockData[pos_to_index(x, y, z)];
    return BlockType::AIR;
}

bool Chunk::InBounds(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE;
}

namespace {

int AlignDown(int value, int step) {
    int q = value / step;
    if (value % step < 0)
        --q;
    return q * step;
}

// Highest solid block. Matches "solid where worldY < noise height".
int SurfaceTopInclusive(FastNoiseLite& noise, int worldX, int worldZ) {
    float n = noise.GetNoise(static_cast<float>(worldX), static_cast<float>(worldZ));
    float height = n * 10.0f + static_cast<float>(std::pow(2.0f, n * 4.0f));
    int y = static_cast<int>(std::floor(height));
    if (static_cast<float>(y) >= height)
        --y;
    return y;
}

// One height for the whole coarse column, taken at its center so the surface keeps the hill shape.
int ColumnFillTop(FastNoiseLite& noise, int worldX, int worldZ, int step) {
    return SurfaceTopInclusive(noise, worldX + step / 2, worldZ + step / 2) + 1;
}

}

void Chunk::GenerateTerrain() {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

    const int originX = static_cast<int>(std::lround(worldPos.x));
    const int originY = static_cast<int>(std::lround(worldPos.y));
    const int originZ = static_cast<int>(std::lround(worldPos.z));

    blockData.assign(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, BlockType::AIR);

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int surface = SurfaceTopInclusive(noise, originX + x, originZ + z);
            for (int y = 0; y < CHUNK_SIZE; ++y) {
                int worldY = originY + y;
                BlockType block = BlockType::AIR;
                if (worldY < surface)
                    block = BlockType::DIRT;
                else if (worldY == surface)
                    block = BlockType::GRASS;
                blockData[pos_to_index(x, y, z)] = block;
            }
        }
    }

    voxels_ready = true;
    terrain_generated = true;
}

void Chunk::GenerateMesh(int lod, const int neighborSteps[4], bool drawBottoms) {
    vertices.clear();
    indices.clear();
    vertexCount = 0;
    indexCount = 0;

    if (lod < 0)
        lod = 0;
    int step = 1 << lod;
    if (step > CHUNK_SIZE)
        step = CHUNK_SIZE;

    built_lod = lod;
    built_bottoms = drawBottoms;

    if (!terrain_generated) {
        mesh_generated = true;
        return;
    }

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);

    const int originX = static_cast<int>(std::lround(worldPos.x));
    const int originY = static_cast<int>(std::lround(worldPos.y));
    const int originZ = static_cast<int>(std::lround(worldPos.z));
    const int chunkTopY = originY + CHUNK_SIZE;
    const int cells = CHUNK_SIZE / step;

    std::vector<int> fill(static_cast<size_t>(cells * cells));
    for (int cz = 0; cz < cells; ++cz)
        for (int cx = 0; cx < cells; ++cx)
            fill[cz * cells + cx] = ColumnFillTop(noise, originX + cx * step, originZ + cz * step, step);

    const float dirt = 0.0f;
    const float grassSide = 1.0f;
    const float grassTop = 2.0f;
    const int noNeighbor = std::numeric_limits<int>::min();

    auto outsideTop = [&](Direction dir, int worldX, int worldZ) {
        int nStep = neighborSteps[dir];
        if (nStep <= 0)
            return noNeighbor;
        return ColumnFillTop(noise, AlignDown(worldX, nStep), AlignDown(worldZ, nStep), nStep);
    };

    for (int cz = 0; cz < cells; ++cz) {
        for (int cx = 0; cx < cells; ++cx) {
            const int top = fill[cz * cells + cx];
            const int yEnd = std::min(chunkTopY, top);
            if (yEnd <= originY)
                continue;

            const float colX = static_cast<float>(cx * step);
            const float colZ = static_cast<float>(cz * step);
            const float block = static_cast<float>(step);

            if (top > originY && top <= chunkTopY)
                AddBlockFace(Direction::TOP, colX, static_cast<float>(top - originY), colZ, block, 0.0f, block, 0.0f, 1.0f, grassTop);

            if (drawBottoms)
                AddBlockFace(Direction::BOTTOM, colX, 0.0f, colZ, block, 0.0f, block, 0.0f, 1.0f, dirt);

            const Direction sides[4] = { Direction::NORTH, Direction::SOUTH, Direction::WEST, Direction::EAST };
            for (Direction dir : sides) {
                const bool border =
                    (dir == Direction::NORTH && cz == 0) ||
                    (dir == Direction::SOUTH && cz == cells - 1) ||
                    (dir == Direction::WEST && cx == 0) ||
                    (dir == Direction::EAST && cx == cells - 1);

                int nStep = step;
                if (border) {
                    nStep = neighborSteps[dir];
                    if (nStep < 0)
                        nStep = 0;
                }
                const int seg = (nStep <= 0) ? step : std::min(step, nStep);

                for (int s = 0; s < step; s += seg) {
                    int nTop = noNeighbor;
                    if (!border) {
                        if (dir == Direction::NORTH)
                            nTop = fill[(cz - 1) * cells + cx];
                        else if (dir == Direction::SOUTH)
                            nTop = fill[(cz + 1) * cells + cx];
                        else if (dir == Direction::WEST)
                            nTop = fill[cz * cells + (cx - 1)];
                        else
                            nTop = fill[cz * cells + (cx + 1)];
                    } else if (nStep > 0) {
                        int sampleX = originX + cx * step;
                        int sampleZ = originZ + cz * step;
                        if (dir == Direction::NORTH || dir == Direction::SOUTH)
                            sampleX += s;
                        else
                            sampleZ += s;
                        if (dir == Direction::NORTH)
                            sampleZ = originZ - 1;
                        else if (dir == Direction::SOUTH)
                            sampleZ = originZ + cz * step + step;
                        else if (dir == Direction::WEST)
                            sampleX = originX - 1;
                        else
                            sampleX = originX + cx * step + step;
                        nTop = outsideTop(dir, sampleX, sampleZ);
                    }

                    const int expose0 = std::max(originY, nTop);
                    const int expose1 = yEnd;
                    if (expose1 <= expose0)
                        continue;

                    for (int y = AlignDown(expose0, step); y < expose1; y += step) {
                        const int yA = std::max(y, expose0);
                        const int yB = std::min(y + step, expose1);
                        if (yB <= yA)
                            continue;

                        const bool topCube = (y + step >= top);
                        const float layer = topCube ? grassSide : dirt;
                        const float v0 = static_cast<float>(yA - y) / static_cast<float>(step);
                        const float v1 = static_cast<float>(yB - y) / static_cast<float>(step);
                        const float localY = static_cast<float>(yA - originY);
                        const float localH = static_cast<float>(yB - yA);
                        const float localS = static_cast<float>(s);
                        const float segF = static_cast<float>(seg);

                        if (dir == Direction::NORTH)
                            AddBlockFace(dir, colX + localS, localY, colZ, segF, localH, block, v0, v1, layer);
                        else if (dir == Direction::SOUTH)
                            AddBlockFace(dir, colX + localS, localY, colZ, segF, localH, block, v0, v1, layer);
                        else if (dir == Direction::WEST)
                            AddBlockFace(dir, colX, localY, colZ + localS, block, localH, segF, v0, v1, layer);
                        else
                            AddBlockFace(dir, colX, localY, colZ + localS, block, localH, segF, v0, v1, layer);
                    }
                }
            }
        }
    }

    mesh_generated = true;
}

bool Chunk::Upload() {
    if (vertices.empty() || indexCount == 0) {
        drawCount = 0;
        ready = true;
        std::vector<float>().swap(vertices);
        std::vector<unsigned int>().swap(indices);
        return false;
    }

    if (VAO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
    } else {
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    }

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    drawCount = indexCount;
    ready = true;
    std::vector<float>().swap(vertices);
    std::vector<unsigned int>().swap(indices);
    return true;
}

void Chunk::Render() {
    if (!ready || drawCount == 0)
        return;

    glBindVertexArray(VAO);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, worldPos);
    if (modelUniform < 0)
        modelUniform = glGetUniformLocation(shader->ID, "model");
    glUniformMatrix4fv(modelUniform, 1, GL_FALSE, glm::value_ptr(model));

    glDrawElements(GL_TRIANGLES, drawCount, GL_UNSIGNED_INT, 0);
}

void Chunk::AddQuad(float x0, float y0, float z0, float u0, float v0,
                    float x1, float y1, float z1, float u1, float v1,
                    float x2, float y2, float z2, float u2, float v2,
                    float x3, float y3, float z3, float u3, float v3,
                    float color, float layer) {
    float quad[] = {
        x0, y0, z0, u0, v0, color, layer,
        x1, y1, z1, u1, v1, color, layer,
        x2, y2, z2, u2, v2, color, layer,
        x3, y3, z3, u3, v3, color, layer
    };
    vertices.insert(vertices.end(), quad, quad + 28);

    unsigned int base = static_cast<unsigned int>(vertexCount);
    unsigned int inds[] = { base, base + 1, base + 2, base + 2, base + 3, base };
    indices.insert(indices.end(), inds, inds + 6);
    indexCount += 6;
    vertexCount += 4;
}

void Chunk::AddBlockFace(Direction direction, float x, float y, float z, float w, float h, float d, float v0, float v1, float layer) {
    float color = 1.0f;
    switch (direction) {
        case TOP: color = 1.0f; break;
        case NORTH:
        case SOUTH: color = 0.86f; break;
        case EAST:
        case WEST: color = 0.8f; break;
        case BOTTOM: color = 0.6f; break;
    }

    switch (direction) {
        case NORTH:
            AddQuad(x, y, z, 0, v0,  x + w, y, z, 1, v0,  x + w, y + h, z, 1, v1,  x, y + h, z, 0, v1, color, layer);
            break;
        case SOUTH:
            AddQuad(x, y + h, z + d, 1, v1,  x + w, y + h, z + d, 0, v1,  x + w, y, z + d, 0, v0,  x, y, z + d, 1, v0, color, layer);
            break;
        case WEST:
            AddQuad(x, y + h, z, 1, v1,  x, y + h, z + d, 0, v1,  x, y, z + d, 0, v0,  x, y, z, 1, v0, color, layer);
            break;
        case EAST:
            AddQuad(x + w, y, z, 0, v0,  x + w, y, z + d, 1, v0,  x + w, y + h, z + d, 1, v1,  x + w, y + h, z, 0, v1, color, layer);
            break;
        case TOP:
            AddQuad(x + w, y, z, 0, 1,  x + w, y, z + d, 1, 1,  x, y, z + d, 1, 0,  x, y, z, 0, 0, color, layer);
            break;
        case BOTTOM:
            AddQuad(x, y, z, 0, 0,  x, y, z + d, 1, 0,  x + w, y, z + d, 1, 1,  x + w, y, z, 0, 1, color, layer);
            break;
    }
}