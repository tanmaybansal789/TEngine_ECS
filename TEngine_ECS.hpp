//
// Created by Tanmay Bansal on 10/07/2024.
//

#ifndef TENGINE_ECS_HPP
#define TENGINE_ECS_HPP

// Standard Library Includes
#include <iostream>     // For input and output operations
#include <fstream>      // For file stream operations
#include <sstream>      // For string stream operations
#include <cassert>      // For the assert macro
#include <limits>       // For numeric limits

// Containers
#include <vector>       // For std::vector
#include <array>        // For std::array
#include <map>          // For std::map
#include <unordered_map>// For std::unordered_map
#include <unordered_set>// For std::unordered_set

// Bit Manipulation
#include <bitset>       // For std::bitset

// Memory Management
#include <memory>       // For std::shared_ptr, std::unique_ptr, std::weak_ptr, std::make_shared, std::make_unique etc.

// Type Information
#include <typeinfo>     // For typeid operator and std::type_info

// Multithreading
#include <thread>                 // For std::thread
#include <future>                 // For std::future
#include <mutex>                  // For std::mutex
#include <condition_variable>     // For std::condition_variable
#include <queue>                  // For std::queue
#include <functional>             // For std::function

// Type definitions
using EntityId = unsigned int;
using ComponentTypeId = unsigned int;


using EventId = unsigned int;
using EventHandler = std::function<void()>;
using EventCondition = std::function<bool()>;

// Constants
constexpr EntityId MAX_ENTITIES = 1000;
constexpr ComponentTypeId MAX_COMPONENTS = 32;

// Signature type (uses MAX_COMPONENTS
using Signature = std::bitset<MAX_COMPONENTS>;

// Null values
constexpr auto tnull = MAX_ENTITIES;
constexpr auto tnullptr = nullptr;

// Tags
template<const unsigned int value>
using Tag = std::integral_constant<const unsigned int, value>;

consteval unsigned int operator"" _hs(const char* str, const std::size_t length) {
    const unsigned int fnv_prime = 16777619u; // FNV prime
    unsigned int hash = 2166136261u; // FNV offset basis
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= static_cast<unsigned char>(str[i]);
        hash *= fnv_prime;
    }
    return hash;
}

template<const unsigned int value>
std::ostream& operator<<(std::ostream& os, Tag<value>) {
    os << "Tag<" << value << ">";
    return os;
}

template<const unsigned int value>
std::istream& operator>>(std::istream& is, Tag<value>) {
    return is;
}

namespace ECS {
    // Forward declarations
    class Context;

    class IComponentStorage {
    public:
        virtual ~IComponentStorage() = default;
        virtual void entityDestroyed(EntityId entityId) = 0;
        virtual void dump(std::ostream& os) const = 0;
        virtual void deserialise(std::istringstream& iss, EntityId entityId) = 0;
    };

    template <typename T>
    class ComponentStorage : public IComponentStorage {
    public:
        explicit ComponentStorage() : m_Components(), entityToIndexMap(), indexToEntityMap(){
            entityToIndexMap.fill(tnull);
            indexToEntityMap.fill(tnull);
        }
        void entityDestroyed(const EntityId entityId) override;
        void add(const EntityId entityId, T& component);
        void remove(const EntityId entityId);
        T& get(const EntityId entityId);
        [[nodiscard]] bool has(const EntityId entityId) const;
        void dump(std::ostream& os) const override;
        void deserialise(std::istringstream& iss, const EntityId entityId) override;

    private:
        std::vector<T> m_Components;
        std::array<unsigned int, MAX_ENTITIES> entityToIndexMap;
        std::array<EntityId, MAX_ENTITIES> indexToEntityMap;
    };

    class System {
    public:
        System(Context& context, const Signature signature) : m_Context(context), m_Signature(signature) {}
        [[nodiscard]] Signature getSignature() const { return m_Signature; }
        [[nodiscard]] std::unordered_set<EntityId>& getEntities() { return m_Entities; }
        virtual void update() = 0;
        virtual ~System() = default;
    protected:
        Context& m_Context;
        const Signature m_Signature;
        std::unordered_set<EntityId> m_Entities;
    };

    class SystemPipeline {
    public:
        SystemPipeline() = default;
        void addSystem(const std::shared_ptr<System>& system) { m_Systems.push_back(system); }
        void update() const;
    private:
        std::vector<std::shared_ptr<System>> m_Systems;
    };

    class Context {
    public:
        Context() : m_EntityIndices() { m_EntityIndices.fill(tnull); }

        // Entity methods
        EntityId createEntity();
        void addEntity(const EntityId entityId);
        void destroyEntity(const EntityId entityId);

