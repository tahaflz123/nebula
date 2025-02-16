//------------------------------------------------------------------------------
//  volumetricfogcontext.cc
//  (C) 2020 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "volumetricfogcontext.h"
#include "graphics/graphicsserver.h"
#include "clustering/clustercontext.h"
#include "graphics/cameracontext.h"
#include "graphics/view.h"
#include "lighting/lightcontext.h"
#include "frame/framesubgraph.h"
#include "frame/framecode.h"
#include "imgui.h"

#include "volumefog.h"
#include "blur_2d_rgba16f_cs.h"
namespace Fog
{

VolumetricFogContext::FogGenericVolumeAllocator VolumetricFogContext::fogGenericVolumeAllocator;
VolumetricFogContext::FogBoxVolumeAllocator VolumetricFogContext::fogBoxVolumeAllocator;
VolumetricFogContext::FogSphereVolumeAllocator VolumetricFogContext::fogSphereVolumeAllocator;
__ImplementContext(VolumetricFogContext, VolumetricFogContext::fogGenericVolumeAllocator);

struct
{
    CoreGraphics::ShaderId classificationShader;
    CoreGraphics::ShaderProgramId cullProgram;
    CoreGraphics::ShaderProgramId renderProgram;

    CoreGraphics::BufferId clusterFogIndexLists;

    Util::FixedArray<CoreGraphics::BufferId> stagingClusterFogLists;
    CoreGraphics::BufferId clusterFogLists;

    CoreGraphics::TextureId fogVolumeTexture0;
    CoreGraphics::TextureId fogVolumeTexture1;
    CoreGraphics::TextureId zBuffer;

    IndexT uniformsSlot;
    IndexT lightingTextureSlot;
    IndexT clusterUniforms;

    Util::FixedArray<CoreGraphics::ResourceTableId> resourceTables;
    float turbidity;
    Math::vec3 color;

    // these are used to update the light clustering
    Volumefog::FogBox fogBoxes[128];
    Volumefog::FogSphere fogSpheres[128];

    Memory::ArenaAllocator<sizeof(Frame::FrameCode) * 5> frameOpAllocator;

