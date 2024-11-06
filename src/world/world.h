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

class World {
    public:
        World(Shader *shader, size_t num_threads = std::thread::hardware_concurrency());
        ~World();
        void queue_task(std::function<void()> task);

        std::vector<Chunk::BlockType> GetChunkData(int chunk_x, int chunk_y, int chunk_z);
        void Update(glm::vec3 player_pos);

        Chunk* GetChunk(int chunk_x, int chunk_y, int chunk_z);
        void GenerateChunk(glm::ivec3 chunk_pos);

        // Global world pointer
        static World *world;
        unsigned int num_chunks = 0, num_chunks_rendered = 0;
    
    private:
        std::unordered_map<std::tuple<int, int, int>, Chunk*> chunks;
        std::queue<glm::vec3> chunk_queue;
        std::set<std::tuple<int, int, int>> chunks_to_render;
        int render_distance = 10;
        int render_height = 2;
        unsigned int chunks_loading = 0;

        Shader *shader;

        // Thread-pooling
        std::vector<std::thread> threads_;
        std::condition_variable cv_;
        std::queue<std::function<void()>> task_queue_;
        std::mutex queue_mutex_;
        bool stop_ = false;

        std::thread chunk_thread;
        std::atomic<bool> running;
        std::mutex chunk_mutex;
};

#endif