        // Component methods
        template<typename T>
        void registerComponentType();
        template<typename T>
        void addComponent(const EntityId entityId, T component);
        template<typename T>
        void removeComponent(const EntityId entityId);
        template<typename T>
        T& getComponent(const EntityId entityId);
        template<typename T>
        bool hasComponent(const EntityId entityId);
        template<typename T>
        ComponentTypeId getComponentTypeId();
        template<typename T>
        std::shared_ptr<ComponentStorage<T>> getComponentStorage();

        // System methods
        void addSystem(const std::shared_ptr<System>& system, unsigned int pipelineIndex = 0);
        void update() const;

        // Event handling methods
        void addEvent(const EventId eventId, const EventCondition& eventCondition);
        void addEventHandler(const EventId eventId, const EventHandler& eventHandler);
        void updateEvents();

        // Serialisation methods
        friend std::ostream& operator<<(std::ostream& os, const Context& context);
        friend std::istream& operator>>(std::istream& is, Context& context);

        // Default destructor
        ~Context() = default;

    private:
        std::vector<EntityId> m_EntityList;
        std::vector<EntityId> m_FreedEntityList;
        std::array<unsigned int, MAX_ENTITIES> m_EntityIndices;
        EntityId nextEntityId = 0;

        std::array<Signature, MAX_ENTITIES> m_EntitySignatures;

        std::array<std::shared_ptr<IComponentStorage>, MAX_COMPONENTS> m_ComponentStorages;
        ComponentTypeId nextComponentTypeId = 0;
        std::map<const char*, ComponentTypeId > m_ComponentTypes;

        std::vector<std::shared_ptr<System>> m_Systems;
        std::vector<std::shared_ptr<SystemPipeline>> m_SystemPipelines;

        std::unordered_map<EventId, EventCondition> m_EventConditions;
        std::unordered_multimap<EventId, EventHandler> m_EventHandlers;
    };
}

namespace ECS {
    // Implement ComponentStorage

    // IComponentStorage Overrides
    template<typename T>
    void ComponentStorage<T>::entityDestroyed(const EntityId entityId) {
        if (has(entityId))
            return remove(entityId);
    }

    template<typename T>
    void ComponentStorage<T>::dump(std::ostream& os) const {
        for (const auto& index : entityToIndexMap) {
            if (index == tnull)
                break;
            os << "Entity: " << indexToEntityMap[index] << ", " << m_Components[index] << std::endl;
        }
    }

    template<typename T>
    void ComponentStorage<T>::deserialise(std::istringstream& iss, const EntityId entityId) {
        T component;
        iss >> component;
        add(entityId, component);
    }

    // ComponentStorage Methods
    template<typename T>
    void ComponentStorage<T>::add(const EntityId entityId, T& component) {
        const auto& index = m_Components.size();
        m_Components.push_back(component);
        entityToIndexMap[entityId] = index;
        indexToEntityMap[index] = entityId;
    }

    template<typename T>
    void ComponentStorage<T>::remove(const EntityId entityId) {
        const auto& index = entityToIndexMap[entityId];
        const auto& lastIndex = m_Components.size() - 1;
        m_Components[index] = m_Components[lastIndex];
        entityToIndexMap[indexToEntityMap[lastIndex]] = index;
        indexToEntityMap[index] = indexToEntityMap[lastIndex];
        m_Components.pop_back();
        entityToIndexMap[entityId] = tnull;
        indexToEntityMap[lastIndex] = tnull;
    }

    template<typename T>
    T& ComponentStorage<T>::get(const EntityId entityId) {
        return m_Components[entityToIndexMap[entityId]];
    }

    template<typename T>
    bool ComponentStorage<T>::has(const EntityId entityId) const {
        return entityToIndexMap[entityId] != tnull;
    }

    inline void SystemPipeline::update() const {
        std::vector<std::future<void>> futures;

        for (const auto& system : m_Systems)
            futures.push_back(std::async(std::launch::async, &System::update, system));

        for (auto& future : futures)
            future.get();
    }

    // Implement Context
    inline EntityId Context::createEntity() {
        EntityId entityId;
        if (m_FreedEntityList.empty())
            entityId = nextEntityId++;
        else {
            entityId = m_FreedEntityList.back();
            m_FreedEntityList.pop_back();
        }
        addEntity(entityId);
        return entityId;
    }

    inline void Context::addEntity(const EntityId entityId) {
        m_EntityList.push_back(entityId);
        m_EntityIndices[entityId] = m_EntityList.size() - 1;
    }

