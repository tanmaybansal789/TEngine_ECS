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

context.addSystem(testSystem, 0); // Add the system on pipeline 0
```

Firstly, you might be wondering what the createSignature function of the HELPER namespace does. 
This creates the Signature (```using Signature = std::bitset<MAX_COMPONENTS>```) which means that this system will only get the entities that at least have those components in its m_Entities unordered_set.

Secondly, you might be wondering about the 0 in context.addSystem(testSystem, 0); // Add the system.
This is the pipeline index is what allows us to access the very basic multithreading capabilities of the ECS. All systems created on pipeline 0 will run concurrently.
These pipelines run sequentially, so if I were to create another system on pipeline 1, it would execute afterwards.
You should use all consecutive pipeline indexes from 0. So, for example, if I have used pipeline 5, I should have also used pipelines 0, 1, 2, 3, and 4 at least once. They are run using std::async

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

Even though that defines a basic ECS, and technically speaking, there isn't anything else <i> needed </i> there are a handful of other features.

<h3> Events </h3>

Basically, this is a way for you to do something dynamically if some condition is met, rather than just be stuck with systems that are closed off from one another. 

Let's bring back our TestSystem again.

```cpp
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

context.addSystem(testSystem, 0); // Add the system on pipeline 0
```

You can have the system add an event handler like so:
(the _hs you'll see after the string literals is a consteval FNV-1a hash to convert a string to an unsigned int or in this case EventId - it is defined at the top of the header.
It could just as well be an unsigned int like 1 or 2 or 3000):

```cpp
class TestSystem : public ECS::System {
public:
    explicit TestSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, VelocityComponent>(context)) {
        context.addEvent("SomeEvent"_hs, testCondition);
        context.addEventHandler("SomeEvent"_hs, testFunction);
    }

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

context.addSystem(testSystem, 0); // Add the system on pipeline 0
```

Or you could do it outside the system:

```cpp
context.addEvent("SomeEvent"_hs, [&testSystem] { return testSystem->testCondition(); });

