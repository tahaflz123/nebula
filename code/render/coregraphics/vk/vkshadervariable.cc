//------------------------------------------------------------------------------
// vkshadervariable.cc
// (C) 2016 Individual contributors, see AUTHORS file
//------------------------------------------------------------------------------
#include "stdneb.h"
#include "vkshadervariable.h"
#include "vkshaderstate.h"
#include "coregraphics/constantbuffer.h"
#include "coregraphics/shaderreadwritetexture.h"
#include "coregraphics/shaderreadwritebuffer.h"
#include "coregraphics/texture.h"
#include "vktexture.h"
#include "vkconstantbuffer.h"
#include "vkshaderreadwritetexture.h"
#include "vkshaderreadwritebuffer.h"
#include "vkshaderstate.h"


using namespace CoreGraphics;
namespace Vulkan
{

//------------------------------------------------------------------------------
/**
*/
void
VkShaderVariableBindToUniformBuffer(const CoreGraphics::ShaderVariableId var, CoreGraphics::ConstantBufferId buffer, VkShaderVariableAllocator& allocator, uint32_t offset, uint32_t size, int8_t* defaultValue)
{
	VkShaderVariableVariableBinding& binding = allocator.Get<0>(var.id);
	binding.backing.uniformBuffer = buffer.id24;
	binding.offset = offset;
	binding.size = size;
	binding.defaultValue = defaultValue;
	binding.isvalid = true;
	binding.isbuffer = true;

	// make sure that the buffer is updated (data is array since we have a char*)
	ConstantBufferUpdate(buffer, defaultValue, offset, size);
}

//------------------------------------------------------------------------------
/**
*/
void
VkShaderVariableBindToPushConstantRange(const CoreGraphics::ShaderVariableId var, uint8_t* buffer, VkShaderVariableAllocator& allocator, uint32_t offset, uint32_t size, int8_t* defaultValue)
{
	VkShaderVariableVariableBinding& binding = allocator.Get<0>(var.id);
	binding.backing.push = buffer;
	binding.offset = offset;
	binding.size = size;
	binding.defaultValue = defaultValue;
	binding.isvalid = true;
	binding.isbuffer = false;
	
	// copy data to buffer
	VkShaderVariableUpdatePushRange(buffer + offset, size, defaultValue);
}

//------------------------------------------------------------------------------
/**
*/
uint32_t
VkShaderVariableGetBinding(const CoreGraphics::ShaderVariableId var, VkShaderVariableAllocator& allocator)
{
	const VkShaderVariableResourceBinding& binding = allocator.Get<1>(var.id);
	return binding.setBinding;
}

//------------------------------------------------------------------------------
/**
*/
VkDescriptorSet
VkShaderVariableGetDescriptorSet(const CoreGraphics::ShaderVariableId var, VkShaderVariableAllocator& allocator)
{
	const VkShaderVariableResourceBinding& binding = allocator.Get<1>(var.id);
	return binding.set;
}

//------------------------------------------------------------------------------
/**
*/
void
VkShaderVariableSetup(AnyFX::VkVariable* var, Ids::Id24 id, VkShaderVariableAllocator& allocator, const VkDescriptorSet set)
{
	n_assert(0 != var);

	Util::String name = var->name.c_str();

	VkShaderVariableVariableBinding& varBind = allocator.Get<0>(id);
	VkShaderVariableResourceBinding& resBind = allocator.Get<1>(id);
	VkShaderVariableSetupInfo& setupInfo = allocator.Get<2>(id);
	
	setupInfo.name = name;
	resBind.set = set;
	switch (var->type)
	{
	case AnyFX::Double:
	case AnyFX::Float:
		setupInfo.type = FloatVariableType;
		break;
	case AnyFX::Short:
	case AnyFX::Integer:
	case AnyFX::UInteger:
		setupInfo.type = IntVariableType;
		break;
	case AnyFX::Bool:
		setupInfo.type = BoolVariableType;
		break;
	case AnyFX::Float3:
	case AnyFX::Float4:
	case AnyFX::Double3:
	case AnyFX::Double4:
	case AnyFX::Integer3:
	case AnyFX::Integer4:
	case AnyFX::UInteger3:
	case AnyFX::UInteger4:
	case AnyFX::Short3:
	case AnyFX::Short4:
	case AnyFX::Bool3:
	case AnyFX::Bool4:
		setupInfo.type = VectorVariableType;
		break;
	case AnyFX::Float2:
	case AnyFX::Double2:
	case AnyFX::Integer2:
	case AnyFX::UInteger2:
	case AnyFX::Short2:
	case AnyFX::Bool2:
		setupInfo.type = Vector2VariableType;
		break;
	case AnyFX::Matrix2x2:
	case AnyFX::Matrix2x3:
	case AnyFX::Matrix2x4:
	case AnyFX::Matrix3x2:
	case AnyFX::Matrix3x3:
	case AnyFX::Matrix3x4:
	case AnyFX::Matrix4x2:
	case AnyFX::Matrix4x3:
	case AnyFX::Matrix4x4:
		setupInfo.type = MatrixVariableType;
		break;
	case AnyFX::Image1D:
	case AnyFX::Image1DArray:
	case AnyFX::Image2D:
	case AnyFX::Image2DArray:
	case AnyFX::Image2DMS:
	case AnyFX::Image2DMSArray:
	case AnyFX::Image3D:
	case AnyFX::ImageCube:
	case AnyFX::ImageCubeArray:
		setupInfo.type = ImageReadWriteVariableType;
		n_assert(set != VK_NULL_HANDLE);
		resBind.setBinding = var->bindingLayout.binding;
		break;
	case AnyFX::Sampler1D:
	case AnyFX::Sampler1DArray:
	case AnyFX::Sampler2D:
	case AnyFX::Sampler2DArray:
	case AnyFX::Sampler2DMS:
	case AnyFX::Sampler2DMSArray:
	case AnyFX::Sampler3D:
	case AnyFX::SamplerCube:
	case AnyFX::SamplerCubeArray:
		setupInfo.type = SamplerVariableType;
		n_assert(set != VK_NULL_HANDLE);
		resBind.setBinding = var->bindingLayout.binding;
		break;
	case AnyFX::Texture1D:
	case AnyFX::Texture1DArray:
	case AnyFX::Texture2D:
	case AnyFX::Texture2DArray:
	case AnyFX::Texture2DMS:
	case AnyFX::Texture2DMSArray:
	case AnyFX::Texture3D:
	case AnyFX::TextureCube:
	case AnyFX::TextureCubeArray:
		setupInfo.type = TextureVariableType;
		n_assert(set != VK_NULL_HANDLE);
		resBind.setBinding = var->bindingLayout.binding;
		break;
	case AnyFX::TextureHandle:
		setupInfo.type = TextureVariableType;
		resBind.textureIsHandle = true;
		break;
	case AnyFX::ImageHandle:
		setupInfo.type = ImageReadWriteVariableType;
		break;
	case AnyFX::SamplerHandle:
		setupInfo.type = SamplerVariableType;
		resBind.textureIsHandle = true;
		break;
	default:
		setupInfo.type = ConstantBufferVariableType;
		n_assert(set != VK_NULL_HANDLE);
		resBind.setBinding = var->bindingLayout.binding;
		break;
	}
}

//------------------------------------------------------------------------------
/**
*/
void
VkShaderVariableSetup(AnyFX::VkVarbuffer* var, Ids::Id24 id, VkShaderVariableAllocator& allocator, const VkDescriptorSet set)
{
	n_assert(0 != var);
	Util::String name = var->name.c_str();
	VkShaderVariableVariableBinding& varBind = allocator.Get<0>(id);
	VkShaderVariableResourceBinding& resBind = allocator.Get<1>(id);
	VkShaderVariableSetupInfo& setupInfo = allocator.Get<2>(id);

	setupInfo.name = name;
	setupInfo.type = BufferReadWriteVariableType;
	resBind.dynamicOffset = var->Flag("DynamicOffset");
	resBind.setBinding = var->bindingLayout.binding;
	resBind.set = set;
}

//------------------------------------------------------------------------------
/**
*/
void
VkShaderVariableSetup(AnyFX::VkVarblock* var, Ids::Id24 id, VkShaderVariableAllocator& allocator, const VkDescriptorSet set)
{
	n_assert(0 != var);
	Util::String name = var->name.c_str();
	VkShaderVariableVariableBinding& varBind = allocator.Get<0>(id);
	VkShaderVariableResourceBinding& resBind = allocator.Get<1>(id);
	VkShaderVariableSetupInfo& setupInfo = allocator.Get<2>(id);
	
	setupInfo.name = name;
	setupInfo.type = ConstantBufferVariableType;
	resBind.dynamicOffset = var->Flag("DynamicOffset");
	resBind.setBinding = var->bindingLayout.binding;
	resBind.set = set;
}

//------------------------------------------------------------------------------
/**
*/
void
SetInt(VkShaderVariableVariableBinding& bind, int value)
{
	if (bind.isbuffer)
		ConstantBufferUpdate(bind.backing.uniformBuffer, &value, bind.offset, sizeof(int));
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(int), value);
}

