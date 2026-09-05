#include "engine/world/chunk_streamer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ksge {

namespace {

constexpr std::uint32_t kChunkMagic = 0x20434B53u;
constexpr std::uint32_t kChunkFileVersion = 1u;

std::int64_t makeChunkKey(std::int32_t gridX, std::int32_t gridZ)
{
    const std::int64_t shifted = static_cast<std::int64_t>(static_cast<std::uint32_t>(gridX));
    return (shifted << 32u) | static_cast<std::uint64_t>(static_cast<std::uint32_t>(gridZ));
}

std::uint32_t nextRandom(std::uint32_t& state)
{
    state = 1664525u * state + 1013904223u;
    return state;
}

}

bool serializeChunkFile(const char* path, const std::vector<ChunkSpawn>& entities)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
    {
        return false;
    }

    const std::uint32_t header[3] = {kChunkMagic, kChunkFileVersion, static_cast<std::uint32_t>(entities.size())};
    const bool headerOk = std::fwrite(header, sizeof(std::uint32_t), 3u, file) == 3u;
    bool bodyOk = headerOk;
    if (bodyOk)
    {
        for (const ChunkSpawn& entity : entities)
        {
            const std::uint32_t data[4] = {
                static_cast<std::uint32_t>(entity.gridX),
                static_cast<std::uint32_t>(entity.gridZ),
                entity.meshIndex,
                entity.materialVariant,
            };
            const float transform[10] = {
                entity.position.x, entity.position.y, entity.position.z,
                entity.rotation.x, entity.rotation.y, entity.rotation.z, entity.rotation.w,
                entity.scale.x, entity.scale.y, entity.scale.z,
            };
            if (std::fwrite(data, sizeof(std::uint32_t), 4u, file) != 4u ||
                std::fwrite(transform, sizeof(float), 10u, file) != 10u)
            {
                bodyOk = false;
                break;
            }
        }
    }
    std::fclose(file);
    return bodyOk;
}

bool deserializeChunkFile(const char* path, std::vector<ChunkSpawn>& entities)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || file == nullptr)
    {
        return false;
    }

    std::uint32_t header[3] = {};
    bool ok = std::fread(header, sizeof(std::uint32_t), 3u, file) == 3u;
    if (ok && header[0] == kChunkMagic && header[1] == kChunkFileVersion)
    {
        entities.clear();
        entities.reserve(header[2]);
        for (std::uint32_t index = 0u; index < header[2]; ++index)
        {
            ChunkSpawn entity;
            std::uint32_t data[4] = {};
            float transform[10] = {};
            if (std::fread(data, sizeof(std::uint32_t), 4u, file) != 4u ||
                std::fread(transform, sizeof(float), 10u, file) != 10u)
            {
                ok = false;
                break;
            }
            entity.gridX = static_cast<std::int32_t>(data[0]);
            entity.gridZ = static_cast<std::int32_t>(data[1]);
            entity.meshIndex = data[2];
            entity.materialVariant = data[3];
            entity.position = {transform[0], transform[1], transform[2]};
            entity.rotation = {transform[3], transform[4], transform[5], transform[6]};
            entity.scale = {transform[7], transform[8], transform[9]};
            entities.push_back(entity);
        }
    }
    std::fclose(file);
    return ok;
}

ChunkStreamer::ChunkStreamer(flecs::world& world, std::uint32_t cubeMesh, std::uint32_t sphereMesh)
    : world_(world)
    , cubeMesh_(cubeMesh)
    , sphereMesh_(sphereMesh)
{
    world_.component<ChunkComponent>();
    worker_ = std::thread([this]
    {
        workerLoop();
    });
}

ChunkStreamer::~ChunkStreamer()
{
    stopRequested_ = true;
    queueSignal_.notify_all();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

void ChunkStreamer::workerLoop()
{
    for (;;)
    {
        std::int64_t key = 0;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueSignal_.wait(lock, [this]
            {
                return stopRequested_.load() || !queuedKeys_.empty();
            });
            if (stopRequested_.load() && queuedKeys_.empty())
            {
                return;
            }
            if (queuedKeys_.empty())
            {
                continue;
            }
            key = queuedKeys_.back();
            queuedKeys_.pop_back();
        }

        const std::int32_t gridX = static_cast<std::int32_t>(static_cast<std::uint64_t>(key) >> 32u);
        const std::int32_t gridZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(key & 0xFFFFFFFFu));
        ChunkResult result = produceChunk(gridX, gridZ);

        std::lock_guard<std::mutex> lock(resultMutex_);
        results_.push_back(std::move(result));
    }
}

