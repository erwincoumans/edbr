#pragma once

#include <cstdint>
#include <limits>

using MeshId = std::size_t;
static const auto NULL_MESH_ID = std::numeric_limits<std::size_t>::max();

using JointId = std::uint16_t;
static const JointId NULL_JOINT_ID = std::numeric_limits<JointId>::max();
static const JointId ROOT_JOINT_ID = 0;

using MaterialId = std::size_t;
static const auto NULL_MATERIAL_ID = std::numeric_limits<std::size_t>::max();
