//------------------------------------------------------------------------------
//  ssrcontext.cc
//  (C) 2018 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "render/stdneb.h"
#include "frame/frameplugin.h"
#include "coregraphics/graphicsdevice.h"
#include "coregraphics/resourcetable.h"
#include "graphics/graphicsserver.h"
#include "graphics/cameracontext.h"
#include "graphics/view.h"
#include "ssrcontext.h"

#include "ssr_cs.h"

namespace PostEffects
{
__ImplementPluginContext(PostEffects::SSRContext);

struct
{
    CoreGraphics::ShaderId traceShader;
    Util::FixedArray<CoreGraphics::ResourceTableId> ssrTraceTables;
    IndexT traceBufferSlot;
    IndexT constantsSlot;

    CoreGraphics::ShaderId resolveShader;
    Util::FixedArray<CoreGraphics::ResourceTableId> ssrResolveTables;
    IndexT reflectionBufferSlot;
    IndexT lightBufferSlot;
    IndexT resolveTraceBufferSlot;

    CoreGraphics::ShaderProgramId traceProgram;
    CoreGraphics::ShaderProgramId resolveProgram;

    CoreGraphics::BufferId constants;

    CoreGraphics::TextureId traceBuffer;
    CoreGraphics::TextureId reflectionBuffer;
} ssrState;

//------------------------------------------------------------------------------
/**
*/
SSRContext::SSRContext()
{
}

//------------------------------------------------------------------------------
/**
*/
SSRContext::~SSRContext()
{
}

//------------------------------------------------------------------------------
/**
*/
void 
SSRContext::Create()
{
    __CreatePluginContext();
    __bundle.OnUpdateViewResources = SSRContext::UpdateViewDependentResources;
    Graphics::GraphicsServer::Instance()->RegisterGraphicsContext(&__bundle, &__state);

    // TODO: Convert to subgraph
    using namespace CoreGraphics;
    Frame::AddCallback("SSR-Trace", [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
        {
#if NEBULA_GRAPHICS_DEBUG
            CoreGraphics::CmdBeginMarker(cmdBuf, NEBULA_MARKER_BLUE, "Screen Space Reflections");
#endif
            TextureDimensions dims = TextureGetDimensions(ssrState.traceBuffer);

            CoreGraphics::CmdSetShaderProgram(cmdBuf, ssrState.traceProgram);
            CoreGraphics::CmdSetResourceTable(cmdBuf, ssrState.ssrTraceTables[bufferIndex], NEBULA_BATCH_GROUP, CoreGraphics::ComputePipeline, nullptr);

            const int TILE_SIZE = 32;
            int workGroups[2] = {
                (dims.width + (dims.width % TILE_SIZE)) / TILE_SIZE,
                (dims.height + (dims.height % TILE_SIZE)) / TILE_SIZE
            };
            CoreGraphics::CmdDispatch(cmdBuf, workGroups[0], workGroups[1], 1);

#if NEBULA_GRAPHICS_DEBUG
            CoreGraphics::CmdEndMarker(cmdBuf);
#endif
        });

    Frame::AddCallback("SSR-Resolve", [](const CoreGraphics::CmdBufferId cmdBuf, const IndexT frame, const IndexT bufferIndex)
        {
#if NEBULA_GRAPHICS_DEBUG
            CoreGraphics::CmdBeginMarker(cmdBuf, NEBULA_MARKER_BLUE, "Screen Space Reflections");
#endif
            TextureDimensions dims = TextureGetDimensions(ssrState.reflectionBuffer);

            CoreGraphics::CmdSetShaderProgram(cmdBuf, ssrState.resolveProgram);
            CoreGraphics::CmdSetResourceTable(cmdBuf, ssrState.ssrResolveTables[bufferIndex], NEBULA_BATCH_GROUP, CoreGraphics::ComputePipeline, nullptr);

            const int TILE_SIZE = 32;
            int workGroups[2] = {
                (dims.width + (dims.width % TILE_SIZE)) / TILE_SIZE,
                (dims.height + (dims.height % TILE_SIZE)) / TILE_SIZE
            };
            CoreGraphics::CmdDispatch(cmdBuf, workGroups[0], workGroups[1], 1);

#if NEBULA_GRAPHICS_DEBUG
            CoreGraphics::CmdEndMarker(cmdBuf);
#endif
        });
}

//------------------------------------------------------------------------------
/**
*/
void 
SSRContext::Discard()
{
    using namespace CoreGraphics;
    for (auto& table : ssrState.ssrTraceTables)
    {
        DestroyResourceTable(table);
        table = CoreGraphics::InvalidResourceTableId;
    }
}

//------------------------------------------------------------------------------
/**
*/
void 
SSRContext::Setup(const Ptr<Frame::FrameScript>& script)
{
    SizeT numFrames = CoreGraphics::GetNumBufferedFrames();
    using namespace CoreGraphics;
    ssrState.traceBuffer = script->GetTexture("SSRTraceBuffer");
    ssrState.reflectionBuffer = script->GetTexture("ReflectionBuffer");

    // create trace shader
    ssrState.traceShader = ShaderGet("shd:ssr_cs.fxb");
    ssrState.constants = CoreGraphics::GetComputeConstantBuffer();
    ssrState.constantsSlot = ShaderGetResourceSlot(ssrState.traceShader, "SSRBlock");
    ssrState.traceBufferSlot = ShaderGetResourceSlot(ssrState.traceShader, "TraceBuffer");

    ssrState.ssrTraceTables.SetSize(numFrames);
    for (IndexT i = 0; i < numFrames; ++i)
    {
        ssrState.ssrTraceTables[i] = ShaderCreateResourceTable(ssrState.traceShader, NEBULA_BATCH_GROUP, numFrames);
        ResourceTableSetRWTexture(ssrState.ssrTraceTables[i], { ssrState.traceBuffer, ssrState.traceBufferSlot, 0, InvalidSamplerId });
        ResourceTableCommitChanges(ssrState.ssrTraceTables[i]);
    }

    //create resolve shader
    ssrState.resolveShader = ShaderGet("shd:ssr_resolve_cs.fxb");
    ssrState.reflectionBufferSlot = ShaderGetResourceSlot(ssrState.resolveShader, "ReflectionBuffer");
    ssrState.lightBufferSlot = ShaderGetResourceSlot(ssrState.resolveShader, "LightBuffer");
    ssrState.resolveTraceBufferSlot = ShaderGetResourceSlot(ssrState.resolveShader, "TraceBuffer");

    ssrState.ssrResolveTables.SetSize(numFrames);
    for (IndexT i = 0; i < numFrames; ++i)
    {
        ssrState.ssrResolveTables[i] = ShaderCreateResourceTable(ssrState.resolveShader, NEBULA_BATCH_GROUP, numFrames);
        ResourceTableSetRWTexture(ssrState.ssrResolveTables[i], { ssrState.reflectionBuffer, ssrState.reflectionBufferSlot, 0, InvalidSamplerId });
        ResourceTableSetTexture(ssrState.ssrResolveTables[i], { ssrState.traceBuffer, ssrState.resolveTraceBufferSlot, 0, InvalidSamplerId });
        ResourceTableCommitChanges(ssrState.ssrResolveTables[i]);
    }

    // setup programs
    ssrState.traceProgram = ShaderGetProgram(ssrState.traceShader, ShaderFeatureFromString("Alt0"));
    ssrState.resolveProgram = ShaderGetProgram(ssrState.resolveShader, ShaderFeatureFromString("Alt0"));
}

//------------------------------------------------------------------------------
/**
*/
void 
SSRContext::UpdateViewDependentResources(const Ptr<Graphics::View>& view, const Graphics::FrameContext& ctx)
{
    using namespace Graphics;
    using namespace CoreGraphics;
    const CameraSettings& cameraSettings = CameraContext::GetSettings(view->GetCamera());
    TextureDimensions dims = TextureGetDimensions(ssrState.traceBuffer);

    float sx = (float)dims.width;
    float sy = (float)dims.height;

    const Math::mat4 scrScale = Math::mat4(
        Math::vec4(sx, 0.0f, 0.0f, 0.0f),
        Math::vec4(0.0f, sy, 0.0f, 0.0f),
        Math::vec4(0.0f, 0.0f, 1.0f, 0.0f),
        Math::vec4(sx, sy, 0.0f, 1.0f)
    );

    Math::mat4 conv = cameraSettings.GetProjTransform();
    conv.r[1] = -conv.r[1];

    Math::mat4 viewToTextureSpaceMatrix = conv * scrScale;

    SsrCs::SSRBlock ssrBlock;
    viewToTextureSpaceMatrix.store(ssrBlock.ViewToTextureSpace);
    uint ssrOffset = CoreGraphics::SetComputeConstants(ssrBlock);

    IndexT bufferIndex = CoreGraphics::GetBufferedFrameIndex();

    ResourceTableSetConstantBuffer(ssrState.ssrTraceTables[bufferIndex], { ssrState.constants, ssrState.constantsSlot, 0, false, false, sizeof(SsrCs::SSRBlock), (SizeT)ssrOffset });
    ResourceTableCommitChanges(ssrState.ssrTraceTables[bufferIndex]);
}

} // namespace PostEffects
