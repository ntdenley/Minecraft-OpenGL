#include <world/world.h>
#include <iostream>
#include <functional>

using namespace std;

World *World::world = nullptr;

World::World(Shader *shader, size_t num_threads) : shader(shader), running(true) {
    cout << "Num threads: " << num_threads << endl;
    for(size_t i = 0; i < num_threads; i++){
        threads_.emplace_back([this] {
            while(true){
                function<void()> task;
                {
                    // Lokc the queue so data can be shared safely
                    unique_lock<mutex> lock(queue_mutex_);

                    // Wait until there is a task in the queue
                    cv_.wait(lock, [this] {
                        return !task_queue_.empty() || stop_; 
                    });

                    if(stop_ && task_queue_.empty())
                        return;
                    
                    // Get the task
                    task = move(task_queue_.front());
                    task_queue_.pop();
                }

                task();
            }
        });
    }
}

World::~World() {
    {
        // Lock the mutex to safely update stop
        unique_lock<mutex> lock(queue_mutex_);
        stop_ = true;
    }

    // Notify all threads to stop
    cv_.notify_all();

    // Join all threads
    for(auto &thread : threads_){
        thread.join();
    }
    running = false;
    if (chunk_thread.joinable()) {
        chunk_thread.join();
    }
}

Chunk* World::GetChunk(int chunk_x, int chunk_y, int chunk_z) {
    lock_guard<mutex> lock(chunk_mutex);
    auto it = chunks.find(std::make_tuple(chunk_x, chunk_y, chunk_z));
    if (it != chunks.end()) {
        return it->second;
    }
    return nullptr;
}

void World::queue_task(function<void()> task) {
    {
        // Lock the queue to safely add the task
        unique_lock<mutex> lock(queue_mutex_);
        task_queue_.emplace(move(task));
    }

    // Notify a thread that there is a task
    cv_.notify_one();
}

void World::GenerateChunk(glm::ivec3 chunk_pos){
    queue_task([this, chunk_pos] {
        auto chunk_key = make_tuple(chunk_pos.x, chunk_pos.y, chunk_pos.z);
        Chunk *chunk = new Chunk(chunk_pos, shader);
        chunk->Generate();
        lock_guard<mutex> lock(chunk_mutex);
        if (chunks.find(chunk_key) == chunks.end()) {
            chunks[chunk_key] = chunk;
        }
    });
}

void World::Update(glm::vec3 player_pos) {
    // Get the chunk that the player is in
    int chunk_x = (int)player_pos.x / CHUNK_SIZE;
    int chunk_y = (int)player_pos.y / CHUNK_SIZE;
    int chunk_z = (int)player_pos.z / CHUNK_SIZE;

    // Load the chunks around the player
    for (int x = -render_distance; x <= render_distance; x++)
    for (int y = -render_height; y <= render_height; y++)
    for (int z = -render_distance; z <= render_distance; z++) {
        // Get the chunk position
        int new_chunk_x = chunk_x + x;
        int new_chunk_y = chunk_y + y;
        int new_chunk_z = chunk_z + z;

        // Create the key
        tuple<int, int, int> chunk_tup = make_tuple(new_chunk_x, new_chunk_y, new_chunk_z);
        
        // Check if the chunk is already loaded
        bool chunk_loaded = false;
        {   
            lock_guard<mutex> lock(chunk_mutex);
            // Chunk is loaded, set flag
            if(chunks.find(chunk_tup) != chunks.end())
                chunk_loaded = true;

            // Chunk is not loaded, add it to the queue
            else if (chunks_to_render.find(chunk_tup) == chunks_to_render.end()){
                cout << "Queueing chunk: " << new_chunk_x << ", " << new_chunk_y << ", " << new_chunk_z << endl;
                GenerateChunk(glm::ivec3(new_chunk_x, new_chunk_y, new_chunk_z));
            }
        }

        // Render the chunk
        if(chunk_loaded){
            Chunk* chunk = nullptr;
            {
                lock_guard<mutex> lock(chunk_mutex);
                chunk = chunks[chunk_tup];
            }
            if (chunk) {
                chunk->Render();
            }
        }
    }
}
