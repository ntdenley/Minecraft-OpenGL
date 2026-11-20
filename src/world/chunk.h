#ifndef CHUNK_H
#define CHUNK_H

#include <vector>
#include <glm/glm.hpp>
#include <vfx/shader.h>

#define CHUNK_SIZE 32

// Sometimes less is more... just get the damn thing working and then refactor
// later on.
class Chunk {
    public:

        enum BlockType {
            AIR,
            DIRT,
            GRASS,
        };

        enum Direction {
            NORTH,
            SOUTH,
            WEST,
            EAST,
            BOTTOM,
            TOP
        };

        Chunk(glm::vec3 offset, Shader *shader);
        ~Chunk();

        void GenerateTerrain();
        // lod 0 is 1x1x1. Each level doubles the block (2, 4, 8, ...).
        // neighborSteps: NORTH, SOUTH, WEST, EAST. 0 means that neighbor is not drawn.
        void GenerateMesh(int lod, const int neighborSteps[4], bool drawBottoms);
        // Uploads the CPU mesh to the GPU. Main thread only. Returns true when a buffer was uploaded.
        bool Upload();
        void Render();

        bool terrain_generated = false;
        bool voxels_ready = false;
        bool voxels_queued = false;
        bool mesh_generated = false;
        bool mesh_in_flight = false;
        int lod = -1;
        int built_lod = -1;
        bool mesh_bottoms = false;
        bool built_bottoms = false;
        int drawCount = 0;

        BlockType GetBlockData(int x, int y, int z) const;
        bool InBounds(int x, int y, int z) const;

        // Filled only for chunks near the player. Distant chunks are meshed from the noise directly.
        std::vector<BlockType> blockData;

        glm::vec3 offset;
        int vertexCount = 0, indexCount = 0;
        bool generated = false;
        bool ready = false;


    private:
        void AddQuad(float x0, float y0, float z0, float u0, float v0,
                     float x1, float y1, float z1, float u1, float v1,
                     float x2, float y2, float z2, float u2, float v2,
                     float x3, float y3, float z3, float u3, float v3,
                     float color, float layer);
        void AddBlockFace(Direction direction, float x, float y, float z, float w, float h, float d, float v0, float v1, float layer);

        unsigned int VBO = 0, VAO = 0, EBO = 0;
        int modelUniform = -1;
        glm::vec3 worldPos;
        Shader *shader;

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
};

#endif