//------------------------------------------------------------------------------
/**
*/
void
SetIntArray(VkShaderVariableVariableBinding& bind, const int* values, SizeT count)
{
	if (bind.isbuffer)
		ConstantBufferArrayUpdate(bind.backing.uniformBuffer, values, bind.offset, sizeof(int), count);
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(int) * count, values);
}

//------------------------------------------------------------------------------
/**
*/
void
SetFloat(VkShaderVariableVariableBinding& bind, float value)
{
	if (bind.isbuffer)
		ConstantBufferUpdate(bind.backing.uniformBuffer, &value, bind.offset, sizeof(float));
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(float), value);
}

//------------------------------------------------------------------------------
/**
*/
void
SetFloatArray(VkShaderVariableVariableBinding& bind, const float* values, SizeT count)
{
	if (bind.isbuffer)
		ConstantBufferArrayUpdate(bind.backing.uniformBuffer, values, bind.offset, sizeof(float), count);
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(float) * count, values);
}

//------------------------------------------------------------------------------
/**
*/
void
SetFloat2(VkShaderVariableVariableBinding& bind, const Math::float2& value)
{
	if (bind.isbuffer)
		ConstantBufferUpdate(bind.backing.uniformBuffer, &value, bind.offset, sizeof(Math::float2));
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(Math::float2), value);
}

