#pragma once

#include <chrono>
#include <cstdint>

#include <flecs.h>

namespace ksge {

struct FrameTime
{
    double deltaSeconds;
    double totalSeconds;
};

class World
{
public:
    World();
    ~World();

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void step();

    flecs::world& handle();

private:
    flecs::world world_;
    std::chrono::steady_clock::time_point last_;
    FrameTime frame_;
};

}