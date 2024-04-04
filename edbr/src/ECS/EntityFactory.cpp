#include <edbr/ECS/EntityFactory.h>

#include <iostream>

#include <edbr/ECS/Components/MetaInfoComponent.h>

const std::string EntityFactory::emptyString{};

void EntityFactory::setCreateDefaultEntityFunc(std::function<CreateDefaultEntityFuncType>&& f)
{
    createDefaultEntityFunc = f;
}

void EntityFactory::setPostInitEntityFunc(std::function<PostInitEntityFuncType>&& f)
{
    postInitEntityFunc = f;
}

void EntityFactory::registerPrefab(std::string prefabName, const std::filesystem::path& path)
{
    JsonFile file(path);
    assert(file.isGood());
    addPrefabFile(std::move(prefabName), std::move(file));
}

void EntityFactory::addPrefabFile(std::string prefabName, JsonFile file)
{
    assert(file.isGood());
    auto [it, inserted] = loadedPrefabFiles.emplace(std::move(prefabName), std::move(file));
    if (!inserted) {
        throw std::runtime_error(
            fmt::format("Prefab with name '{}' was already registered", prefabName));
    }
}

entt::handle EntityFactory::createDefaultEntity(
    entt::registry& registry,
    const std::string& sceneNodeName,
    bool postInit) const
{
    auto e = createDefaultEntityFunc ? createDefaultEntityFunc(registry) :
                                       entt::handle{registry, registry.create()};

    auto& mic = e.emplace<MetaInfoComponent>();
    mic.sceneNodeName = sceneNodeName;

    if (postInit && postInitEntityFunc) {
        postInitEntityFunc(e);
    }
    return e;
}

entt::handle EntityFactory::createEntity(
    entt::registry& registry,
    const std::string& prefabName,
    const std::string& sceneNodeName) const
{
    const auto& mappedPrefabName = getMappedPrefabName(prefabName);
    const auto& actualPrefabName = !mappedPrefabName.empty() ? mappedPrefabName : prefabName;
    const auto prefabLoader = getPrefabDataLoader(actualPrefabName);

    auto e = createDefaultEntity(registry, sceneNodeName, false);
    for (const auto& [componentName, loader] : prefabLoader.getKeyValueMap()) {
        if (!componentFactory.componentRegistered(componentName)) {
            std::cout << "prefabName=" << actualPrefabName << ": component '" << componentName
                      << "' was not registered. Skipping..." << std::endl;
            continue;
        }
        componentFactory.makeComponent(componentName, e, loader);
    }

    e.get<MetaInfoComponent>().prefabName = actualPrefabName;

    if (postInitEntityFunc) {
        postInitEntityFunc(e);
    }

    return e;
}

JsonDataLoader EntityFactory::getPrefabDataLoader(const std::string& prefabName) const
{
    auto it = loadedPrefabFiles.find(prefabName);
    if (it == loadedPrefabFiles.end()) {
        throw std::runtime_error(
            fmt::format("Prefab with name '{}' was not registered", prefabName));
    }
    return it->second.getLoader();
}

bool EntityFactory::prefabExists(const std::string& prefabName) const
{
    return loadedPrefabFiles.contains(prefabName);
}

void EntityFactory::addMappedPrefabName(const std::string& from, const std::string& to)
{
    if (from == to) {
        assert(false && "huh?");
        return;
    }

    if (prefabNameMapping.contains(from)) {
        throw std::runtime_error(
            fmt::format("Mapping for prefab '{}' was already added: {}", from, to));
    }
    if (prefabExists(to)) {
        prefabNameMapping.emplace(from, to);
        return;
    }

    auto mapped = getMappedPrefabName(to);
    if (!mapped.empty()) {
        prefabNameMapping.emplace(from, mapped);
        return;
    }
    throw std::runtime_error(fmt::format("Prefab with name '{}' was not registered", to));
}

const std::string& EntityFactory::getMappedPrefabName(const std::string& prefabName) const
{
    if (const auto it = prefabNameMapping.find(prefabName); it != prefabNameMapping.end()) {
        return it->second;
    }
    if (prefabExists(prefabName)) {
        return prefabName;
    }

    static const std::string emptyString{};
    return emptyString;
}
