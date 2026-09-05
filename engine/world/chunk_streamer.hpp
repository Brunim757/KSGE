#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <flecs.h>

#include "engine/scene/components.hpp"

namespace ksge {

constexpr float kChunkSize = 64.0f;
constexpr std::uint32_t kChunkRevision = 1u;

struct ChunkSpawn
{
    std::int32_t gridX = 0;
    std::int32_t gridZ = 0;
    std::uint32_t meshIndex = ~0u;
    std::uint32_t materialVariant = 0u;
    math::Vec3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

bool serializeChunkFile(const char* path, const std::vector<ChunkSpawn>& entities);
bool deserializeChunkFile(const char* path, std::vector<ChunkSpawn>& entities);

class ChunkStreamer
{
public:
    ChunkStreamer(flecs::world& world, std::uint32_t cubeMesh, std::uint32_t sphereMesh);
    ~ChunkStreamer();

    ChunkStreamer(const ChunkStreamer&) = delete;
    ChunkStreamer& operator=(const ChunkStreamer&) = delete;

    void update(float cameraX, float cameraZ);
    void saveAll();
    void setRadius(float meters);
    float radius() const;
    std::size_t activeChunks() const;
    std::size_t pendingJobs() const;

private:
    struct ChunkSlot
    {
        std::vector<flecs::entity> entities;
        bool dirty = false;
        bool loaded = false;
    };

    struct ChunkResult
    {
        std::vector<ChunkSpawn> spawns;
        bool fromFile = false;
    };

    void workerLoop();
    void drainResults();
    void unloadFarChunks(float cameraX, float cameraZ);
    void applyChunk(const ChunkResult& result);
    void removeChunk(std::int64_t key);
    static std::string chunkPath(std::int32_t gridX, std::int32_t gridZ);
    ChunkResult produceChunk(std::int32_t gridX, std::int32_t gridZ);

    flecs::world& world_;
    std::uint32_t cubeMesh_;
    std::uint32_t sphereMesh_;
    float radiusMeters_ = 120.0f;

    std::map<std::int64_t, ChunkSlot> chunks_;
    std::vector<std::int64_t> queuedKeys_;
    std::vector<ChunkResult> results_;
    mutable std::mutex queueMutex_;
    std::mutex resultMutex_;
    std::condition_variable queueSignal_;
    std::thread worker_;
    std::atomic<bool> stopRequested_{false};
};

}