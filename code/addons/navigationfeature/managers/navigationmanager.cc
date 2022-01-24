//------------------------------------------------------------------------------
//  navigationmanager.cc
//  (C) 2022 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "application/stdneb.h"
#include "navigationmanager.h"
#include "game/gameserver.h"
#include "properties/navigation.h"

namespace NavigationFeature
{

__ImplementSingleton(NavigationManager)

//------------------------------------------------------------------------------
/**
*/
NavigationManager::NavigationManager()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
NavigationManager::~NavigationManager()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
void NavigationManager::InitCreateAgentProcessor()
{
    Game::FilterCreateInfo filterInfo;
    filterInfo.inclusive[0] = Game::GetPropertyId("Owner");
    filterInfo.access[0] = Game::AccessMode::READ;
    filterInfo.inclusive[1] = Game::GetPropertyId("WorldTransform");
    filterInfo.access[1] = Game::AccessMode::READ;
    filterInfo.inclusive[2] = Game::GetPropertyId("AgentKind");
    filterInfo.access[2] = Game::AccessMode::READ;
    filterInfo.numInclusive = 3;

    filterInfo.exclusive[0] = this->pids.navigationActor;
    filterInfo.numExclusive = 1;

    Game::Filter filter = Game::CreateFilter(filterInfo);

    Game::ProcessorCreateInfo processorInfo;
    processorInfo.async = false;
    processorInfo.filter = filter;
    processorInfo.name = "NavigationManager.CreateAgents"_atm;
    processorInfo.OnBeginFrame = [](Game::World* world, Game::Dataset data)
    {
        Game::OpBuffer opBuffer = Game::CreateOpBuffer(world);
        
        for (int v = 0; v < data.numViews; v++)
        {
            Game::Dataset::CategoryTableView const& view = data.views[v];
            Game::Entity const* const owners = (Game::Entity*)view.buffers[0];
            Math::mat4 const* const transforms = (Math::mat4*)view.buffers[1];
            Util::String const* const kinds = (Util::String*)view.buffers[2];
            
            for (IndexT i = 0; i < view.numInstances; ++i)
            {
                Game::Entity const& entity = owners[i];
                Math::mat4 const& trans = transforms[i];
                Util::String const& kind = kinds[i];
                /*

                Resources::CreateResource(res, "PHYS",
                    [world, trans, &opBuffer, entity, staticPID](Resources::ResourceId id)
                    {
                        bool const dynamic = !Game::HasProperty(world, entity, staticPID);
                        Physics::ActorId actorid = Physics::CreateActorInstance(id, trans, dynamic, Ids::Id32(entity));
                        
                        Game::Op::RegisterProperty regOp;
                        regOp.entity = entity;
                        regOp.pid = PhysicsManager::Singleton->pids.physicsActor;
                        regOp.value = &actorid;
                        Game::AddOp(opBuffer, regOp);

                    }, nullptr, true);

                */
            }
        }

        // execute ops
        Game::Dispatch(opBuffer);
    };
    
    Game::ProcessorHandle pHandle = Game::CreateProcessor(processorInfo);
}

//------------------------------------------------------------------------------
/**
*/
void
NavigationManager::OnDecay()
{
    /*
    Game::PropertyDecayBuffer const decayBuffer = Game::GetDecayBuffer(Singleton->pids.physicsActor);
    Physics::ActorId* data = (Physics::ActorId*)decayBuffer.buffer;
    for (int i = 0; i < decayBuffer.size; i++)
    {
        Physics::DestroyActorInstance(data[i]);
    }
    */
}

//------------------------------------------------------------------------------
/**
*/
void NavigationManager::InitUpdateAgentTransformProcessor()
{
    Game::FilterCreateInfo filterInfo;
    filterInfo.inclusive[0] = this->pids.navigationActor;
    filterInfo.access[0] = Game::AccessMode::READ;
    filterInfo.inclusive[1] = Game::GetPropertyId("WorldTransform"_atm);
    filterInfo.access[1] = Game::AccessMode::WRITE;
    filterInfo.numInclusive = 2;

    filterInfo.exclusive[0] = Game::GetPropertyId("Static");
    filterInfo.numExclusive = 1;

    Game::Filter filter = Game::CreateFilter(filterInfo);

    Game::ProcessorCreateInfo processorInfo;
    processorInfo.async = false;
    processorInfo.filter = filter;
    processorInfo.name = "PhysicsManager.PollTransforms"_atm;
    processorInfo.OnFrame = [](Game::World* world, Game::Dataset data)
    {
        /*
        for (int v = 0; v < data.numViews; v++)
        {
            Game::Dataset::CategoryTableView const& view = data.views[v];
            Physics::ActorId const* const actorIdBuffer = (Physics::ActorId*)view.buffers[0];
            Math::mat4* const transforms = (Math::mat4*)view.buffers[1];

            for (IndexT i = 0; i < view.numInstances; ++i)
            {
                Physics::ActorId const actorid = actorIdBuffer[i];
                Math::mat4& transform = transforms[i];
                transform = Physics::ActorContext::GetTransform(actorid);
            }
        }
        */
    };

    Game::ProcessorHandle pHandle = Game::CreateProcessor(processorInfo);
}

//------------------------------------------------------------------------------
/**
*/
Game::ManagerAPI
NavigationManager::Create()
{
	n_assert(NavigationFeature::Details::navigation_registered);
	n_assert(!NavigationManager::HasInstance());
    NavigationManager::Singleton = n_new(NavigationManager);

    /*
    Game::PropertyCreateInfo info;
    info.name = "PhysicsActorId";
    Navigation::ActorId defaultValue;
    info.defaultValue = &defaultValue;
    info.flags = Game::PropertyFlags::PROPERTYFLAG_MANAGED;
    info.byteSize = sizeof(Physics::ActorId);
    Singleton->pids.navigationActor = Game::CreateProperty(info);

    Singleton->InitCreateAgentProcessor();
    Singleton->InitUpdateAgentTransformProcessor();
    */
    Game::ManagerAPI api;
    api.OnCleanup    = &OnCleanup;
    api.OnDeactivate = &Destroy;
    api.OnDecay      = &OnDecay;
    return api;
}

//------------------------------------------------------------------------------
/**
*/
void
NavigationManager::Destroy()
{
    n_delete(NavigationManager::Singleton);
    NavigationManager::Singleton = nullptr;
}

//------------------------------------------------------------------------------
/**
*/
void
NavigationManager::OnCleanup(Game::World* world)
{
    n_assert(NavigationManager::HasInstance());
    Game::FilterCreateInfo filterInfo;
    filterInfo.inclusive[0] = Singleton->pids.navigationActor;
    filterInfo.access[0] = Game::AccessMode::WRITE;
    filterInfo.numInclusive = 1;

    Game::Filter filter = Game::CreateFilter(filterInfo);
    Game::Dataset data = Game::Query(world, filter);
    for (int v = 0; v < data.numViews; v++)
    {
        /*
        Game::Dataset::CategoryTableView const& view = data.views[v];
        Physics::ActorId* const actors = (Physics::ActorId*)view.buffers[0];

        for (IndexT i = 0; i < view.numInstances; ++i)
        {
            Physics::ActorId const& actorid = actors[i];
            Physics::DestroyActorInstance(actorid);
        }
        */
    }

    Game::DestroyFilter(filter);
}

} // namespace PhysicsFeature
