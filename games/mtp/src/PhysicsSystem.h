#pragma once

#include <iostream>
#include <unordered_map>
#include <vector>

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <edbr/DevTools/JoltDebugRenderer.h>
#include <edbr/Graphics/IdTypes.h>
#include <edbr/Math/Transform.h>

#include <entt/entity/handle.hpp>
#include <entt/entity/registry.hpp>

#include "Events.h"
#include "VirtualCharacterParams.h"

namespace Layers
{
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
};

struct CPUMesh;
class InputManager;
class EventManager;
class SceneCache;

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
    {
        switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING; // Non moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer NON_MOVING{0};
static constexpr JPH::BroadPhaseLayer MOVING{1};
static constexpr std::uint32_t NUM_LAYERS{2};
};

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl()
    {
        // Create a mapping table from object to broad phase layer
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    std::uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
            return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
            return "MOVING";
        default:
            JPH_ASSERT(false);
            return "INVALID";
        }
    }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

/// Class that determines if an object layer can collide with a broadphase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// An example contact listener
class MyContactListener : public JPH::ContactListener {
public:
    // See: ContactListener
    JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult) override
    {
        // std::cout << "Contact validate callback" << std::endl;

        // Allows you to ignore a contact before it is created (using layers to not make objects
        // collide is cheaper!)
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        // std::cout << "A contact was added" << std::endl;
    }

    void OnContactPersisted(
        const JPH::Body& inBody1,
        const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        // std::cout << "A contact was persisted" << std::endl;
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
    {
        /* std::cout << "A contact was removed" << inSubShapePair.GetBody1ID().GetIndex() << " "
                  << inSubShapePair.GetBody2ID().GetIndex() << std::endl; */
    }
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener {
public:
    void OnBodyActivated(const JPH::BodyID& inBodyID, std::uint64_t inBodyUserData) override
    {
        // std::cout << "A body got activated" << std::endl;
    }

    void OnBodyDeactivated(const JPH::BodyID& inBodyID, std::uint64_t inBodyUserData) override
    {
        // std::cout << "A body went to sleep" << std::endl;
    }
};

class PhysicsSystem {
public:
    PhysicsSystem(EventManager& eventManager);
    // Need to call this function before init to initialize various Jolt stuff
    static void InitStaticObjects();

    void init();
    void drawDebugShapes(const Camera& camera);
    void handleCharacterInput(
        float dt,
        glm::vec3 movementDirection,
        bool jumping,
        bool jumpHeld,
        bool running);
    void update(float dt, const glm::quat& characterRotation);
    void cleanup();

    void stopCharacterMovement();
    void setCharacterPosition(const glm::vec3 pos);
    glm::vec3 getCharacterPosition() const;
    glm::vec3 getCharacterVelocity() const;
    bool isCharacterOnGround() const;

    // creates entity physics body
    void addEntity(entt::handle e, SceneCache& sceneCache);

    JPH::Ref<JPH::Shape> cacheMeshShape(
        const std::vector<const CPUMesh*>& meshes,
        const std::vector<MeshId>& meshIds,
        const std::vector<Transform>& meshTransforms);

    JPH::BodyID createBody(
        entt::handle e,
        const Transform& transform,
        JPH::Ref<JPH::Shape> shape,
        bool staticBody,
        bool sensor);
    void updateTransform(JPH::BodyID id, const Transform& transform, bool updateScale = false);
    void setVelocity(JPH::BodyID id, const glm::vec3& velocity);
    void syncCharacterTransform();
    void syncVisibleTransform(JPH::BodyID id, Transform& transform);

    void doForBody(JPH::BodyID id, std::function<void(const JPH::Body&)> f);

    JoltDebugRenderer debugRenderer;

    void updateDevUI(const InputManager& im, float dt);

    entt::handle getEntityByBodyID(const JPH::BodyID& bodyID) const;

    const std::vector<entt::handle>& getInteractableEntities() const
    {
        return interactableEntities;
    }

    void onEntityTeleported(const EntityTeleportedEvent& event);
    // Remove physics body on destory
    void onEntityDestroyed(entt::handle e);

    // draw settings
    bool drawCollisionLinesWithDepth{true};
    bool drawCollisionShapes{false};
    bool drawCollisionShapesWireframe{true};
    bool drawCollisionShapeBoundingBox{false};
    bool drawSensorsOnly{true};
    bool drawCharacterShape{false};

private:
    void collectInteractableEntities(const glm::quat& characterRotation);
    void drawBodies(const Camera& camera);
    void sendCollisionEvents();

    JPH::PhysicsSystem physicsSystem;
    EventManager& eventManager;

    JPH::Body* floor{nullptr};

    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    JPH::JobSystemThreadPool job_system;

    BPLayerInterfaceImpl bpLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBPLayerFilter;
    ObjectLayerPairFilterImpl objectVsObjectPairFilter;

    MyContactListener contactListener;
    MyBodyActivationListener bodyActivationListener;

    struct CachedMeshShape {
        std::vector<MeshId> meshIds;
        std::vector<Transform> meshTransforms;
        JPH::Ref<JPH::Shape> meshShape;
    };
    std::vector<CachedMeshShape> cachedMeshShapes;

    std::unordered_map<std::uint32_t, entt::handle> bodyIDToEntity;
    std::vector<entt::handle> interactableEntities;

    // character stuff
    bool characterOnGround{true};
    void createCharacter(entt::handle e, const VirtualCharacterParams& cp);
    void characterPreUpdate(float dt, const glm::quat& characterRotation);
    // character data
    entt::handle characterEntity;
    JPH::Ref<JPH::CharacterVirtual> character;
    JPH::RefConst<JPH::Shape> characterShape;
    JPH::Vec3 characterDesiredVelocity;
    VirtualCharacterParams characterParams;
    // character interaction
    JPH::RefConst<JPH::Shape> characterInteractionShape;
    float interactionSphereRadius{0.5f};
    glm::vec3 interactionSphereOffset{0.f, 1.f, 0.5f};
    bool handledPlayerInputThisFrame{false};
};
