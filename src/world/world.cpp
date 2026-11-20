#include <world/world.h>
#include <iostream>
#include <functional>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>

using namespace std;

namespace {

constexpr int kMaxTerrainPerFrame = 48;
constexpr int kMaxMeshPerFrame = 8;
constexpr int kMaxPendingMeshes = 16;
constexpr int kMaxUploadsPerFrame = 8;
constexpr auto kUploadBudget = chrono::microseconds(2500);

// Chunk distance at which a brand-new chunk is built. 1 chunk is 32 blocks.
// Full detail holds until a single block is only a couple of pixels tall, so the
// step up to 2x2 and then 4x4 happens at the horizon instead of beside the player.
int LodForChunkDistance(int dist) {
    if (dist <= 22) return 0;
    if (dist <= 32) return 1;
    return 2;
}

// Keep the current size until the player is clearly past the boundary.
int SelectLod(int dist, int currentLod) {
    if (currentLod < 0)
        return LodForChunkDistance(dist);
    if (currentLod == 0)
        return dist >= 24 ? 1 : 0;
    if (currentLod == 1) {
        if (dist <= 20) return 0;
        if (dist >= 34) return 2;
        return 1;
    }
    return dist <= 30 ? 1 : 2;
}

// Visit chunks around the player, nearest shell first. fn returns false to stop.
template <typename Fn>
void ForEachChunkNear(int cx, int cy, int cz, int radius, int height, Fn&& fn) {
    for (int r = 0; r <= radius; ++r) {
        for (int dy = 0; dy <= height; ++dy) {
            int ySteps = (dy == 0) ? 1 : 2;
            for (int step = 0; step < ySteps; ++step) {
                int y = cy + (step == 0 ? dy : -dy);
                for (int x = -r; x <= r; ++x) {
                    for (int z = -r; z <= r; ++z) {
                        if (r > 0 && std::abs(x) != r && std::abs(z) != r)
                            continue;
                        if (!fn(cx + x, y, cz + z))
                            return;
                    }
                }
            }
        }
    }
}

}

World *World::world = nullptr;

