# TEngine ECS
This is a simple, single-header ECS in C++ 20.

# Getting Started
To start (after including the header), you can create an ECS context in your code like:

```cpp
ECS::Context context;
```

This is what handles everything about the ECS such as adding/removing entities, components, systems, handling signatures etc.

An entity can be created like this:

```cpp
const EntityId entity = context.createEntity(); // EntityId is just an unsigned int
```

Before that, to actually make these entities useful, you'll probably want to give them some components.

Here is an example PositionComponent:
```cpp
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
```
The << and >> operator overloadings are mainly necessary for serialisation (explained later). You can omit them if that is not something you are worried about.

Besides that, it just stores a float x and y.

It can be registered for use like this:
```cpp
context.registerComponent<PositionComponent>();
```

Then it can be added to our entity from before like this:
```cpp
context.addComponent(PositionComponent{2.5f, 324.2f}); // Arbitrary values
```

There is also a simpler way to create an entity with some components from the get go using the HELPER namespace:
```cpp

// An extra velocity component
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

context.registerComponent<VelocityComponent>();

const EntityId newEntity = HELPER::createEntityWithComponents(PositionComponent{3.5f, 324.2f}, VelocityComponent{-19.62f, 0.0f});

```

To get a reference to a component using the context:

```cpp
auto& newEntityPos = context.getComponent<PositionComponent>(newEntity);
newEntityPos.x = 0.5f // Arbitrary change
```

You can also remove components and destroy entities like so:

```cpp
context.removeComponent<VelocityComponent>(newEntity); // Now it only has a PositionComponent
context.destroyEntity(newEntity); // Now it doesn't exist

```

So that is all fine, but what if you want to add systems to operate on those components?

Well you can create it like so:
```cpp
Make the system:

class TestSystem : public ECS::System {
public:
    explicit TestSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, VelocityComponent>(context)) {}

    void update() override {
        for (const auto& entityId : m_Entities) {
            const auto& position = m_Context.getComponent<PositionComponent>(entityId); // taking a const reference since we are not changing the components
            const auto& velocity = m_Context.getComponent<VelocityComponent>(entityId);
            std::cout << "Entity " << entityId << " at " << position << " with " << velocity << std::endl;
        }
    }

    static void testFunction() {
        std::cout << "Test function called" << std::endl;
    }

    static bool testCondition() {
        return true;
    }
};

auto testSystem = std::make_shared<TestSystem>(context); // Make a shared pointer of the system with a reference to the context

context.addSystem(testSystem, 0); // Add the system
```

Firstly, you might be wondering what the createSignature function of the HELPER namespace does. 
This creates the Signature (using Signature = std::bitset<MAX_COMPONENTS>) which means that this system will only get the entities that at least have those components in its m_Entities unordered_set.

Secondly, you might be wondering about the 0 in context.addSystem(testSystem, 0); // Add the system.
This is the pipeline index is what allows us to access the very basic multithreading capabilities of the ECS. All systems created on pipeline 0 will run concurrently.
These pipelines run sequentially, so if I were to create another system on pipeline 1, it would execute afterwards.
You should use all consecutive pipeline indexes. So, for example, if I have used pipeline 1, and pipeline 5, I should have also used pipelines 2, 3, and 4.

Then to update all the systems:

```cpp
context.update();
```

Here is another example of how systems can be used:

```cpp
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

context.registerComponent<HealthComponent>();

class RenderSystem : public ECS::System {
public:
    int dt;
    
    explicit RenderSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, HealthComponent>(context)) {}

    void update() override {
        for (const auto& entityId : m_Entities) {
            const auto& position = m_Context.getComponent<PositionComponent>(entityId);
            const auto& health = m_Context.getComponent<HealthComponent>(entityId);
            std::cout << "Entity " << entityId << " at " << position << " with " << health << std::endl;
            std::cout << "dt: " << dt << std::endl;
            // not very realistic rendering :)
        }
    }
};

auto renderSystem = std::make_shared<RenderSystem>(context);

for (auto i = 0; i < 5; ++i)
{
    currentDt = 100 // you would calculate the actual value - just for example
    renderSystem->dt = currentDt;
    context.update()
}

```

As you can see, beyond running the system's update functions, doing other stuff with the system is on the user (except for events - explained later).

# Advanced features
coming soon