    inline void Context::destroyEntity(const EntityId entityId) {
        if (m_EntityIndices[entityId] == tnull)
            return;

        m_FreedEntityList.push_back(entityId);
        m_EntitySignatures[entityId].reset();

        m_EntityList[m_EntityIndices[entityId]] = m_EntityList.back(); // Move the last entity to the destroyed entity's place
        m_EntityIndices[m_EntityList.back()] = m_EntityIndices[entityId]; // Update the index of the moved entity

        m_EntityList.pop_back(); // Remove the last entity
        m_EntityIndices[entityId] = tnull; // Invalidate the destroyed entity's index

        for (const auto& storage : m_ComponentStorages)
            if (storage)
                storage->entityDestroyed(entityId);

        for (const auto& system : m_Systems) {
            if (auto& entities = system->getEntities(); entities.contains(entityId))
                entities.erase(entityId);
        }
    }

    template<typename T>
    void Context::registerComponentType() {
        m_ComponentTypes[typeid(T).name()] = nextComponentTypeId;
        m_ComponentStorages[nextComponentTypeId] = std::make_shared<ComponentStorage<T>>();
        ++nextComponentTypeId;
    }

    template<typename T>
    void Context::addComponent(const EntityId entityId, T component) {
        getComponentStorage<T>()->add(entityId, component);
        const auto& typeId = getComponentTypeId<T>();
        m_EntitySignatures[entityId].set(typeId);

        const auto& entitySignature = m_EntitySignatures[entityId];
        for (const auto& system : m_Systems) {
            if (const Signature& systemSignature = system->getSignature(); (entitySignature & systemSignature) == systemSignature)
                system->getEntities().insert(entityId);
        }
    }

    template<typename T>
    void Context::removeComponent(const EntityId entityId) {
        const auto& typeId = getComponentTypeId<T>();
        m_EntitySignatures[entityId].reset(typeId);
        getComponentStorage<T>()->remove(entityId);
        const auto& entitySignature = m_EntitySignatures[entityId];
        for (const auto& system : m_Systems) {
            const Signature& systemSignature = system->getSignature();
            auto& systemEntities = system->getEntities();
            if ((entitySignature & systemSignature) != systemSignature && systemEntities.contains(entityId))
                systemEntities.erase(entityId);
        }
    }

    template<typename T>
    T& Context::getComponent(const EntityId entityId) {
        return getComponentStorage<T>()->get(entityId);
    }

    template<typename T>
    bool Context::hasComponent(const EntityId entityId) {
        const auto typeId = getComponentTypeId<T>();
        return m_EntitySignatures[entityId][typeId];
    }

    template<typename T>
    ComponentTypeId Context::getComponentTypeId() {
        return m_ComponentTypes[typeid(T).name()];
    }

    template<typename T>
    std::shared_ptr<ComponentStorage<T>> Context::getComponentStorage() {
        const auto typeId = getComponentTypeId<T>();
        return std::static_pointer_cast<ComponentStorage<T>>(m_ComponentStorages[typeId]);
    }

    inline void Context::addSystem(const std::shared_ptr<System>& system, unsigned int pipelineIndex) {
        m_Systems.emplace_back(system);

        if (m_SystemPipelines.size() <= pipelineIndex)
            m_SystemPipelines.resize(pipelineIndex + 1);

        if (!m_SystemPipelines[pipelineIndex])
            m_SystemPipelines[pipelineIndex] = std::make_shared<SystemPipeline>();

        m_SystemPipelines[pipelineIndex]->addSystem(system);

        const auto& systemSignature = system->getSignature();
        for (const auto& entityId : m_EntityList) {
            const auto& entitySignature = m_EntitySignatures[entityId];
            if ((entitySignature & systemSignature) == systemSignature)
                system->getEntities().insert(entityId);
        }
    }

    inline void Context::addEvent(const EventId eventId, const EventCondition& eventCondition) {
        m_EventConditions[eventId] = eventCondition;
    }

    inline void Context::addEventHandler(const EventId eventId, const EventHandler& eventHandler) {
        m_EventHandlers.emplace(eventId, eventHandler);
    }

    inline void Context::updateEvents() {
        for (const auto& [eventId, eventCondition] : m_EventConditions) {
            if (eventCondition()) {
                const auto& range = m_EventHandlers.equal_range(eventId);
                for (auto it = range.first; it != range.second; ++it)
                    it->second();
            }
        }
    }

    inline void Context::update() const {
        for (const auto& pipeline : m_SystemPipelines)
            pipeline->update();
    }

