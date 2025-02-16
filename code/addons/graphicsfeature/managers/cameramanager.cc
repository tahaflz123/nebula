//------------------------------------------------------------------------------
//  cameramanager.cc
//  (C) 2020 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "application/stdneb.h"
#include "cameramanager.h"
#include "graphics/graphicsentity.h"
#include "graphics/graphicsserver.h"
#include "graphics/cameracontext.h"
#include "visibility/visibilitycontext.h"
#include "game/gameserver.h"
#include "graphicsfeature/graphicsfeatureunit.h"
#include "graphics/view.h"
#include "basegamefeature/components/transform.h"

namespace GraphicsFeature
{

__ImplementSingleton(CameraManager)

//------------------------------------------------------------------------------
/**
*/
CameraManager::CameraManager()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
CameraManager::~CameraManager()
{
    // empty
}

//------------------------------------------------------------------------------
/**
*/
ViewHandle
CameraManager::RegisterView(Ptr<Graphics::View> const& view)
{
    n_assert(CameraManager::HasInstance());
    Ids::Id32 id;
    Singleton->viewHandlePool.Allocate(id);
    ViewData data;
    data.gid = Graphics::CreateEntity();
    data.view = view;
    if (Singleton->viewHandleMap.Size() <= Ids::Index(id))
        Singleton->viewHandleMap.Append(data);
    else
        Singleton->viewHandleMap[Ids::Index(id)] = data;
    
    if (view->GetCamera() != Graphics::GraphicsEntityId::Invalid())
        n_warning("WARNING: View already has a camera entity assigned which is being overridden!\n");

    Graphics::CameraContext::RegisterEntity(data.gid);
    Graphics::CameraContext::SetLODCamera(data.gid);
    Visibility::ObserverContext::RegisterEntity(data.gid);
    Visibility::ObserverContext::Setup(data.gid, Visibility::VisibilityEntityType::Camera);
    view->SetCamera(data.gid);

    return id;
}

//------------------------------------------------------------------------------
/**
*/
void
UpdateCameraSettings(Graphics::GraphicsEntityId gid, Camera& settings, Camera const& change)
{
    // check for changes
    bool changed = false;
    changed |= settings.fieldOfView != change.fieldOfView;
    changed |= settings.orthographicWidth != change.orthographicWidth;
    changed |= settings.projectionMode != change.projectionMode;
    changed |= settings.zFar != change.zFar;
    changed |= settings.zNear != change.zNear;
    changed |= settings.aspectRatio != change.aspectRatio;

    if (changed)
    {
        if (change.projectionMode == ProjectionMode::PERSPECTIVE)
            Graphics::CameraContext::SetupProjectionFov(gid, change.aspectRatio, Math::deg2rad(change.fieldOfView), change.zNear, change.zFar);
        else
            Graphics::CameraContext::SetupOrthographic(gid, change.orthographicWidth, change.orthographicWidth * change.aspectRatio, change.zNear, change.zFar);
        settings = change;
    }
    else
    {
        settings.localTransform = change.localTransform;
    }
}

//------------------------------------------------------------------------------
/**
*/
void
CameraManager::InitUpdateCameraProcessor()
{
    // Setup processor that handles both worldtransform and camera (heirarchy)
    Game::ProcessorBuilder("CameraManager.UpdateCameraWorldTransform").Func(
        [](Game::World*, Camera const& camera, Game::WorldTransform const& parentTransform)
        {
            if (IsViewHandleValid(camera.viewHandle))
            {
                Graphics::GraphicsEntityId gid = Singleton->viewHandleMap[Ids::Index(camera.viewHandle)].gid;
                Camera& settings = Singleton->viewHandleMap[Ids::Index(camera.viewHandle)].currentSettings;
                UpdateCameraSettings(gid, settings, camera);
                Graphics::CameraContext::SetView(gid, parentTransform.value * settings.localTransform);
            }
        }
    ).Build();

    // Setup processor that handles just a regular old camera
    Game::ProcessorBuilder("CameraManager.UpdateCamera")
        .Func([](Game::World* world, Camera const& camera)
        {
            if (IsViewHandleValid(camera.viewHandle))
            {
                Graphics::GraphicsEntityId gid = Singleton->viewHandleMap[Ids::Index(camera.viewHandle)].gid;
                Camera& settings = Singleton->viewHandleMap[Ids::Index(camera.viewHandle)].currentSettings;
                UpdateCameraSettings(gid, settings, camera);
                Graphics::CameraContext::SetView(gid, settings.localTransform);
            }
        })
        .Excluding<Game::WorldTransform>()
        .Build();
}

//------------------------------------------------------------------------------
/**
*/
Game::ManagerAPI
CameraManager::Create()
{
    n_assert(!CameraManager::HasInstance());
    CameraManager::Singleton = n_new(CameraManager);
   
    Singleton->InitUpdateCameraProcessor();

    Game::ManagerAPI api;
    api.OnDeactivate = &Destroy;
    return api;
}

//------------------------------------------------------------------------------
/**
*/
void
CameraManager::Destroy()
{
    n_assert(CameraManager::HasInstance());
    n_delete(CameraManager::Singleton);
    CameraManager::Singleton = nullptr;
}

//------------------------------------------------------------------------------
/**
*/
Math::mat4
CameraManager::GetProjection(ViewHandle handle)
{
    n_assert(CameraManager::HasInstance());
    auto gid = Singleton->viewHandleMap[Ids::Index(handle)].gid;
    return Graphics::CameraContext::GetProjection(gid);
}

//------------------------------------------------------------------------------
/**
*/
Math::mat4
CameraManager::GetLocalTransform(ViewHandle handle)
{
    n_assert(CameraManager::HasInstance());
    auto gid = Singleton->viewHandleMap[Ids::Index(handle)].gid;
    return Graphics::CameraContext::GetTransform(gid);
}

} // namespace Game
