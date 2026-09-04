#pragma once

#include <flecs.h>

#include "engine/platform/input.hpp"

namespace ksge {

class CameraService
{
public:
    explicit CameraService(flecs::world& world);

    CameraService(const CameraService&) = delete;
    CameraService& operator=(const CameraService&) = delete;

    void update(const InputSnapshot& input);

    flecs::entity editorCamera() const;

private:
    flecs::world world_;
    flecs::entity camera_;
};

}