    inline std::ostream& operator<<(std::ostream& os, const Context& context) {
        os << "# ECS Serialisation\n";
        os << "# Version: 1.0 \n\n";
        os << "# Entities\n";
        os << "EntityCount: " << context.m_EntityList.size() << std::endl;
        os << "NextEntityId: " << context.nextEntityId << std::endl;
        os << "FreedEntityList: ";
        for (const auto& entityId : context.m_FreedEntityList)
            os << entityId << " ";
        os << std::endl;

        for (const auto& entityId : context.m_EntityList)
            os << "Entity: " << entityId << ", Signature: " << context.m_EntitySignatures[entityId] << std::endl;

        os << "\n# Components\n";

        os << "NextComponentTypeId: " << context.nextComponentTypeId << std::endl;
        for (const auto& componentTypePair : context.m_ComponentTypes) {
            const auto& typeId = componentTypePair.second;
            const auto& componentStorage = context.m_ComponentStorages[typeId];
            os << "ComponentType: " << componentTypePair.first << std::endl;
            componentStorage->dump(os);
        }
        return os;
    }

    inline std::istream& operator>>(std::istream& is, Context& context) {
        std::string line;
        unsigned int entityCount;
        ComponentTypeId currentComponentTypeId = 0;
        bool inComponentSection = false;
        while (std::getline(is, line)) {
            if (line.empty() || line[0] == '#') continue; // Skip comments and empty lines

            std::istringstream iss(line);
            std::string key;
            iss >> key;

            if (key == "EntityCount:") {
                iss >> entityCount;
            } else if (key == "NextEntityId:") {
                iss >> context.nextEntityId;
            } else if (key == "FreedEntityList:") {
                EntityId entityId;
                while (iss >> entityId) {
                    context.m_FreedEntityList.push_back(entityId);
                }
            } else if (key == "Entity:") {
                if (!inComponentSection) {
                    EntityId entityId;
                    std::string signature;
                    iss >> entityId;
                    iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    iss >> signature;
                    context.m_EntityList.push_back(entityId);
                    context.m_EntityIndices[entityId] = context.m_EntityList.size() - 1;
                    context.m_EntitySignatures[entityId] = std::bitset<MAX_COMPONENTS>(signature);
                }
                else {
                    EntityId entityId;
                    iss >> entityId;
                    iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
                    context.m_ComponentStorages[currentComponentTypeId]->deserialise(iss, entityId);
                }
            } else if (key == "ComponentType:") {
                if (!inComponentSection) inComponentSection = true;
                else ++currentComponentTypeId;
            }
        }
        return is;
    }
}

namespace HELPER {
    // System signature creation
    template<typename... Components>
    Signature createSignature(ECS::Context& context) {
        Signature signature;
        (..., signature.set(context.getComponentTypeId<Components>()));
        return signature;
    }

    // Entity creation with components
    template<typename... Components>
    EntityId createEntityWithComponents(ECS::Context& context, Components&&... components) {
        auto entityId = context.createEntity();
        (context.addComponent(entityId, std::forward<Components>(components)), ...);
        return entityId;
    }

    // Serialisation and Deserialisation
    inline void writeContextToFile(const ECS::Context& context, const std::string& filename) {
        std::ofstream outFile(filename);
        if (!outFile) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return;
        }
        outFile << context;
        outFile.close();
    }

    inline void readContextFromFile(ECS::Context& context, const std::string& filename) {
        std::ifstream inFile(filename);
        if (!inFile) {
            std::cerr << "Failed to open file for reading: " << filename << std::endl;
            return;
        }
        inFile >> context;
        inFile.close();
    }
}

namespace DEMO {

    struct PositionComponent {
        float x, y;
        friend std::ostream& operator<<(std::ostream& os, const PositionComponent& position) {
            os << "Position: " << position.x << " " << position.y;
            return os;
        }
        friend std::istream& operator>>(std::istream& is, PositionComponent& position) {
            // Extracts from "Position: <x> <y>"
            is.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            is >> position.x >> position.y;
            return is;
        }
    };

    struct VelocityComponent {
        float dx, dy;
        friend std::ostream& operator<<(std::ostream& os, const VelocityComponent& velocity) {
            os << "Velocity: " << velocity.dx << " " << velocity.dy;
            return os;
        }
        friend std::istream& operator>>(std::istream& is, VelocityComponent& velocity) {
            // Extracts from "Velocity: <dx> <dy>"
            is.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            is >> velocity.dx >> velocity.dy;
            return is;
        }
    };

    struct HealthComponent {
        int health;
        friend std::ostream& operator<<(std::ostream& os, const HealthComponent& health) {
            os << "Health: " << health.health;
            return os;
        }
        friend std::istream& operator>>(std::istream& is, HealthComponent& health) {
            // Extracts from "Health: <health>"
            is.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            is >> health.health;
            return is;
        }
    };


