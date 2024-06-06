#include "Game.h"

#include <edbr/DevTools/ImGuiPropertyTable.h>

#include "Components.h"

#include <edbr/ECS/Components/MetaInfoComponent.h>
#include <edbr/ECS/Components/NameComponent.h>
#include <edbr/ECS/Components/PersistentComponent.h>
#include <edbr/ECS/Components/SceneComponent.h>
#include <edbr/ECS/Components/TransformComponent.h>

#include <edbr/GameCommon/CommonComponentDisplayers.h>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>

#include <edbr/Util/JoltUtil.h>

void Game::registerComponentDisplayers()
{
    using namespace devtools;

    auto& eid = entityInfoDisplayer;

    edbr::registerMetaInfoComponentDisplayer(eid);

    eid.registerDisplayer("Scene", [](entt::const_handle e, const SceneComponent& sc) {
        BeginPropertyTable();
        {
            DisplayProperty("Prefab scene name", sc.sceneName);
            DisplayProperty("Creation scene name", sc.creationSceneName);
            DisplayProperty("glTF node name", sc.sceneNodeName);
        }
        EndPropertyTable();
    });

    edbr::registerTagComponentDisplayer(eid);

    eid.registerDisplayer("Name", [](entt::const_handle e, const NameComponent& nc) {
        BeginPropertyTable();
        if (!nc.name.empty()) {
            DisplayProperty("Name", nc.name);
        }
        EndPropertyTable();
    });

    eid.registerDisplayer("Transform", [](entt::const_handle e, const TransformComponent& tc) {
        BeginPropertyTable();
        {
            DisplayProperty("Position", tc.transform.getPosition());
            DisplayProperty("Heading", tc.transform.getHeading());
            DisplayProperty("Scale", tc.transform.getScale());
        }
        EndPropertyTable();
    });

    edbr::registerMovementComponentDisplayer(eid);

    eid.registerDisplayer(
        "Physics",
        [this](entt::const_handle e, const PhysicsComponent& pc) {
            BeginPropertyTable();
            {
                DisplayProperty("bodyId", pc.bodyId.GetIndex());

                const auto typeString = [](PhysicsComponent::Type t) {
                    switch (t) {
                    case PhysicsComponent::Type::Static:
                        return "Static";
                    case PhysicsComponent::Type::Dynamic:
                        return "Dynamic";
                    case PhysicsComponent::Type::Kinematic:
                        return "Kinematic";
                    default:
                        return "???";
                    }
                }(pc.type);
                DisplayProperty("Type", typeString);

                const auto originTypeString = [](PhysicsComponent::OriginType t) {
                    switch (t) {
                    case PhysicsComponent::OriginType::BottomPlane:
                        return "BottomPlane";
                    case PhysicsComponent::OriginType::Center:
                        return "Center";
                    default:
                        return "???";
                    }
                }(pc.originType);
                DisplayProperty("Origin type", originTypeString);

                const auto bodyTypeString = [](PhysicsComponent::BodyType t) {
                    switch (t) {
                    case PhysicsComponent::BodyType::None:
                        return "None";
                    case PhysicsComponent::BodyType::Sphere:
                        return "Sphere";
                    case PhysicsComponent::BodyType::AABB:
                        return "AABB";
                    case PhysicsComponent::BodyType::Capsule:
                        return "Capsule";
                    case PhysicsComponent::BodyType::Cylinder:
                        return "Cylinder";
                    case PhysicsComponent::BodyType::TriangleMesh:
                        return "Triangle mesh";
                    default:
                        return "???";
                    }
                }(pc.bodyType);
                DisplayProperty("Body type", bodyTypeString);

                physicsSystem->doForBody(pc.bodyId, [](const JPH::Body& body) {
                    DisplayProperty("Position", util::joltToGLM(body.GetPosition()));
                    DisplayProperty(
                        "Center of mass", util::joltToGLM(body.GetCenterOfMassPosition()));
                    DisplayProperty("Rotation", util::joltToGLM(body.GetRotation()));
                    DisplayProperty("Linear velocity", util::joltToGLM(body.GetLinearVelocity()));
                    DisplayProperty("Angular velocity", util::joltToGLM(body.GetAngularVelocity()));
                    DisplayProperty("Object layer", body.GetObjectLayer());
                    DisplayProperty("Broadphase layer", body.GetBroadPhaseLayer().GetValue());
                    const auto motionTypeStr = [](JPH::EMotionType t) {
                        switch (t) {
                        case JPH::EMotionType::Dynamic:
                            return "Dynamic";
                        case JPH::EMotionType::Kinematic:
                            return "Kinematic";
                        case JPH::EMotionType::Static:
                            return "Static";
                        }
                        return "Unknown";
                    }(body.GetMotionType());
                    DisplayProperty("Motion type", motionTypeStr);
                    DisplayProperty("Active", body.IsActive());
                    DisplayProperty("Sensor", body.IsSensor());
                    if (!body.IsStatic()) {
                        const auto& motionProps = *body.GetMotionProperties();
                        DisplayProperty("Gravity factor", motionProps.GetGravityFactor());
                    }
                });
            }
            EndPropertyTable();
        },
        EntityInfoDisplayer::DisplayStyle::CollapsedByDefault);

    eid.registerDisplayer(
        "Mesh",
        [](entt::const_handle e, const MeshComponent& mc) {
            BeginPropertyTable();
            {
                DisplayProperty("Cast shadow", mc.castShadow);
                for (const auto& id : mc.meshes) {
                    DisplayProperty("mesh", id);
                }
            }
            EndPropertyTable();
        },
        EntityInfoDisplayer::DisplayStyle::CollapsedByDefault);

    eid.registerDisplayer("Skeleton", [](entt::const_handle e, const SkeletonComponent& sc) {
        const auto& animator = sc.skeletonAnimator;
        BeginPropertyTable();
        {
            DisplayProperty("Animation", animator.getCurrentAnimationName());
            DisplayProperty("Anim length", animator.getAnimation()->duration);
            DisplayProperty("Progress", animator.getProgress());
            DisplayProperty("Frame", animator.getCurrentFrame());
            DisplayProperty("Looped", animator.getAnimation()->looped);
        }
        EndPropertyTable();
    });

    eid.registerDisplayer("Light", [](entt::const_handle e, const LightComponent& lc) {
        const auto& light = lc.light;

        const auto lightTypeString = [](LightType lt) {
            switch (lt) {
            case LightType::Directional:
                return "Directional";
            case LightType::Point:
                return "Point";
            case LightType::Spot:
                return "Spot";
            default:
                return "";
            }
        }(light.type);

        BeginPropertyTable();
        {
            DisplayProperty("Type", lightTypeString);

            DisplayProperty("Color", light.color);
            DisplayProperty("Range", light.range);
            DisplayProperty("Intensity", light.intensity);
            if (light.type == LightType::Spot) {
                DisplayProperty("Scale offset", light.scaleOffset);
            }
            DisplayProperty("Cast shadow", light.castShadow);
        }
        EndPropertyTable();
    });

    eid.registerEmptyDisplayer<TriggerComponent>("Trigger");
    eid.registerEmptyDisplayer<PlayerSpawnComponent>("PlayerSpawn");
    eid.registerEmptyDisplayer<PlayerComponent>("Player");
    eid.registerEmptyDisplayer<PersistentComponent>("Persistent");
    eid.registerEmptyDisplayer<ColliderComponent>("Collider");

    edbr::registerNPCComponentDisplayer(eid);

    eid.registerEmptyDisplayer<CameraComponent>("Camera", [this](entt::const_handle e) {
        if (ImGui::Button("Make current")) {
            setCurrentCamera(e);
        }
    });

    eid.registerDisplayer("Interact", [](entt::const_handle e, const InteractComponent& ic) {
        BeginPropertyTable();
        {
            const auto typeString = [](InteractComponent::Type t) {
                switch (t) {
                case InteractComponent::Type::Interact:
                    return "Interact";
                case InteractComponent::Type::Talk:
                    return "Talk";
                default:
                    return "???";
                }
            }(ic.type);
            DisplayProperty("Type", typeString);
        }
        EndPropertyTable();
    });

    eid.registerDisplayer(
        "AnimationEventSound",
        [](entt::const_handle e, const AnimationEventSoundComponent& sc) {
            BeginPropertyTable();
            {
                for (const auto& [event, sound] : sc.eventSounds) {
                    DisplayProperty(event.c_str(), sound);
                }
            }
            EndPropertyTable();
        },
        EntityInfoDisplayer::DisplayStyle::CollapsedByDefault);
}