context.addEventHandler("SomeEvent"_hs, [&testSystem] {
    testSystem->testFunction()
});
```

Although, probably the best method would be to add the event outside:

```cpp
context.addEvent("SomeEvent"_hs, [&testSystem] { return testSystem->testCondition(); });
```

and have your system add a handler/subscriber:
```cpp
explicit TestSystem(ECS::Context& context) : System(context, HELPER::createSignature<PositionComponent, VelocityComponent>(context)) {
    context.addEventHandler("SomeEvent"_hs, testFunction);
}
```
This means that you easily create multiple subscribers to the event without re-adding the event.

Finally, you can update the context's events like so:

```cpp
context.updateEvents()
```

This will trigger any events whose conditions are ```true``` at that point.

Ok, so that is events - it can sometimes be helpful to allow for communication between systems.

<h3> Tags </h3>

A really short one - this is an easier way to create "marker" components that don't store any data, to filter entities (This is essentially the exact same as how EnTT (https://github.com/skypjack/entt) handles tags).

They can be registered and used like so:
```cpp
context.registerComponent<Tag<some unsigned int>>();
```
Alternatively, you can again use the _hs to work with tags like so
```cpp
context.registerComponent<Tag<"Something"_hs>>();
```

So then you can add it to our entity ```entity```:
```cpp
context.addComponent(entity, Tag<"Something"_hs">);
```
Now our entity is officially a "Something" (or technically has the tag ```Tag<1243378187>```).

This can be used in systems like any other component:

```cpp
class TagTestSystem : public ECS::System {
public:
    explicit TagTestSystem(ECS::Context& context) : System(context, HELPER::createSignature<Tag<"Something"_hs>>(context)) {}

    void update() override {
        for (const auto& entityId : m_Entities) {
            std::cout << "Entity " << entityId << " has Something" << std::endl;
        }
    }
};
```

And it could come in handy for anything like differentiating between players/enemies even if they have the same components.

Obviously, this wouldn't be the hardest to implement yourself (the so-called Tag is just an std::integral_constant, for which we've defined << and >> operator overloadings).

```cpp
struct SomethingComponent{};

context.registerComponent<SomethingComponent>();

class SomethingSystem : public ECS::System {
public:
    explicit TagTestSystem(ECS::Context& context) : System(context, HELPER::createSignature<SomethingComponent>(context)) {}

    void update() override {
        for (const auto& entityId : m_Entities) {
            std::cout << "Entity " << entityId << " has Something" << std::endl;
        }
    }
};
```

<h3> Serialisation </h3>

So, you want to save and reload your states of the ECS. A pretty common requirement, given that it is quite a nice feature to be able to save your game, for example.

Firstly, make sure that your components have a << and >> (insertion and extraction) operator overloaded. It could look like:
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
```

Then you can just serialise by performing << on the context, with some ostream.

It would look like:

```
some_ostream << context;
```

If done to std::cout that would print something out in this format:

```
# ECS Serialisation
# Version: 1.0 

# Entities
EntityCount: 3
NextEntityId: 3
FreedEntityList: 
Entity: 0, Signature: 00000000000000000000000000001111
Entity: 1, Signature: 00000000000000000000000000000111
Entity: 2, Signature: 00000000000000000000000000000101

# Components
NextComponentTypeId: 4
ComponentType: N4DEMO17PositionComponentE
Entity: 0, Position: 10 10
Entity: 1, Position: 0 0
Entity: 2, Position: 0 0
ComponentType: N4DEMO17VelocityComponentE
Entity: 0, Velocity: 1 1
Entity: 1, Velocity: -0.5 -0.5
ComponentType: N4DEMO15HealthComponentE
Entity: 0, Health: 100
Entity: 1, Health: 50
Entity: 2, Health: 75
ComponentType: NSt3__117integral_constantIKjLj1243378187EEE
Entity: 0, Tag<1243378187>
```

This is just what you will get from the demo provided in the code (the typenames are mangled).

To read from in from an istream you can just write:

```cpp
some_istream >> context;
```

provided that you register the same components in the same order for the new context.

So here is an example of what you would be able to do:

```cpp
ECS::Context context1;
context1.registerComponent<PositionComponent>();
context1.registerComponent<VelocityComponent>();
context1.registerComponent<HealthComponent>();

// Add some entities and components:
const auto entity1 = HELPER::createEntityWithComponents(context1, PositionComponent{1, 2}, HealthComponent{2});
const auto entity2 = HELPER::createEntityWithComponents(context1, VelocityComponent{0, 0}, PositionComponent{0, 42});
const auto entity3 = HELPER::createEntityWithComponents(context1, HealthComponent{20});

// A new context
ECS::Context context2;
context2.registerComponent<PositionComponent>();
context2.registerComponent<VelocityComponent>();
context2.registerComponent<HealthComponent>();
std::stringstream buffer;
buffer << context; // Serialise the source context into the buffer
buffer >> context2; // Deserialise the buffer into the destination context

// Now context2 has all entities and components from context
```

However, there is also the option of serialising it to a file and writing that file, using the HELPER::writeContextToFile and HELPER::readContextFromFile functions:
```cpp
HELPER::writeContextToFile(context, "demo.tecs");

ECS::Context context2;

context2.registerComponentType<PositionComponent>();
context2.registerComponentType<VelocityComponent>();
context2.registerComponentType<HealthComponent>();
context2.registerComponentType<Tag<"Something"_hs>>();

HELPER::readContextFromFile(context2, "demo.tecs");

std::cout << std::endl << "New ECS Context serialised state:" << std::endl;
std::cout << context2;
```

You pass in the context along with a path. 

That basically covers everything about the ECS in its current state.

I hope this helped you understand a bit more about it!
This was a fun project to work on and I want to highlight, along with skypjacks ECS back and forth blogs (https://skypjack.github.io/), 
this blog post by Austin Morlan on his implementation: https://austinmorlan.com/posts/entity_component_system/.

They are very useful for understanding the ECS and its usage/limitations, as well as its implementations.
😃
