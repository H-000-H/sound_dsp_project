#pragma once

enum class ModuleState
{
    Created = 0,
    Initialized,
    Started,
    Suspended,
    Stopped,
    Failed,
};

class Lifecycle
{
public:
    virtual ~Lifecycle() = default;

    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual ModuleState state() const = 0;
};
