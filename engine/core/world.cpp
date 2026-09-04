#include "engine/core/world.hpp"

#include <flecs.h>

namespace ksge {

namespace {

using Clock = std::chrono::steady_clock;

}

World::World()
    : world_()
{
    world_.component<FrameTime>();
    world_.set<FrameTime>({0.0, 0.0});
    last_ = Clock::now();
    frame_ = {0.0, 0.0};
}

World::~World() = default;

flecs::world& World::handle()
{
    return world_;
}

void World::step()
{
    const Clock::time_point now = Clock::now();
    const double elapsed = std::chrono::duration<double>(now - last_).count();
    last_ = now;

    frame_.deltaSeconds = elapsed;
    frame_.totalSeconds += elapsed;

    world_.set<FrameTime>(frame_);
    world_.progress(static_cast<float>(elapsed));
}

}