//------------------------------------------------------------------------------
/**
*/
void
SetFloat2Array(VkShaderVariableVariableBinding& bind, const Math::float2* values, SizeT count)
{
	if (bind.isbuffer)
		ConstantBufferArrayUpdate(bind.backing.uniformBuffer, values, bind.offset, sizeof(Math::float2), count);
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(Math::float2) * count, values);
}

//------------------------------------------------------------------------------
/**
*/
void
SetFloat4(VkShaderVariableVariableBinding& bind, const Math::float4& value)
{
	if (bind.isbuffer)
		ConstantBufferUpdate(bind.backing.uniformBuffer, &value, bind.offset, sizeof(Math::float4));
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(Math::float4), value);
}

//------------------------------------------------------------------------------
/**
*/
void
SetFloat4Array(VkShaderVariableVariableBinding& bind, const Math::float4* values, SizeT count)
{
	if (bind.isbuffer)
		ConstantBufferArrayUpdate(bind.backing.uniformBuffer, values, bind.offset, sizeof(Math::float4), count);
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(Math::float4) * count, values);
}

//------------------------------------------------------------------------------
/**
*/
void
SetMatrix(VkShaderVariableVariableBinding& bind, const Math::matrix44& value)
{
	if (bind.isbuffer)
		ConstantBufferUpdate(bind.backing.uniformBuffer, &value, bind.offset, sizeof(Math::matrix44));
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(Math::matrix44), value);
}

//------------------------------------------------------------------------------
/**
*/
void
SetMatrixArray(VkShaderVariableVariableBinding& bind, const Math::matrix44* values, SizeT count)
{
	if (bind.isbuffer)
		ConstantBufferArrayUpdate(bind.backing.uniformBuffer, values, bind.offset, sizeof(Math::matrix44), count);
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(Math::matrix44) * count, values);
}

//------------------------------------------------------------------------------
/**
*/
void
SetBool(VkShaderVariableVariableBinding& bind, bool value)
{
	if (bind.isbuffer)
		ConstantBufferUpdate(bind.backing.uniformBuffer, &value, bind.offset, sizeof(bool));
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(bool), value);
}

//------------------------------------------------------------------------------
/**
*/
void
SetBoolArray(VkShaderVariableVariableBinding& bind, const bool* values, SizeT count)
{
	if (bind.isbuffer)
		ConstantBufferArrayUpdate(bind.backing.uniformBuffer, values, bind.offset, sizeof(bool), count);
	else
		VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(bool) * count, values);
}

//------------------------------------------------------------------------------
/**
*/
void
SetTexture(VkShaderVariableVariableBinding& bind, VkShaderVariableResourceBinding& res, Util::Array<VkWriteDescriptorSet>& writes, const CoreGraphics::TextureId tex)
{
	VkTextureRuntimeInfo& info = textureAllocator.GetSafe<0>(tex.allocId);

	// only change if there is a difference
	if (info.view != res.write.img.imageView)
	{
		res.write.img.imageView = info.view;

		// if image can be set as an integer, do it
		if (res.textureIsHandle)
		{
			// update texture id
			if (bind.isbuffer)
				ConstantBufferUpdate(bind.backing.uniformBuffer, &info.bind, bind.offset, sizeof(uint32_t));
			else
				VkShaderVariableUpdatePushRange(bind.backing.push, sizeof(uint32_t), info.bind);
		}
		else
		{
			// dependent on type of variable, select if sampler should be coupled with sampler or if it will be assembled in-shader
			const VkImageView& view = info.view;
			VkDescriptorType type;
			if (info.type == TextureVariableType)		type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			else if (info.type == SamplerVariableType)	type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			else
			{
				n_error("Variable '%d' is must be either Texture or Integer to be assigned a texture!\n", res.setBinding);
			}

			VkWriteDescriptorSet set;
			set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			set.pNext = NULL;
			set.descriptorCount = 1;
			set.descriptorType = type;
			set.dstArrayElement = 0;
			set.dstBinding = res.setBinding;
			set.dstSet = res.set;
			set.pBufferInfo = NULL;
			set.pTexelBufferView = NULL;
			set.pImageInfo = &res.write.img;
			res.write.img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			res.write.img.sampler = VK_NULL_HANDLE;
			res.write.img.imageView = info.view;

			// add to shader to update on next update
			writes.Append(set);
		}
	}	
}