    class MovementSystem : public ECS::System {
    public:
        explicit MovementSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, VelocityComponent>(context)) {}

        void update() override {
            for (const auto& entityId : m_Entities) {
                auto& position = m_Context.getComponent<PositionComponent>(entityId);
                auto& velocity = m_Context.getComponent<VelocityComponent>(entityId);
                position.x += velocity.dx;
                position.y += velocity.dy;
            }
        }
    };

    class RenderSystem : public ECS::System {
    public:
        explicit RenderSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, HealthComponent>(context)) {}

        void update() override {
            for (const auto& entityId : m_Entities) {
                const auto& position = m_Context.getComponent<PositionComponent>(entityId);
                const auto& health = m_Context.getComponent<HealthComponent>(entityId);
                std::cout << "Entity " << entityId << " at " << position << " with " << health << std::endl;
            }
        }
    };

    class TestSystem : public ECS::System {
    public:
        explicit TestSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, VelocityComponent>(context)) {}

        void update() override {
            std::cout << "This test system on pipeline 1 will run after Movement System and Render System" << std::endl;
            for (const auto& entityId : m_Entities) {
                const auto& position = m_Context.getComponent<PositionComponent>(entityId);
                const auto& velocity = m_Context.getComponent<VelocityComponent>(entityId);
                std::cout << "Entity " << entityId << " at " << position << " with " << velocity << std::endl;
            }
        }

        static void testFunction() {
            std::cout << "Test function called" << std::endl;
        }

        bool TestCondition() {
            return true;
        }
    };

    class TagTestSystem : public ECS::System {
    public:
        explicit TagTestSystem(ECS::Context& context) : System(context, HELPER::createSignature<Tag<"TagTest"_hs>>(context)) {}

        void update() override {
            for (const auto& entityId : m_Entities) {
                std::cout << "Entity " << entityId << " has TagTest" << std::endl;
            }
        }
    };

    // Main demo function
    inline void runDemo() {
        ECS::Context context;
        // Register component types
        context.registerComponentType<PositionComponent>();
        context.registerComponentType<VelocityComponent>();
        context.registerComponentType<HealthComponent>();
        context.registerComponentType<Tag<"TagTest"_hs>>();

        const auto movementSystem = std::make_shared<MovementSystem>(context);
        const auto renderSystem = std::make_shared<RenderSystem>(context);
        const auto testSystem = std::make_shared<TestSystem>(context);
        const auto tagTestSystem = std::make_shared<TagTestSystem>(context);

        auto entity1 = HELPER::createEntityWithComponents(context, PositionComponent{5, 5}, VelocityComponent{1, 1}, HealthComponent{100}, Tag<"TagTest"_hs>{});
        auto entity2 = HELPER::createEntityWithComponents(context, PositionComponent{2.5, 2.5}, VelocityComponent{-0.5, -0.5}, HealthComponent{50});
        auto entity3 = HELPER::createEntityWithComponents(context, PositionComponent{0, 0}, HealthComponent{75});

        context.addSystem(movementSystem, 0);
        context.addSystem(renderSystem, 0);
        context.addSystem(testSystem, 1);
        context.addSystem(tagTestSystem, 2);

        // test event handling
        context.addEvent(1, [&]() { return true; });
        context.addEventHandler(1, [&testSystem] {
            std::cout << "Event 1 triggered. Calling TestSystem test function" << std::endl;
            testSystem->testFunction();
        });

        context.addEventHandler(1, [] {
            std::cout << "Alternate handler for Event 1" << std::endl;
        });

        context.addEvent(2, [&testSystem] { return testSystem->TestCondition(); });

        context.addEventHandler(2, [] {
            std::cout << "Event 2 triggered by TestSystem test condition" << std::endl;
        });

        context.updateEvents();

        for (int i = 0; i < 5; ++i) {
            std::cout << "Update " << i + 1 << std::endl;
            context.update();
        }

        std::cout << std::endl << "Original ECS Context serialised state:" << std::endl;
        std::cout << context;

        HELPER::writeContextToFile(context, "demo.tecs");

        ECS::Context context2;

        context2.registerComponentType<PositionComponent>();
        context2.registerComponentType<VelocityComponent>();
        context2.registerComponentType<HealthComponent>();
        context2.registerComponentType<Tag<"TagTest"_hs>>();

        HELPER::readContextFromFile(context2, "demo.tecs");

        std::cout << std::endl << "New ECS Context serialised state:" << std::endl;
        std::cout << context2;
    }
}

#endif //TENGINE_ECS_HPP