ChunkStreamer::ChunkResult ChunkStreamer::produceChunk(std::int32_t gridX, std::int32_t gridZ)
{
    ChunkResult result;
    result.spawns.reserve(6u);
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const std::string path = chunkPath(gridX, gridZ);
        if (deserializeChunkFile(path.c_str(), result.spawns))
        {
            result.fromFile = true;
            return result;
        }
    }

    std::uint32_t state = static_cast<std::uint32_t>(gridX * 73856093) ^
        static_cast<std::uint32_t>(gridZ * 19349663) ^ 0x7F3B5Cu;
    const float baseX = static_cast<float>(gridX) * kChunkSize;
    const float baseZ = static_cast<float>(gridZ) * kChunkSize;

    for (std::uint32_t index = 0u; index < 6u; ++index)
    {
        const std::uint32_t random = nextRandom(state);
        const float minX = 0.15f;
        const float maxX = 0.85f;
        const float t = static_cast<float>(random % 1000u) * 0.001f;
        const float u = static_cast<float>(nextRandom(state) % 1000u) * 0.001f;

        ChunkSpawn entity;
        entity.gridX = gridX;
        entity.gridZ = gridZ;
        entity.meshIndex = ((random >> 3u) & 1u) != 0u ? cubeMesh_ : sphereMesh_;
        entity.materialVariant = random % 3u;
        entity.position = {
            baseX + (minX + t * (maxX - minX)) * kChunkSize,
            0.4f + static_cast<float>(index % 3u) * 0.8f,
            baseZ + (minX + u * (maxX - minX)) * kChunkSize,
        };
        entity.scale = {0.8f, 0.8f, 0.8f};
        result.spawns.push_back(entity);
    }
    return result;
}

void ChunkStreamer::update(float cameraX, float cameraZ)
{
    const std::int32_t centerX = static_cast<std::int32_t>(std::floor(cameraX / kChunkSize));
    const std::int32_t centerZ = static_cast<std::int32_t>(std::floor(cameraZ / kChunkSize));
    const std::int32_t radiusChunks =
        static_cast<std::int32_t>(std::ceil(radiusMeters_ / kChunkSize));

    for (std::int32_t offsetX = -radiusChunks; offsetX <= radiusChunks; ++offsetX)
    {
        for (std::int32_t offsetZ = -radiusChunks; offsetZ <= radiusChunks; ++offsetZ)
        {
            const std::int32_t gridX = centerX + offsetX;
            const std::int32_t gridZ = centerZ + offsetZ;
            const std::int64_t key = makeChunkKey(gridX, gridZ);
            std::lock_guard<std::mutex> lock(queueMutex_);
            const bool alreadyQueued = std::find(queuedKeys_.begin(), queuedKeys_.end(), key) != queuedKeys_.end();
            if (chunks_.find(key) == chunks_.end() && !alreadyQueued)
            {
                queuedKeys_.push_back(key);
                queueSignal_.notify_one();
            }
        }
    }

    drainResults();
    unloadFarChunks(cameraX, cameraZ);
}

void ChunkStreamer::drainResults()
{
    std::vector<ChunkResult> ready;
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        ready.swap(results_);
    }
    for (ChunkResult& result : ready)
    {
        applyChunk(result);
    }
}

void ChunkStreamer::unloadFarChunks(float cameraX, float cameraZ)
{
    const float unloadDistance = radiusMeters_ + kChunkSize * 1.5f;
    std::vector<std::int64_t> toRemove;
    for (const auto& iterator : chunks_)
    {
        const std::int64_t key = iterator.first;
        const std::int32_t gridX = static_cast<std::int32_t>(static_cast<std::uint64_t>(key) >> 32u);
        const std::int32_t gridZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(key & 0xFFFFFFFFu));
        const float chunkCenterX = (static_cast<float>(gridX) + 0.5f) * kChunkSize;
        const float chunkCenterZ = (static_cast<float>(gridZ) + 0.5f) * kChunkSize;
        const float distance = std::sqrt((chunkCenterX - cameraX) * (chunkCenterX - cameraX) +
            (chunkCenterZ - cameraZ) * (chunkCenterZ - cameraZ));
        if (distance > unloadDistance)
        {
            toRemove.push_back(key);
        }
    }
    for (const std::int64_t key : toRemove)
    {
        removeChunk(key);
    }
}

