#ifndef WORLD_H
#define WORLD_H

#include <vector>
#include <unordered_map>
#include <queue>
#include <set>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <functional>

#include <util/hashtuple.h>
#include <world/chunk.h>

using namespace std;

class World {
    public:
        // num_threads == 0 keeps two cores free for the render thread.
        World(Shader *shader, size_t num_threads = 0);
        ~World();
        void queue_task(function<void()> task);

        vector<Chunk::BlockType> GetChunkData(int chunk_x, int chunk_y, int chunk_z);
        void Update(glm::vec3 player_pos, const glm::mat4& viewProj);

        Chunk* GetChunk(int chunk_x, int chunk_y, int chunk_z);
        void GenerateChunk(glm::ivec3 chunk_pos, bool storeVoxels);
        void UpdateChunks();

        // Global world pointer
        static World *world;
        unsigned int num_chunks = 0, num_chunks_rendered = 0;
    
    private:
        void IntegrateGeneratedChunks();
        void RequestTerrain(int originX, int originY, int originZ);
        void QueueMeshes(int originX, int originY, int originZ);
        void UploadMeshes();

        unordered_map<tuple<int, int, int>, Chunk*> update_chunks;
        // Main thread only. Workers hand finished terrain back through chunks_to_add.
        unordered_map<tuple<int, int, int>, Chunk*> chunks;
        unordered_map<tuple<int, int, int>, Chunk*> chunks_to_add;
        vector<Chunk*> draw_list;

        queue<glm::vec3> chunk_queue;

        set<tuple<int, int, int>> chunks_to_render;
        // Full-detail terrain covers the play area. Coarser blocks only appear near the horizon.
        int render_distance = 40;
        int voxel_distance = 8;
        int render_height = 2;
        int cached_chunk_x = 0x7fffffff;
        int cached_chunk_y = 0;
        int cached_chunk_z = 0;
        bool terrain_saturated = false;
        bool meshes_settled = false;
        unsigned int chunks_loading = 0;

        Shader *shader;

        // Thread-pooling
        vector<thread> threads_;
        condition_variable cv_;
        queue<function<void()>> task_queue_;
        mutex queue_mutex_;
        bool stop_ = false;
        
        mutex chunk_mutex;
        mutex generation_mutex;
        mutex render_mutex;
        mutex ready_chunks_mutex;
        queue<Chunk*> upload_queue;
        // Meshes being built on workers and not yet in upload_queue.
        atomic<int> meshes_in_flight{0};
};

#endif