//------------------------------------------------------------------------------
/**
*/
void
SetConstantBuffer(VkShaderVariableResourceBinding& bind, Util::Array<VkWriteDescriptorSet>& writes, const CoreGraphics::ConstantBufferId buf)
{
	VkConstantBufferRuntimeInfo& info = constantBufferAllocator.Get<0>(buf.id24);
	if (info.buf != bind.write.buf.buffer)
	{
		VkWriteDescriptorSet set;
		set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		set.pNext = NULL;
		set.descriptorCount = 1;
		if (bind.dynamicOffset)		set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		else						set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		set.dstArrayElement = 0;
		set.dstBinding = bind.setBinding;
		set.dstSet = bind.set;
		set.pBufferInfo = &bind.write.buf;
		set.pTexelBufferView = NULL;
		set.pImageInfo = NULL;
		bind.write.buf.buffer = info.buf;
		bind.write.buf.offset = 0;
		bind.write.buf.range = VK_WHOLE_SIZE;

		// add to shader to update on next update
		writes.Append(set);
	}
}

//------------------------------------------------------------------------------
/**
*/
void
SetShaderReadWriteTexture(VkShaderVariableResourceBinding& bind, Util::Array<VkWriteDescriptorSet>& writes, const CoreGraphics::ShaderRWTextureId tex)
{
	VkShaderRWTextureRuntimeInfo& info = shaderRWTextureAllocator.Get<1>(tex.id24);
	if (info.view != bind.write.img.imageView)
	{
		VkWriteDescriptorSet set;
		set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		set.pNext = NULL;

		set.descriptorCount = 1;
		set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		set.dstArrayElement = 0;
		set.dstBinding = bind.setBinding;
		set.dstSet = bind.set;
		set.pBufferInfo = NULL;
		set.pTexelBufferView = NULL;
		set.pImageInfo = &bind.write.img;
		bind.write.img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		bind.write.img.imageView = info.view;
		bind.write.img.sampler = VK_NULL_HANDLE;

		// add to shader to update on next update
		writes.Append(set);
	}
}

//------------------------------------------------------------------------------
/**
*/
void
SetShaderReadWriteTexture(VkShaderVariableResourceBinding& bind, Util::Array<VkWriteDescriptorSet>& writes, const CoreGraphics::TextureId tex)
{
	VkTextureRuntimeInfo& info = textureAllocator.GetSafe<0>(tex.allocId);
	if (info.view != bind.write.img.imageView)
	{
		VkWriteDescriptorSet set;
		set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		set.pNext = NULL;

		set.descriptorCount = 1;
		set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		set.dstArrayElement = 0;
		set.dstBinding = bind.setBinding;
		set.dstSet = bind.set;
		set.pBufferInfo = NULL;
		set.pTexelBufferView = NULL;
		set.pImageInfo = &bind.write.img;
		bind.write.img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		bind.write.img.imageView = info.view;
		bind.write.img.sampler = VK_NULL_HANDLE;

		// add to shader to update on next update
		writes.Append(set);
	}
}

//------------------------------------------------------------------------------
/**
*/
void
SetShaderReadWriteBuffer(VkShaderVariableResourceBinding& bind, Util::Array<VkWriteDescriptorSet>& writes, const CoreGraphics::ShaderRWBufferId buf)
{
	VkShaderRWBufferRuntimeInfo& info = shaderRWBufferAllocator.Get<1>(buf.id24);
	if (info.buf != bind.write.buf.buffer)
	{
		VkWriteDescriptorSet set;
		set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		set.pNext = NULL;

		set.descriptorCount = 1;
		if (bind.dynamicOffset)		set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		else						set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		set.dstArrayElement = 0;
		set.dstBinding = bind.setBinding;
		set.dstSet = bind.set;
		set.pBufferInfo = &bind.write.buf;
		set.pTexelBufferView = NULL;
		set.pImageInfo = NULL;

		bind.write.buf.buffer = info.buf;
		bind.write.buf.offset = 0;
		bind.write.buf.range = VK_WHOLE_SIZE;

		// add to shader to update on next update
		writes.Append(set);
	}
}

} // namespace Vulkan
