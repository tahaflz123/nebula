#pragma once
//------------------------------------------------------------------------------
/**
    Handles stream-loaded Vulkan textures.

    Handles alloc/dealloc object creation through the memory texture pool
    
    @copyright
    (C) 2016-2020 Individual contributors, see AUTHORS file
*/
//------------------------------------------------------------------------------
#include "core/refcounted.h"
#include "resources/resourcestreamcache.h"
#include "vktexture.h"
#include "vkmemory.h"

namespace Vulkan
{
class VkStreamTextureCache : public Resources::ResourceStreamCache
{
    __DeclareClass(VkStreamTextureCache);
public:
    /// constructor
    VkStreamTextureCache();
    /// destructor
    virtual ~VkStreamTextureCache();

private:
    /// load texture
    LoadStatus LoadFromStream(const Resources::ResourceId res, const Util::StringAtom& tag, const Ptr<IO::Stream>& stream, bool immediate = false) override;
    /// unload texture
    void Unload(const Resources::ResourceId id);

    /// stream mips
    void StreamMaxLOD(const Resources::ResourceId& id, const float lod, bool immediate) override;

    /// allocate object
    Resources::ResourceUnknownId AllocObject() override;
    /// deallocate object
    void DeallocObject(const Resources::ResourceUnknownId id) override;
};

} // namespace Vulkan