    bool showUI = false;
} fogState;

struct
{
    CoreGraphics::ShaderId blurShader;
    CoreGraphics::ShaderProgramId blurXProgram, blurYProgram;
    Util::FixedArray<CoreGraphics::ResourceTableId> blurXTable, blurYTable;
    IndexT blurInputXSlot, blurInputYSlot, blurOutputXSlot, blurOutputYSlot;
} blurState;


//------------------------------------------------------------------------------
/**
*/
VolumetricFogContext::VolumetricFogContext()
{

}

//------------------------------------------------------------------------------
/**
*/
VolumetricFogContext::~VolumetricFogContext()
{

}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::Create(const Ptr<Frame::FrameScript>& frameScript)
{
    __CreateContext();

    __bundle.OnUpdateViewResources = VolumetricFogContext::UpdateViewDependentResources;
    __bundle.OnBegin = VolumetricFogContext::RenderUI;

    Graphics::GraphicsServer::Instance()->RegisterGraphicsContext(&__bundle, &__state);

    using namespace CoreGraphics;
    fogState.classificationShader = ShaderServer::Instance()->GetShader("shd:volumefog.fxb");
    fogState.uniformsSlot = ShaderGetResourceSlot(fogState.classificationShader, "VolumeFogUniforms");
    fogState.lightingTextureSlot = ShaderGetResourceSlot(fogState.classificationShader, "Lighting");

    IndexT fogIndexListsSlot = ShaderGetResourceSlot(fogState.classificationShader, "FogIndexLists");
    IndexT fogListsSlot = ShaderGetResourceSlot(fogState.classificationShader, "FogLists");

    IndexT lightIndexListsSlot = ShaderGetResourceSlot(fogState.classificationShader, "LightIndexLists");
    IndexT lightListsSlot = ShaderGetResourceSlot(fogState.classificationShader, "LightLists");
    IndexT clusterAABBSlot = ShaderGetResourceSlot(fogState.classificationShader, "ClusterAABBs");

    BufferCreateInfo rwbInfo;
    rwbInfo.name = "FogIndexListsBuffer";
    rwbInfo.size = 1;
    rwbInfo.elementSize = sizeof(Volumefog::FogIndexLists);
    rwbInfo.mode = BufferAccessMode::DeviceLocal;
    rwbInfo.usageFlags = CoreGraphics::ReadWriteBuffer | CoreGraphics::TransferBufferDestination;
    rwbInfo.queueSupport = CoreGraphics::GraphicsQueueSupport | CoreGraphics::ComputeQueueSupport;
    fogState.clusterFogIndexLists = CreateBuffer(rwbInfo);

    rwbInfo.name = "FogLists";
    rwbInfo.elementSize = sizeof(Volumefog::FogLists);
    fogState.clusterFogLists = CreateBuffer(rwbInfo);

    rwbInfo.name = "FogListsStagingBuffer";
    rwbInfo.mode = BufferAccessMode::HostLocal;
    rwbInfo.usageFlags = CoreGraphics::TransferBufferSource;
    fogState.stagingClusterFogLists.Resize(CoreGraphics::GetNumBufferedFrames());

    fogState.cullProgram = ShaderGetProgram(fogState.classificationShader, ShaderServer::Instance()->FeatureStringToMask("Cull"));
    fogState.renderProgram = ShaderGetProgram(fogState.classificationShader, ShaderServer::Instance()->FeatureStringToMask("Render"));

    fogState.resourceTables.Resize(CoreGraphics::GetNumBufferedFrames());
    for (IndexT i = 0; i < fogState.resourceTables.Size(); i++)
    {
        fogState.resourceTables[i] = ShaderCreateResourceTable(fogState.classificationShader, NEBULA_BATCH_GROUP, fogState.resourceTables.Size());
    }

    // get per-view resource tables
    const Util::FixedArray<CoreGraphics::ResourceTableId>& viewTables = TransformDevice::Instance()->GetViewResourceTables();

    for (IndexT i = 0; i < viewTables.Size(); i++)
    {
        fogState.stagingClusterFogLists[i] = CreateBuffer(rwbInfo);

        ResourceTableSetRWBuffer(viewTables[i], { fogState.clusterFogIndexLists, fogIndexListsSlot, 0, false, false, NEBULA_WHOLE_BUFFER_SIZE, 0 });
        ResourceTableSetRWBuffer(viewTables[i], { fogState.clusterFogLists, fogListsSlot, 0, false, false, NEBULA_WHOLE_BUFFER_SIZE, 0 });
        ResourceTableSetConstantBuffer(viewTables[i], { CoreGraphics::GetComputeConstantBuffer(), fogState.uniformsSlot, 0, false, false, sizeof(Volumefog::VolumeFogUniforms), 0 });
    }

    blurState.blurShader = ShaderServer::Instance()->GetShader("shd:blur_2d_rgba16f_cs.fxb");
    blurState.blurXProgram = ShaderGetProgram(blurState.blurShader, ShaderServer::Instance()->FeatureStringToMask("Alt0"));
    blurState.blurYProgram = ShaderGetProgram(blurState.blurShader, ShaderServer::Instance()->FeatureStringToMask("Alt1"));
    blurState.blurXTable.Resize(CoreGraphics::GetNumBufferedFrames());
    blurState.blurYTable.Resize(CoreGraphics::GetNumBufferedFrames()); 

    for (IndexT i = 0; i < blurState.blurXTable.Size(); i++)
    {
        blurState.blurXTable[i] = ShaderCreateResourceTable(blurState.blurShader, NEBULA_BATCH_GROUP, blurState.blurXTable.Size());
        blurState.blurYTable[i] = ShaderCreateResourceTable(blurState.blurShader, NEBULA_BATCH_GROUP, blurState.blurXTable.Size());
    }

    blurState.blurInputXSlot = ShaderGetResourceSlot(blurState.blurShader, "InputImageX");
    blurState.blurInputYSlot = ShaderGetResourceSlot(blurState.blurShader, "InputImageY");
    blurState.blurOutputXSlot = ShaderGetResourceSlot(blurState.blurShader, "BlurImageX");
    blurState.blurOutputYSlot = ShaderGetResourceSlot(blurState.blurShader, "BlurImageY");

    fogState.turbidity = 0.1f;
    fogState.color = Math::vec3(1);

    fogState.fogVolumeTexture0 = frameScript->GetTexture("VolumetricFogBuffer0");
    fogState.fogVolumeTexture1 = frameScript->GetTexture("VolumetricFogBuffer1");
    fogState.zBuffer = frameScript->GetTexture("ZBuffer");

     // The first pass is to copy decals over
    Frame::FrameCode* fogCopy = fogState.frameOpAllocator.Alloc<Frame::FrameCode>();
    fogCopy->domain = CoreGraphics::BarrierDomain::Global;
    fogCopy->queue = CoreGraphics::QueueType::ComputeQueueType;
    fogCopy->bufferDeps.Add(fogState.clusterFogLists,
                                {
                                    "Decals List"
                                    , CoreGraphics::PipelineStage::TransferWrite
                                    , CoreGraphics::BufferSubresourceInfo()
                                });
    fogCopy->func = [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
    {
        CoreGraphics::BufferCopy from, to;
        from.offset = 0;
        to.offset = 0;
        CmdCopy(cmdBuf, fogState.stagingClusterFogLists[bufferIndex], { from }, fogState.clusterFogLists, { to }, sizeof(Volumefog::FogLists));
    };

    // The second pass is to cull the decals based on screen space AABBs
    Frame::FrameCode* fogCull = fogState.frameOpAllocator.Alloc<Frame::FrameCode>();
    fogCull->domain = CoreGraphics::BarrierDomain::Global;
    fogCull->queue = CoreGraphics::QueueType::ComputeQueueType;
    fogCull->bufferDeps.Add(fogState.clusterFogLists,
                            {
                                "Fog List"
                                , CoreGraphics::PipelineStage::ComputeShaderRead
                                , CoreGraphics::BufferSubresourceInfo()
                            });
    fogCull->bufferDeps.Add(fogState.clusterFogIndexLists,
                            {
                                "Fog Index Lists"
                                , CoreGraphics::PipelineStage::ComputeShaderWrite
                                , CoreGraphics::BufferSubresourceInfo()
                            });
    fogCull->bufferDeps.Add(Clustering::ClusterContext::GetClusterBuffer(),
                            {
                                "Cluster AABB"
                                , CoreGraphics::PipelineStage::ComputeShaderRead
                                , CoreGraphics::BufferSubresourceInfo()
                            });
    fogCull->func = [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
    {
        CmdSetShaderProgram(cmdBuf, fogState.cullProgram);

        // Run chunks of 1024 threads at a time
        std::array<SizeT, 3> dimensions = Clustering::ClusterContext::GetClusterDimensions();

        CmdDispatch(cmdBuf, Math::ceil((dimensions[0] * dimensions[1] * dimensions[2]) / 64.0f), 1, 1);
    };
    Frame::AddSubgraph("Fog Cull", { fogCopy, fogCull });

    Frame::FrameCode* fogCompute = fogState.frameOpAllocator.Alloc<Frame::FrameCode>();
    fogCompute->domain = CoreGraphics::BarrierDomain::Global;
    fogCompute->bufferDeps.Add(fogState.clusterFogIndexLists,
                                {
                                    "Fog Index Lists"
                                    , CoreGraphics::PipelineStage::ComputeShaderRead
                                    , CoreGraphics::BufferSubresourceInfo()
                                });
    fogCompute->textureDeps.Add(fogState.fogVolumeTexture0,
                                {
                                    "Fog Volume Texture 0"
                                    , CoreGraphics::PipelineStage::ComputeShaderWrite
                                    , CoreGraphics::ImageSubresourceInfo::ColorNoMipNoLayer()
                                });
    fogCompute->textureDeps.Add(fogState.zBuffer,
                                {
                                    "ZBuffer"
                                    , CoreGraphics::PipelineStage::ComputeShaderRead
                                    , CoreGraphics::ImageSubresourceInfo::DepthStencilNoMipNoLayer()
                                });
    fogCompute->func = [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
    {
        CmdSetShaderProgram(cmdBuf, fogState.renderProgram);

        // Set frame resources
        CmdSetResourceTable(cmdBuf, fogState.resourceTables[bufferIndex], NEBULA_BATCH_GROUP, CoreGraphics::ComputePipeline, nullptr);

        // run volumetric fog compute
        TextureDimensions dims = TextureGetDimensions(fogState.fogVolumeTexture0);
        CmdDispatch(cmdBuf, Math::divandroundup(dims.width, 64), dims.height, 1);
    };

    Frame::FrameCode* blurX = fogState.frameOpAllocator.Alloc<Frame::FrameCode>();
    blurX->domain = CoreGraphics::BarrierDomain::Global;
    blurX->textureDeps.Add(fogState.fogVolumeTexture0,
                                {
                                    "Fog Volume Texture 0"
                                    , CoreGraphics::PipelineStage::ComputeShaderRead
                                    , CoreGraphics::ImageSubresourceInfo::ColorNoMipNoLayer()
                                });
    blurX->textureDeps.Add(fogState.fogVolumeTexture1,
                                {
                                    "Fog Volume Texture 1"
                                    , CoreGraphics::PipelineStage::ComputeShaderWrite
                                    , CoreGraphics::ImageSubresourceInfo::ColorNoMipNoLayer()
                                });
    blurX->func = [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
    {
        CmdSetShaderProgram(cmdBuf, blurState.blurXProgram);
        CmdSetResourceTable(cmdBuf, blurState.blurXTable[bufferIndex], NEBULA_BATCH_GROUP, ComputePipeline, nullptr);

        // fog0 -> read, fog1 -> write
        TextureDimensions dims = TextureGetDimensions(fogState.fogVolumeTexture0);
        CmdDispatch(cmdBuf, Math::divandroundup(dims.width, Blur2dRgba16fCs::BlurTileWidth), dims.height, 1);
    };

    Frame::FrameCode* blurY = fogState.frameOpAllocator.Alloc<Frame::FrameCode>();
    blurY->domain = CoreGraphics::BarrierDomain::Global;
    blurY->textureDeps.Add(fogState.fogVolumeTexture1,
                                {
                                    "Fog Volume Texture 1"
                                    , CoreGraphics::PipelineStage::ComputeShaderRead
                                    , CoreGraphics::ImageSubresourceInfo::ColorNoMipNoLayer()
                                });
    blurY->textureDeps.Add(fogState.fogVolumeTexture0,
                                {
                                    "Fog Volume Texture 0"
                                    , CoreGraphics::PipelineStage::ComputeShaderWrite
                                    , CoreGraphics::ImageSubresourceInfo::ColorNoMipNoLayer()
                                });
    blurY->func = [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
    {
        CmdSetShaderProgram(cmdBuf, blurState.blurYProgram);
        CmdSetResourceTable(cmdBuf, blurState.blurYTable[bufferIndex], NEBULA_BATCH_GROUP, ComputePipeline, nullptr);

        // fog0 -> read, fog1 -> write
        TextureDimensions dims = TextureGetDimensions(fogState.fogVolumeTexture0);
        CmdDispatch(cmdBuf, Math::divandroundup(dims.height, Blur2dRgba16fCs::BlurTileWidth), dims.width, 1);
    };
    Frame::AddSubgraph("Fog Compute", { fogCompute, blurX, blurY });
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::Discard()
{
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetupBoxVolume(
    const Graphics::GraphicsEntityId id, 
    const Math::mat4& transform,
    const float density, 
    const Math::vec3& absorption)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    Ids::Id32 fog = fogBoxVolumeAllocator.Alloc();
    fogBoxVolumeAllocator.Set<FogBoxVolume_Transform>(fog, transform);

    fogGenericVolumeAllocator.Set<FogVolume_Type>(cid.id, BoxVolume);
    fogGenericVolumeAllocator.Set<FogVolume_TypedId>(cid.id, fog);
    fogGenericVolumeAllocator.Set<FogVolume_Turbidity>(cid.id, density);
    fogGenericVolumeAllocator.Set<FogVolume_Absorption>(cid.id, absorption);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetBoxTransform(const Graphics::GraphicsEntityId id, const Math::mat4& transform)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    Ids::Id32 lid = fogGenericVolumeAllocator.Get<FogVolume_TypedId>(cid.id);
    FogVolumeType type = fogGenericVolumeAllocator.Get<FogVolume_Type>(cid.id);
    n_assert(type == FogVolumeType::BoxVolume);
    fogBoxVolumeAllocator.Set<FogBoxVolume_Transform>(lid, transform);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetupSphereVolume(
    const Graphics::GraphicsEntityId id, 
    const Math::vec3& position,
    float radius, 
    const float density, 
    const Math::vec3& absorption)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    Ids::Id32 fog = fogSphereVolumeAllocator.Alloc();
    fogSphereVolumeAllocator.Set<FogSphereVolume_Position>(fog, position);
    fogSphereVolumeAllocator.Set<FogSphereVolume_Radius>(fog, radius);

    fogGenericVolumeAllocator.Set<FogVolume_Type>(cid.id, SphereVolume);
    fogGenericVolumeAllocator.Set<FogVolume_TypedId>(cid.id, fog);
    fogGenericVolumeAllocator.Set<FogVolume_Turbidity>(cid.id, density);
    fogGenericVolumeAllocator.Set<FogVolume_Absorption>(cid.id, absorption);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetSpherePosition(const Graphics::GraphicsEntityId id, const Math::vec3& position)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    Ids::Id32 lid = fogGenericVolumeAllocator.Get<FogVolume_TypedId>(cid.id);
    FogVolumeType type = fogGenericVolumeAllocator.Get<FogVolume_Type>(cid.id);
    n_assert(type == FogVolumeType::SphereVolume);
    fogSphereVolumeAllocator.Set<FogSphereVolume_Position>(lid, position);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetSphereRadius(const Graphics::GraphicsEntityId id, const float radius)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    Ids::Id32 lid = fogGenericVolumeAllocator.Get<FogVolume_TypedId>(cid.id);
    FogVolumeType type = fogGenericVolumeAllocator.Get<FogVolume_Type>(cid.id);
    n_assert(type == FogVolumeType::SphereVolume);
    fogSphereVolumeAllocator.Set<FogSphereVolume_Radius>(lid, radius);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetTurbidity(const Graphics::GraphicsEntityId id, const float turbidity)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    fogGenericVolumeAllocator.Set<FogVolume_Turbidity>(cid.id, turbidity);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetAbsorption(const Graphics::GraphicsEntityId id, const Math::vec3& absorption)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    fogGenericVolumeAllocator.Set<FogVolume_Absorption>(cid.id, absorption);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::UpdateViewDependentResources(const Ptr<Graphics::View>& view, const Graphics::FrameContext& ctx)
{
    using namespace CoreGraphics;
    IndexT bufferIndex = CoreGraphics::GetBufferedFrameIndex();
    Math::mat4 viewTransform = Graphics::CameraContext::GetView(view->GetCamera());

    SizeT numFogBoxVolumes = 0;
    SizeT numFogSphereVolumes = 0;

    const Util::Array<FogVolumeType>& types = fogGenericVolumeAllocator.GetArray<FogVolume_Type>();
    const Util::Array<Ids::Id32>& typeIds = fogGenericVolumeAllocator.GetArray<FogVolume_TypedId>();
    IndexT i;
    for (i = 0; i < types.Size(); i++)
    {
        switch (types[i])
        {
        case BoxVolume:
        {
            auto& fog = fogState.fogBoxes[numFogBoxVolumes];
            fogGenericVolumeAllocator.Get<FogVolume_Absorption>(i).store(fog.absorption);
            fog.turbidity = fogGenericVolumeAllocator.Get<FogVolume_Turbidity>(i);
            fog.falloff = 64.0f;
            Math::mat4 transform = fogBoxVolumeAllocator.Get<FogBoxVolume_Transform>(typeIds[i]) * viewTransform;
            Math::bbox box(transform);
            box.pmin.store3(fog.bboxMin);
            box.pmax.store3(fog.bboxMax);
            inverse(transform).store(fog.invTransform);
            numFogBoxVolumes++;
            break;
        }
        case SphereVolume:
        {
            auto& fog = fogState.fogSpheres[numFogSphereVolumes];
            fogGenericVolumeAllocator.Get<FogVolume_Absorption>(i).store(fog.absorption);
            fog.turbidity = fogGenericVolumeAllocator.Get<FogVolume_Turbidity>(i);
            Math::vec4 pos = Math::vec4(fogSphereVolumeAllocator.Get<FogSphereVolume_Position>(typeIds[i]), 1);
            pos = viewTransform * pos;
            pos.store3(fog.position);
            fog.radius = fogSphereVolumeAllocator.Get<FogSphereVolume_Radius>(typeIds[i]);
            fog.falloff = 64.0f;
            numFogSphereVolumes++;
            break;
        }
        }
    }

    // update list of point lights
    if (numFogBoxVolumes > 0 || numFogSphereVolumes > 0)
    {
        Volumefog::FogLists fogList;
        Memory::CopyElements(fogState.fogBoxes, fogList.FogBoxes, numFogBoxVolumes);
        Memory::CopyElements(fogState.fogSpheres, fogList.FogSpheres, numFogSphereVolumes);
        CoreGraphics::BufferUpdate(fogState.stagingClusterFogLists[bufferIndex], fogList);
        CoreGraphics::BufferFlush(fogState.stagingClusterFogLists[bufferIndex]);
    }

    Volumefog::VolumeFogUniforms fogUniforms;
    fogUniforms.NumFogBoxes = numFogBoxVolumes;
    fogUniforms.NumFogSpheres = numFogSphereVolumes;
    fogUniforms.NumVolumeFogClusters = Clustering::ClusterContext::GetNumClusters();
    fogUniforms.GlobalTurbidity = fogState.turbidity;
    fogState.color.store(fogUniforms.GlobalAbsorption);
    fogUniforms.DownscaleFog = 4;


    TextureDimensions dims = TextureGetDimensions(fogState.fogVolumeTexture0);

    ResourceTableSetRWTexture(fogState.resourceTables[bufferIndex], { fogState.fogVolumeTexture0, fogState.lightingTextureSlot, 0, CoreGraphics::InvalidSamplerId });
    ResourceTableCommitChanges(fogState.resourceTables[bufferIndex]);

    // get per-view resource tables
    const Util::FixedArray<CoreGraphics::ResourceTableId>& viewTables = TransformDevice::Instance()->GetViewResourceTables();

    uint offset = SetComputeConstants(fogUniforms);
    ResourceTableSetConstantBuffer(viewTables[bufferIndex], { GetComputeConstantBuffer(), fogState.uniformsSlot, 0, false, false, sizeof(Volumefog::VolumeFogUniforms), (SizeT)offset });

    // setup blur tables
    ResourceTableSetTexture(blurState.blurXTable[bufferIndex], { fogState.fogVolumeTexture0, blurState.blurInputXSlot, 0, CoreGraphics::InvalidSamplerId, false }); // ping
    ResourceTableSetRWTexture(blurState.blurXTable[bufferIndex], { fogState.fogVolumeTexture1, blurState.blurOutputXSlot, 0, CoreGraphics::InvalidSamplerId }); // pong
    ResourceTableSetTexture(blurState.blurYTable[bufferIndex], { fogState.fogVolumeTexture1, blurState.blurInputYSlot, 0, CoreGraphics::InvalidSamplerId }); // ping
    ResourceTableSetRWTexture(blurState.blurYTable[bufferIndex], { fogState.fogVolumeTexture0, blurState.blurOutputYSlot, 0, CoreGraphics::InvalidSamplerId }); // pong
    ResourceTableCommitChanges(blurState.blurXTable[bufferIndex]);
    ResourceTableCommitChanges(blurState.blurYTable[bufferIndex]);
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::RenderUI(const Graphics::FrameContext& ctx)
{
    if (fogState.showUI)
    {
        float col[3];
        fogState.color.storeu(col);
        Shared::PerTickParams& tickParams = CoreGraphics::ShaderServer::Instance()->GetTickParams();
        if (ImGui::Begin("Volumetric Fog Params"))
        {
            ImGui::SetWindowSize(ImVec2(240, 400), ImGuiCond_Once);
            ImGui::SliderFloat("Turbidity", &fogState.turbidity, 0, 200.0f);
            ImGui::ColorEdit3("Fog Color", col);
        }
        fogState.color.loadu(col);

        ImGui::End();
    }
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetGlobalTurbidity(float f)
{
    fogState.turbidity = f;
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetGlobalAbsorption(const Math::vec3& color)
{
    fogState.color = color;
}

//------------------------------------------------------------------------------
/**
*/
Math::mat4 
VolumetricFogContext::GetTransform(const Graphics::GraphicsEntityId id)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    FogVolumeType type = fogGenericVolumeAllocator.Get<FogVolume_Type>(cid.id);
    Ids::Id32 tid = fogGenericVolumeAllocator.Get<FogVolume_TypedId>(cid.id);
    switch (type)
    {
    case BoxVolume:
        return fogBoxVolumeAllocator.Get<FogBoxVolume_Transform>(tid);
    case SphereVolume:
        return Math::translation(fogSphereVolumeAllocator.Get<FogSphereVolume_Position>(tid));
    };
    return Math::mat4();
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::SetTransform(const Graphics::GraphicsEntityId id, const Math::mat4& mat)
{
    const Graphics::ContextEntityId cid = GetContextId(id);
    FogVolumeType type = fogGenericVolumeAllocator.Get<FogVolume_Type>(cid.id);
    Ids::Id32 tid = fogGenericVolumeAllocator.Get<FogVolume_TypedId>(cid.id);
    switch (type)
    {
    case BoxVolume:
        return fogBoxVolumeAllocator.Set<FogBoxVolume_Transform>(tid, mat);
    case SphereVolume:
        return fogSphereVolumeAllocator.Set<FogSphereVolume_Position>(tid, xyz(mat.position));
    };
}

//------------------------------------------------------------------------------
/**
*/
Graphics::ContextEntityId 
VolumetricFogContext::Alloc()
{
    return fogGenericVolumeAllocator.Alloc();
}

//------------------------------------------------------------------------------
/**
*/
void 
VolumetricFogContext::Dealloc(Graphics::ContextEntityId id)
{
    fogGenericVolumeAllocator.Dealloc(id.id);
}

} // namespace Fog