World::World(Shader *shader, size_t num_threads) : shader(shader) {
    if (num_threads == 0) {
        unsigned int hw = thread::hardware_concurrency();
        // Leave two cores free so generation cannot schedule the render thread out.
        num_threads = hw > 2 ? static_cast<size_t>(hw - 2) : 1;
    }

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

void World::GenerateChunk(glm::ivec3 chunk_pos, bool storeVoxels){
    queue_task([this, chunk_pos, storeVoxels] {
        auto chunk_key = make_tuple(chunk_pos.x, chunk_pos.y, chunk_pos.z);
        Chunk *chunk = new Chunk(chunk_pos, shader);

        if (storeVoxels)
            chunk->GenerateTerrain();
        else
            chunk->terrain_generated = true;

        {
            lock_guard<mutex> lock(generation_mutex);
            if (!chunks_to_add.emplace(chunk_key, chunk).second)
                delete chunk;
        }
    });
}

void World::IntegrateGeneratedChunks() {
    unordered_map<tuple<int, int, int>, Chunk*> ready;
    {
        lock_guard<mutex> lock(generation_mutex);
        ready.swap(chunks_to_add);
    }

    if (!ready.empty())
        meshes_settled = false;

    for (auto& entry : ready) {
        Chunk* chunk = entry.second;
        if (!chunks.emplace(entry.first, chunk).second) {
            delete chunk;
            continue;
        }
        draw_list.push_back(chunk);

        int dist = std::max(std::abs(std::get<0>(entry.first) - cached_chunk_x),
                            std::abs(std::get<2>(entry.first) - cached_chunk_z));
        if (dist <= voxel_distance && !chunk->voxels_ready && !chunk->voxels_queued) {
            chunk->voxels_queued = true;
            queue_task([chunk] { chunk->GenerateTerrain(); });
        }
    }
}

void World::RequestTerrain(int originX, int originY, int originZ) {
    int requested = 0;
    bool finished = true;
    // One chunk past the render radius so a visible chunk's face neighbors exist.
    ForEachChunkNear(originX, originY, originZ, render_distance + 1, render_height + 1, [&](int x, int y, int z) {
        if (requested >= kMaxTerrainPerFrame) {
            finished = false;
            return false;
        }

        int dist = std::max(std::abs(x - originX), std::abs(z - originZ));
        bool wantVoxels = dist <= voxel_distance;
        auto key = make_tuple(x, y, z);
        auto it = chunks.find(key);
        if (it != chunks.end()) {
            Chunk* chunk = it->second;
            if (wantVoxels && !chunk->voxels_ready && !chunk->voxels_queued) {
                chunk->voxels_queued = true;
                queue_task([chunk] { chunk->GenerateTerrain(); });
                requested++;
            }
            return true;
        }
        if (chunks_to_render.find(key) != chunks_to_render.end())
            return true;

        chunks_to_render.insert(key);
        GenerateChunk(glm::ivec3(x, y, z), wantVoxels);
        requested++;
        return true;
    });
    if (finished)
        terrain_saturated = true;
}

void World::QueueMeshes(int originX, int originY, int originZ) {
    size_t pendingUploads = 0;
    {
        lock_guard<mutex> lock(ready_chunks_mutex);
        pendingUploads = upload_queue.size();
    }

    int room = kMaxPendingMeshes - static_cast<int>(pendingUploads) - meshes_in_flight.load();
    if (room <= 0)
        return;
    if (room > kMaxMeshPerFrame)
        room = kMaxMeshPerFrame;

    int queued = 0;
    bool finished = true;
    ForEachChunkNear(originX, originY, originZ, render_distance, render_height, [&](int x, int y, int z) {
        if (queued >= room) {
            finished = false;
            return false;
        }

        auto it = chunks.find(make_tuple(x, y, z));
        if (it == chunks.end())
            return true;

        Chunk* chunk = it->second;
        if (!chunk->terrain_generated || chunk->mesh_in_flight)
            return true;

        int dist = std::max(std::abs(x - originX), std::abs(z - originZ));
        int desiredLod = SelectLod(dist, chunk->lod);
        bool wantBottoms = std::abs((y - 1) - originY) > render_height;
        if (chunk->ready && chunk->drawCount == 0)
            return true;
        if (chunk->ready && chunk->lod == desiredLod && chunk->mesh_bottoms == wantBottoms)
            return true;

        auto stepAt = [&](int cx, int cz) {
            int neighborDist = std::max(std::abs(cx - originX), std::abs(cz - originZ));
            if (neighborDist > render_distance)
                return 0;
            auto neighbor = chunks.find(make_tuple(cx, y, cz));
            if (neighbor != chunks.end() && neighbor->second->lod >= 0)
                return 1 << neighbor->second->lod;
            return 1 << LodForChunkDistance(neighborDist);
        };
        int northStep = stepAt(x, z - 1);
        int southStep = stepAt(x, z + 1);
        int westStep = stepAt(x - 1, z);
        int eastStep = stepAt(x + 1, z);

        chunk->mesh_in_flight = true;
        meshes_in_flight.fetch_add(1);

        queue_task([this, chunk, northStep, southStep, westStep, eastStep, desiredLod, wantBottoms] {
            int neighborSteps[4] = { northStep, southStep, westStep, eastStep };
            chunk->GenerateMesh(desiredLod, neighborSteps, wantBottoms);
            {
                lock_guard<mutex> lock(ready_chunks_mutex);
                upload_queue.push(chunk);
            }
            meshes_in_flight.fetch_sub(1);
        });

        queued++;
        return true;
    });
    if (finished && queued == 0)
        meshes_settled = true;
}

void World::UploadMeshes() {
    auto start = chrono::steady_clock::now();
    int gpuUploads = 0;
    int processed = 0;

    while (processed < 512 && gpuUploads < kMaxUploadsPerFrame) {
        Chunk* chunk = nullptr;
        {
            lock_guard<mutex> lock(ready_chunks_mutex);
            if (upload_queue.empty())
                return;
            chunk = upload_queue.front();
            upload_queue.pop();
        }

        processed++;
        bool uploaded = chunk->Upload();
        chunk->lod = chunk->built_lod;
        chunk->mesh_bottoms = chunk->built_bottoms;
        chunk->mesh_in_flight = false;
        if (!uploaded)
            continue;

        gpuUploads++;
        if (gpuUploads >= kMaxUploadsPerFrame)
            return;
        if (chrono::steady_clock::now() - start >= kUploadBudget)
            return;
    }
}

struct FrustumPlane {
    glm::vec3 normal;
    float offset;
};

void ExtractFrustum(const glm::mat4& viewProj, FrustumPlane planes[6]) {
    glm::vec4 rows[4];
    for (int i = 0; i < 4; ++i)
        rows[i] = glm::vec4(viewProj[0][i], viewProj[1][i], viewProj[2][i], viewProj[3][i]);

    glm::vec4 raw[6] = {
        rows[3] + rows[0],
        rows[3] - rows[0],
        rows[3] + rows[1],
        rows[3] - rows[1],
        rows[3] + rows[2],
        rows[3] - rows[2]
    };
    for (int i = 0; i < 6; ++i) {
        float length = glm::length(glm::vec3(raw[i]));
        planes[i].normal = glm::vec3(raw[i]) / length;
        planes[i].offset = raw[i].w / length;
    }
}

bool OutsideFrustum(const FrustumPlane planes[6], glm::vec3 boxMin, glm::vec3 boxMax) {
    for (int i = 0; i < 6; ++i) {
        glm::vec3 positive = boxMin;
        if (planes[i].normal.x >= 0.0f) positive.x = boxMax.x;
        if (planes[i].normal.y >= 0.0f) positive.y = boxMax.y;
        if (planes[i].normal.z >= 0.0f) positive.z = boxMax.z;
        if (glm::dot(planes[i].normal, positive) + planes[i].offset < 0.0f)
            return true;
    }
    return false;
}

void World::Update(glm::vec3 player_pos, const glm::mat4& viewProj) {
    int chunk_x = static_cast<int>(std::floor(player_pos.x / CHUNK_SIZE));
    int chunk_y = static_cast<int>(std::floor(player_pos.y / CHUNK_SIZE));
    int chunk_z = static_cast<int>(std::floor(player_pos.z / CHUNK_SIZE));

    if (chunk_x != cached_chunk_x || chunk_y != cached_chunk_y || chunk_z != cached_chunk_z) {
        cached_chunk_x = chunk_x;
        cached_chunk_y = chunk_y;
        cached_chunk_z = chunk_z;
        terrain_saturated = false;
        meshes_settled = false;
    }

    // Workers build terrain and meshes. This thread only publishes results and
    // uploads a few GPU buffers, so a load burst cannot stall the frame.
    IntegrateGeneratedChunks();
    if (!terrain_saturated)
        RequestTerrain(chunk_x, chunk_y, chunk_z);
    if (!meshes_settled)
        QueueMeshes(chunk_x, chunk_y, chunk_z);
    UploadMeshes();

    FrustumPlane planes[6];
    ExtractFrustum(viewProj, planes);

    num_chunks_rendered = 0;
    const float chunkSize = static_cast<float>(CHUNK_SIZE);
    for (Chunk* chunk : draw_list) {
        if (!chunk->ready || chunk->drawCount == 0)
            continue;

        int cx = static_cast<int>(chunk->offset.x);
        int cy = static_cast<int>(chunk->offset.y);
        int cz = static_cast<int>(chunk->offset.z);
        if (std::abs(cx - chunk_x) > render_distance ||
            std::abs(cy - chunk_y) > render_height ||
            std::abs(cz - chunk_z) > render_distance)
            continue;

        glm::vec3 boxMin(cx * chunkSize, cy * chunkSize, cz * chunkSize);
        if (OutsideFrustum(planes, boxMin, boxMin + glm::vec3(chunkSize)))
            continue;

        chunk->Render();
        num_chunks_rendered++;
    }
}