void ChunkStreamer::applyChunk(const ChunkResult& result)
{
    if (result.spawns.empty())
    {
        return;
    }
    const std::int64_t key = makeChunkKey(result.spawns[0].gridX, result.spawns[0].gridZ);
    if (chunks_.find(key) != chunks_.end())
    {
        return;
    }
    ChunkSlot& slot = chunks_[key];
    slot.dirty = !result.fromFile;
    slot.loaded = true;
    for (const ChunkSpawn& spawn : result.spawns)
    {
        PbrMaterial material;
        if (spawn.materialVariant == 1u)
        {
            material.metallicFactor = 1.0f;
            material.roughnessFactor = 0.2f;
        }
        else if (spawn.materialVariant == 2u)
        {
            material.emissiveFactor = {0.6f, 0.2f, 0.1f};
            material.roughnessFactor = 0.5f;
        }
        else
        {
            material.metallicFactor = 0.1f;
            material.roughnessFactor = 0.7f;
        }
        const flecs::entity entity = world_.entity()
            .set<ChunkComponent>({spawn.gridX, spawn.gridZ, kChunkRevision})
            .set<Transform>({spawn.position, spawn.rotation, spawn.scale})
            .set<MeshRenderer>({spawn.meshIndex})
            .set<PbrMaterial>(material);
        slot.entities.push_back(entity);
    }
}

void ChunkStreamer::removeChunk(std::int64_t key)
{
    const auto iterator = chunks_.find(key);
    if (iterator == chunks_.end())
    {
        return;
    }
    for (const flecs::entity& entity : iterator->second.entities)
    {
        entity.destruct();
    }
    chunks_.erase(iterator);
}

void ChunkStreamer::saveAll()
{
    for (auto& iterator : chunks_)
    {
        ChunkSlot& slot = iterator.second;
        if (!slot.dirty)
        {
            continue;
        }
        const std::int64_t key = iterator.first;
        const std::int32_t gridX = static_cast<std::int32_t>(static_cast<std::uint64_t>(key) >> 32u);
        const std::int32_t gridZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(key & 0xFFFFFFFFu));

        std::vector<ChunkSpawn> spawns;
        spawns.reserve(slot.entities.size());
        for (const flecs::entity& entity : slot.entities)
        {
            if (!entity.is_alive())
            {
                continue;
            }
            const ChunkComponent& chunk = entity.get<ChunkComponent>();
            const Transform& transform = entity.get<Transform>();
            const MeshRenderer& mesh = entity.get<MeshRenderer>();
            const PbrMaterial& material = entity.get<PbrMaterial>();
            ChunkSpawn spawn;
            spawn.gridX = chunk.coordX;
            spawn.gridZ = chunk.coordZ;
            spawn.meshIndex = mesh.meshAsset;
            spawn.position = transform.position;
            spawn.rotation = transform.rotation;
            spawn.scale = transform.scale;
            if (material.metallicFactor > 0.5f)
            {
                spawn.materialVariant = 1u;
            }
            else if (material.emissiveFactor.x > 0.1f)
            {
                spawn.materialVariant = 2u;
            }
            else
            {
                spawn.materialVariant = 0u;
            }
            spawns.push_back(spawn);
        }

        const std::string path = chunkPath(gridX, gridZ);
        if (!spawns.empty() && serializeChunkFile(path.c_str(), spawns))
        {
            slot.dirty = false;
        }
    }
}

std::string ChunkStreamer::chunkPath(std::int32_t gridX, std::int32_t gridZ)
{
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "data/maps/chunk_%d_%d.bin", gridX, gridZ);
    return std::string(buffer);
}

void ChunkStreamer::setRadius(float meters)
{
    radiusMeters_ = std::max(meters, kChunkSize * 1.5f);
}

float ChunkStreamer::radius() const
{
    return radiusMeters_;
}

std::size_t ChunkStreamer::activeChunks() const
{
    return chunks_.size();
}

std::size_t ChunkStreamer::pendingJobs() const
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    return queuedKeys_.size();
}

}