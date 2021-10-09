/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d9_impl_device.hpp"
#include "d3d9_impl_type_convert.hpp"

auto reshade::d3d9::convert_format(api::format format, bool lockable) -> D3DFORMAT
{
	switch (format)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::format::unknown:
		break;
	case api::format::r1_unorm:
		return D3DFMT_A1;
	case api::format::l8_unorm:
		return D3DFMT_L8;
	case api::format::a8_unorm:
		return D3DFMT_A8;
	case api::format::r8_typeless:
	case api::format::r8_unorm:
	case api::format::r8_uint:
	case api::format::r8_sint:
	case api::format::r8_snorm:
		return D3DFMT_L8;
	case api::format::l8a8_unorm:
		return D3DFMT_A8L8;
	case api::format::r8g8_typeless:
	case api::format::r8g8_unorm:
	case api::format::r8g8_uint:
	case api::format::r8g8_snorm:
	case api::format::r8g8_sint:
		break; // Unsupported
	case api::format::r8g8b8a8_typeless:
	case api::format::r8g8b8a8_unorm:
	case api::format::r8g8b8a8_unorm_srgb:
		return D3DFMT_A8B8G8R8;
	case api::format::r8g8b8a8_uint:
	case api::format::r8g8b8a8_sint:
	case api::format::r8g8b8a8_snorm:
		break; // Unsupported
	case api::format::r8g8b8x8_typeless:
	case api::format::r8g8b8x8_unorm:
	case api::format::r8g8b8x8_unorm_srgb:
		return D3DFMT_X8B8G8R8;
	case api::format::b8g8r8a8_typeless:
	case api::format::b8g8r8a8_unorm:
	case api::format::b8g8r8a8_unorm_srgb:
		return D3DFMT_A8R8G8B8;
	case api::format::b8g8r8x8_typeless:
	case api::format::b8g8r8x8_unorm:
	case api::format::b8g8r8x8_unorm_srgb:
		return D3DFMT_X8R8G8B8;
	case api::format::r10g10b10a2_typeless:
	case api::format::r10g10b10a2_uint:
	case api::format::r10g10b10a2_unorm:
		return D3DFMT_A2B10G10R10;
	case api::format::r10g10b10a2_xr_bias:
		return D3DFMT_A2B10G10R10_XR_BIAS;
	case api::format::b10g10r10a2_typeless:
	case api::format::b10g10r10a2_uint:
	case api::format::b10g10r10a2_unorm:
		return D3DFMT_A2R10G10B10;
	case api::format::l16_unorm:
	case api::format::r16_uint:
	case api::format::r16_sint:
	case api::format::r16_unorm:
	case api::format::r16_snorm:
		return D3DFMT_L16;
	case api::format::r16_typeless:
	case api::format::r16_float:
		return D3DFMT_R16F;
	case api::format::l16a16_unorm:
		break; // Unsupported
	case api::format::r16g16_uint:
	case api::format::r16g16_sint:
	case api::format::r16g16_unorm:
	case api::format::r16g16_snorm:
		return D3DFMT_G16R16;
	case api::format::r16g16_typeless:
	case api::format::r16g16_float:
		return D3DFMT_G16R16F;
	case api::format::r16g16b16a16_uint:
	case api::format::r16g16b16a16_sint:
	case api::format::r16g16b16a16_unorm:
	case api::format::r16g16b16a16_snorm:
		return D3DFMT_A16B16G16R16;
	case api::format::r16g16b16a16_typeless:
	case api::format::r16g16b16a16_float:
		return D3DFMT_A16B16G16R16F;
	case api::format::r32_uint:
	case api::format::r32_sint:
		break; // Unsupported
	case api::format::r32_typeless:
	case api::format::r32_float:
		return D3DFMT_R32F;
	case api::format::r32g32_uint:
	case api::format::r32g32_sint:
		break; // Unsupported
	case api::format::r32g32_typeless:
	case api::format::r32g32_float:
		return D3DFMT_G32R32F;
	case api::format::r32g32b32a32_uint:
	case api::format::r32g32b32a32_sint:
		break; // Unsupported
	case api::format::r32g32b32a32_typeless:
	case api::format::r32g32b32a32_float:
		return D3DFMT_A32B32G32R32F;
	case api::format::r9g9b9e5:
	case api::format::r11g11b10_float:
		break; // Unsupported
	case api::format::b5g6r5_unorm:
		return D3DFMT_R5G6B5;
	case api::format::b5g5r5a1_unorm:
		return D3DFMT_A1R5G5B5;
	case api::format::b5g5r5x1_unorm:
		return D3DFMT_X1R5G5B5;
	case api::format::b4g4r4a4_unorm:
		return D3DFMT_A4R4G4B4;
	case api::format::s8_uint:
		return D3DFMT_S8_LOCKABLE;
	case api::format::d16_unorm:
		return lockable ? D3DFMT_D16_LOCKABLE : D3DFMT_D16;
	case api::format::d16_unorm_s8_uint:
		break; // Unsupported
	case api::format::r24_g8_typeless:
	case api::format::d24_unorm_s8_uint:
		return D3DFMT_D24S8;
	case api::format::r24_unorm_x8_uint:
	case api::format::x24_unorm_g8_uint:
		break; // Unsupported
	case api::format::d32_float:
		return lockable ? D3DFMT_D32F_LOCKABLE : D3DFMT_D32;
	case api::format::r32_g8_typeless:
	case api::format::r32_float_x8_uint:
	case api::format::x32_float_g8_uint:
	case api::format::d32_float_s8_uint:
		break; // Unsupported
	case api::format::bc1_typeless:
	case api::format::bc1_unorm:
	case api::format::bc1_unorm_srgb:
		return D3DFMT_DXT1;
	case api::format::bc2_typeless:
	case api::format::bc2_unorm:
	case api::format::bc2_unorm_srgb:
		return D3DFMT_DXT3;
	case api::format::bc3_typeless:
	case api::format::bc3_unorm:
	case api::format::bc3_unorm_srgb:
		return D3DFMT_DXT5;
	case api::format::bc4_typeless:
	case api::format::bc4_unorm:
	case api::format::bc4_snorm:
		return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '1'));
	case api::format::bc5_typeless:
	case api::format::bc5_unorm:
	case api::format::bc5_snorm:
		return static_cast<D3DFORMAT>(MAKEFOURCC('A', 'T', 'I', '2'));
	case api::format::r8g8_b8g8_unorm:
		return D3DFMT_G8R8_G8B8;
	case api::format::g8r8_g8b8_unorm:
		return D3DFMT_R8G8_B8G8;
	case api::format::intz:
		return static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
	}

	return D3DFMT_UNKNOWN;
}
auto reshade::d3d9::convert_format(D3DFORMAT d3d_format) -> api::format
{
	switch (static_cast<DWORD>(d3d_format))
	{
	default:
	case D3DFMT_UNKNOWN:
		return api::format::unknown;
	case D3DFMT_A1:
		return api::format::r1_unorm;
	case D3DFMT_L8:
		return api::format::l8_unorm;
	case D3DFMT_A8:
		return api::format::a8_unorm;
	case D3DFMT_A8B8G8R8:
		return api::format::r8g8b8a8_unorm;
	case D3DFMT_X8B8G8R8:
		return api::format::r8g8b8x8_unorm;
	case D3DFMT_A8R8G8B8:
		return api::format::b8g8r8a8_unorm;
	case D3DFMT_X8R8G8B8:
		return api::format::b8g8r8x8_unorm;
	case D3DFMT_A2B10G10R10:
		return api::format::r10g10b10a2_unorm;
	case D3DFMT_A2B10G10R10_XR_BIAS:
		return api::format::r10g10b10a2_xr_bias;
	case D3DFMT_A2R10G10B10:
		return api::format::b10g10r10a2_unorm;
	case D3DFMT_L16:
		return api::format::l16_unorm;
	case D3DFMT_R16F:
		return api::format::r16_float;
	case D3DFMT_G16R16F:
		return api::format::r16g16_float;
	case D3DFMT_G16R16:
		return api::format::r16g16_unorm;
	case D3DFMT_A16B16G16R16F:
		return api::format::r16g16b16a16_float;
	case D3DFMT_A16B16G16R16:
		return api::format::r16g16b16a16_unorm;
	case D3DFMT_R32F:
		return api::format::r32_float;
	case D3DFMT_G32R32F:
		return api::format::r32g32_float;
	case D3DFMT_A32B32G32R32F:
		return api::format::r32g32b32a32_float;
	case D3DFMT_R5G6B5:
		return api::format::b5g6r5_unorm;
	case D3DFMT_A1R5G5B5:
		return api::format::b5g5r5a1_unorm;
	case D3DFMT_X1R5G5B5:
		return api::format::b5g5r5x1_unorm;
	case D3DFMT_A4R4G4B4:
		return api::format::b4g4r4a4_unorm;
	case D3DFMT_S8_LOCKABLE:
		return api::format::s8_uint;
	case D3DFMT_D16:
		return api::format::d16_unorm;
	case D3DFMT_D24S8:
		return api::format::d24_unorm_s8_uint;
	case D3DFMT_D32:
		return api::format::d32_float;
	case D3DFMT_DXT1:
		return api::format::bc1_unorm;
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
		return api::format::bc2_unorm;
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return api::format::bc3_unorm;
	case MAKEFOURCC('A', 'T', 'I', '1'):
		return api::format::bc4_unorm;
	case MAKEFOURCC('A', 'T', 'I', '2'):
		return api::format::bc5_unorm;
	case D3DFMT_R8G8_B8G8:
		return api::format::g8r8_g8b8_unorm;
	case D3DFMT_G8R8_G8B8:
		return api::format::r8g8_b8g8_unorm;
	case D3DFMT_INDEX16:
		return api::format::r16_uint;
	case D3DFMT_INDEX32:
		return api::format::r32_uint;
	case MAKEFOURCC('I', 'N', 'T', 'Z'):
		return api::format::intz;
	}
}

void reshade::d3d9::convert_memory_heap_to_d3d_pool(api::memory_heap heap, D3DPOOL &d3d_pool)
{
	// Managed resources are special and already moved to device-accessible memory as needed, so do not change pool to an explicit one for those
	if (d3d_pool == D3DPOOL_MANAGED)
		return;

	switch (heap)
	{
	case api::memory_heap::unknown:
		d3d_pool = D3DPOOL_MANAGED;
		break;
	case api::memory_heap::gpu_only:
		d3d_pool = D3DPOOL_DEFAULT;
		break;
	case api::memory_heap::cpu_to_gpu:
	case api::memory_heap::gpu_to_cpu:
		d3d_pool = D3DPOOL_SYSTEMMEM;
		break;
	case api::memory_heap::cpu_only:
		d3d_pool = D3DPOOL_SCRATCH;
		break;
	}
}
void reshade::d3d9::convert_d3d_pool_to_memory_heap(D3DPOOL d3d_pool, api::memory_heap &heap)
{
	switch (d3d_pool)
	{
	case D3DPOOL_DEFAULT:
		heap = api::memory_heap::gpu_only;
		break;
	case D3DPOOL_MANAGED:
		heap = api::memory_heap::unknown;
		break;
	case D3DPOOL_SYSTEMMEM:
		heap = api::memory_heap::cpu_to_gpu;
		break;
	case D3DPOOL_SCRATCH:
		heap = api::memory_heap::cpu_only;
		break;
	}
}

void reshade::d3d9::convert_resource_usage_to_d3d_usage(api::resource_usage usage, DWORD &d3d_usage)
{
	// Copying textures is implemented using the rasterization pipeline (see 'device_impl::copy_resource' implementation), so needs render target usage
	// When the destination in 'IDirect3DDevice9::StretchRect' is a texture surface, it too has to have render target usage (see https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-stretchrect)
	if ((usage & (api::resource_usage::render_target | api::resource_usage::copy_dest | api::resource_usage::resolve_dest)) != api::resource_usage::undefined)
		d3d_usage |= D3DUSAGE_RENDERTARGET;
	else
		d3d_usage &= ~D3DUSAGE_RENDERTARGET;

	if ((usage & (api::resource_usage::depth_stencil)) != api::resource_usage::undefined)
		d3d_usage |= D3DUSAGE_DEPTHSTENCIL;
	else
		d3d_usage &= ~D3DUSAGE_DEPTHSTENCIL;

	// Unordered access is not supported in D3D9
	assert((usage & api::resource_usage::unordered_access) == api::resource_usage::undefined);
}
void reshade::d3d9::convert_d3d_usage_to_resource_usage(DWORD d3d_usage, api::resource_usage &usage)
{
	if (d3d_usage & D3DUSAGE_RENDERTARGET)
		usage |= api::resource_usage::render_target;
	if (d3d_usage & D3DUSAGE_DEPTHSTENCIL)
		usage |= api::resource_usage::depth_stencil;
}

void reshade::d3d9::convert_resource_desc(const api::resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels, const D3DCAPS9 &caps)
{
	assert(desc.type == api::resource_type::texture_3d);

	internal_desc.Width = desc.texture.width;
	internal_desc.Height = desc.texture.height;
	internal_desc.Depth = desc.texture.depth_or_layers;

	if (const D3DFORMAT format = convert_format(desc.texture.format);
		format != D3DFMT_UNKNOWN)
		internal_desc.Format = format;

	assert(desc.texture.samples == 1);

	if (internal_desc.Pool != D3DPOOL_MANAGED)
	{
		convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
		// Volume textures cannot have render target or depth-stencil usage, so do not call 'convert_resource_usage_to_d3d_usage'
		// See https://docs.microsoft.com/en-us/windows/win32/direct3d9/d3dusage

		if ((desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic && (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) != 0)
		{
			internal_desc.Usage |= D3DUSAGE_DYNAMIC;

			// Keep dynamic textures in the default pool
			if (desc.heap == api::memory_heap::cpu_to_gpu)
				internal_desc.Pool = D3DPOOL_DEFAULT;
		}
	}

	assert((desc.flags & api::resource_flags::generate_mipmaps) != api::resource_flags::generate_mipmaps);

	if (levels != nullptr)
		*levels = desc.texture.levels;
	else
		assert(desc.texture.levels == 1);
}
void reshade::d3d9::convert_resource_desc(const api::resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels, const D3DCAPS9 &caps)
{
	assert(desc.type == api::resource_type::surface || desc.type == api::resource_type::texture_2d);

	internal_desc.Width = desc.texture.width;
	internal_desc.Height = desc.texture.height;

	if (const D3DFORMAT format = convert_format(desc.texture.format);
		format != D3DFMT_UNKNOWN)
		internal_desc.Format = format;

	if (desc.texture.samples > 1)
		internal_desc.MultiSampleType = static_cast<D3DMULTISAMPLE_TYPE>(desc.texture.samples);
	else
		internal_desc.MultiSampleType = D3DMULTISAMPLE_NONE;

	if (internal_desc.Pool != D3DPOOL_MANAGED)
	{
		convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
		// System memory textures cannot have render target or depth-stencil usage
		if (desc.heap == api::memory_heap::gpu_only)
			convert_resource_usage_to_d3d_usage(desc.usage, internal_desc.Usage);

		if (desc.type == api::resource_type::texture_2d && (desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic && (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) != 0)
		{
			internal_desc.Usage |= D3DUSAGE_DYNAMIC;

			// Keep dynamic textures in the default pool
			if (desc.heap == api::memory_heap::cpu_to_gpu)
				internal_desc.Pool = D3DPOOL_DEFAULT;
		}
	}

	if ((desc.flags & api::resource_flags::cube_compatible) == api::resource_flags::cube_compatible)
	{
		assert(desc.texture.depth_or_layers == 6); // D3DRTYPE_CUBETEXTURE
	}
	else
	{
		assert(desc.texture.depth_or_layers == 1);
	}

	if ((desc.flags & api::resource_flags::generate_mipmaps) == api::resource_flags::generate_mipmaps)
	{
		assert(desc.type != api::resource_type::surface);

		internal_desc.Usage |= D3DUSAGE_AUTOGENMIPMAP;
		if (levels != nullptr)
			*levels = 0;
	}
	else
	{
		if (levels != nullptr)
		{
			assert(desc.type != api::resource_type::surface);
			*levels = desc.texture.levels;
		}
		else
		{
			assert(desc.texture.levels == 1);
		}
	}
}
void reshade::d3d9::convert_resource_desc(const api::resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::buffer);

	assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.buffer.size);

	assert((desc.usage & (api::resource_usage::vertex_buffer | api::resource_usage::index_buffer)) == api::resource_usage::index_buffer);

	if (internal_desc.Pool != D3DPOOL_MANAGED)
	{
		if (desc.heap == api::memory_heap::gpu_to_cpu)
		{
			internal_desc.Pool = D3DPOOL_DEFAULT;
			assert((internal_desc.Usage & D3DUSAGE_WRITEONLY) == 0);
		}
		else
		{
			convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
			if (desc.heap == api::memory_heap::gpu_only)
				internal_desc.Usage |= D3DUSAGE_WRITEONLY;
			else if (desc.heap == api::memory_heap::cpu_to_gpu)
				internal_desc.Usage |= D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC;
		}

		if ((desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic)
		{
			internal_desc.Usage |= D3DUSAGE_DYNAMIC;

			// Keep dynamic buffers in the default pool
			if (desc.heap == api::memory_heap::cpu_to_gpu)
				internal_desc.Pool = D3DPOOL_DEFAULT;
		}
	}
}
void reshade::d3d9::convert_resource_desc(const api::resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc)
{
	assert(desc.type == api::resource_type::buffer);

	assert(desc.buffer.size <= std::numeric_limits<UINT>::max());
	internal_desc.Size = static_cast<UINT>(desc.buffer.size);

	assert((desc.usage & (api::resource_usage::vertex_buffer | api::resource_usage::index_buffer)) == api::resource_usage::vertex_buffer);

	if (internal_desc.Pool != D3DPOOL_MANAGED)
	{
		if (desc.heap == api::memory_heap::gpu_to_cpu)
		{
			internal_desc.Pool = D3DPOOL_DEFAULT;
			assert((internal_desc.Usage & D3DUSAGE_WRITEONLY) == 0);
		}
		else
		{
			convert_memory_heap_to_d3d_pool(desc.heap, internal_desc.Pool);
			if (desc.heap == api::memory_heap::gpu_only)
				internal_desc.Usage |= D3DUSAGE_WRITEONLY;
			else if (desc.heap == api::memory_heap::cpu_to_gpu)
				internal_desc.Usage |= D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC;
		}

		if ((desc.flags & api::resource_flags::dynamic) == api::resource_flags::dynamic)
		{
			internal_desc.Usage |= D3DUSAGE_DYNAMIC;

			// Keep dynamic buffers in the default pool
			if (desc.heap == api::memory_heap::cpu_to_gpu)
				internal_desc.Pool = D3DPOOL_DEFAULT;
		}
	}
}
reshade::api::resource_desc reshade::d3d9::convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels)
{
	assert(internal_desc.Type == D3DRTYPE_VOLUME || internal_desc.Type == D3DRTYPE_VOLUMETEXTURE);

	api::resource_desc desc = {};
	desc.type = api::resource_type::texture_3d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = internal_desc.Height;
	assert(internal_desc.Depth <= std::numeric_limits<uint16_t>::max());
	desc.texture.depth_or_layers = static_cast<uint16_t>(internal_desc.Depth);
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	desc.texture.format = convert_format(internal_desc.Format);
	desc.texture.samples = 1;

	convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	if (internal_desc.Type == D3DRTYPE_VOLUMETEXTURE)
		desc.usage |= api::resource_usage::shader_resource;

	return desc;
}
reshade::api::resource_desc reshade::d3d9::convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels, const D3DCAPS9 &caps)
{
	assert(internal_desc.Type == D3DRTYPE_SURFACE || internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE);

	api::resource_desc desc = {};
	desc.type = (internal_desc.Type == D3DRTYPE_SURFACE) ? api::resource_type::surface : api::resource_type::texture_2d;
	desc.texture.width = internal_desc.Width;
	desc.texture.height = internal_desc.Height;
	desc.texture.depth_or_layers = internal_desc.Type == D3DRTYPE_CUBETEXTURE ? 6 : 1;
	assert(levels <= std::numeric_limits<uint16_t>::max());
	desc.texture.levels = static_cast<uint16_t>(levels);
	desc.texture.format = convert_format(internal_desc.Format);

	if (internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
		desc.texture.samples = static_cast<uint16_t>(internal_desc.MultiSampleType);
	else
		desc.texture.samples = 1;

	convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	if (levels == 1 && internal_desc.Type == D3DRTYPE_TEXTURE && (internal_desc.Usage & D3DUSAGE_DYNAMIC) != 0)
		desc.heap = api::memory_heap::cpu_to_gpu;

	convert_d3d_usage_to_resource_usage(internal_desc.Usage, desc.usage);
	if ((internal_desc.Type == D3DRTYPE_TEXTURE || internal_desc.Type == D3DRTYPE_CUBETEXTURE) && (internal_desc.Pool == D3DPOOL_DEFAULT || internal_desc.Pool == D3DPOOL_MANAGED || (internal_desc.Pool == D3DPOOL_SYSTEMMEM && (caps.DevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY) != 0)))
	{
		switch (static_cast<DWORD>(internal_desc.Format))
		{
		default: // Includes INTZ, RAWZ, DF16 and DF24
			desc.usage |= api::resource_usage::shader_resource;
			break;
		case D3DFMT_D16_LOCKABLE:
		case D3DFMT_D32:
		case D3DFMT_D15S1:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
		case D3DFMT_D24X4S4:
		case D3DFMT_D16:
		case D3DFMT_D32F_LOCKABLE:
		case D3DFMT_D24FS8:
		case D3DFMT_D32_LOCKABLE:
		case D3DFMT_S8_LOCKABLE:
			assert((internal_desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0);
			break;
		case MAKEFOURCC('R', 'E', 'S', 'Z'):
		case MAKEFOURCC('N', 'U', 'L', 'L'):
			break;
		}
	}

	// Copying is restricted by limitations of 'IDirect3DDevice9::StretchRect' (see https://docs.microsoft.com/windows/win32/api/d3d9helper/nf-d3d9helper-idirect3ddevice9-stretchrect)
	// or performing copy between two textures using rasterization pipeline (see 'device_impl::copy_resource' implementation)
	if (internal_desc.Pool == D3DPOOL_DEFAULT && (internal_desc.Type == D3DRTYPE_SURFACE || (caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES) != 0))
	{
		switch (static_cast<DWORD>(internal_desc.Format))
		{
		default:
			desc.usage |= api::resource_usage::copy_source;
			if (internal_desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
				desc.usage |= api::resource_usage::resolve_source;
			if ((internal_desc.Usage & D3DUSAGE_RENDERTARGET) != 0)
				desc.usage |= api::resource_usage::copy_dest | api::resource_usage::resolve_dest;
			break;
		case D3DFMT_DXT1:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
			// Stretching is not supported if either surface is in a compressed format
			break;
		case D3DFMT_D16_LOCKABLE:
		case D3DFMT_D32:
		case D3DFMT_D15S1:
		case D3DFMT_D24S8:
		case D3DFMT_D24X8:
		case D3DFMT_D24X4S4:
		case D3DFMT_D16:
		case D3DFMT_D32F_LOCKABLE:
		case D3DFMT_D24FS8:
		case D3DFMT_D32_LOCKABLE:
		case D3DFMT_S8_LOCKABLE:
			// Stretching depth stencil surfaces is extremly limited (does not support copying from surface to texture for example), so just do not allow it
			assert((internal_desc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0);
			break;
		case MAKEFOURCC('R', 'E', 'S', 'Z'):
			desc.usage |= api::resource_usage::resolve_source;
			break;
		case MAKEFOURCC('N', 'U', 'L', 'L'):
			// Special render target format that has no memory attached, so cannot be copied
			break;
		}
	}
	else if (internal_desc.Pool == D3DPOOL_SYSTEMMEM)
	{
		// Implemented via 'IDirect3DDevice9::GetRenderTargetData' and 'IDirect3DDevice9::UpdateSurface'
		desc.usage |= api::resource_usage::copy_source | api::resource_usage::copy_dest;
	}

	if (internal_desc.Type == D3DRTYPE_CUBETEXTURE)
		desc.flags |= api::resource_flags::cube_compatible;
	if ((internal_desc.Usage & D3DUSAGE_DYNAMIC) != 0)
		desc.flags |= api::resource_flags::dynamic;
	if ((internal_desc.Usage & D3DUSAGE_AUTOGENMIPMAP) != 0)
		desc.flags |= api::resource_flags::generate_mipmaps;

	return desc;
}
reshade::api::resource_desc reshade::d3d9::convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = internal_desc.Size;
	if (internal_desc.Pool == D3DPOOL_DEFAULT && (internal_desc.Usage & D3DUSAGE_WRITEONLY) == 0)
		desc.heap = api::memory_heap::gpu_to_cpu;
	else
		convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	desc.usage = api::resource_usage::index_buffer;

	if ((internal_desc.Usage & D3DUSAGE_DYNAMIC) != 0)
	{
		desc.heap = api::memory_heap::cpu_to_gpu;
		desc.flags |= api::resource_flags::dynamic;
	}

	return desc;
}
reshade::api::resource_desc reshade::d3d9::convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc)
{
	api::resource_desc desc = {};
	desc.type = api::resource_type::buffer;
	desc.buffer.size = internal_desc.Size;
	if (internal_desc.Pool == D3DPOOL_DEFAULT && (internal_desc.Usage & D3DUSAGE_WRITEONLY) == 0)
		desc.heap = api::memory_heap::gpu_to_cpu;
	else
		convert_d3d_pool_to_memory_heap(internal_desc.Pool, desc.heap);
	desc.usage = api::resource_usage::vertex_buffer;

	if ((internal_desc.Usage & D3DUSAGE_DYNAMIC) != 0)
	{
		desc.heap = api::memory_heap::cpu_to_gpu;
		desc.flags |= api::resource_flags::dynamic;
	}

	return desc;
}

void reshade::d3d9::convert_pipeline_desc(const api::pipeline_desc &desc, std::vector<D3DVERTEXELEMENT9> &internal_elements)
{
	assert(desc.type == api::pipeline_stage::all_graphics || desc.type == api::pipeline_stage::input_assembler);
	internal_elements.reserve(16 + 1);

	for (UINT i = 0; i < 16 && desc.graphics.input_layout[i].format != api::format::unknown; ++i)
	{
		auto &internal_element = internal_elements.emplace_back();
		const api::input_layout_element &element = desc.graphics.input_layout[i];

		assert(element.buffer_binding <= std::numeric_limits<WORD>::max());
		internal_element.Stream = static_cast<WORD>(element.buffer_binding);
		assert(element.offset <= std::numeric_limits<WORD>::max());
		internal_element.Offset = static_cast<WORD>(element.offset);

		switch (element.format)
		{
		default:
			assert(false);
			[[fallthrough]];
		case api::format::unknown:
			internal_element.Type = D3DDECLTYPE_UNUSED;
			break;
		case api::format::r8g8b8a8_uint:
			internal_element.Type = D3DDECLTYPE_UBYTE4;
			break;
		case api::format::r8g8b8a8_unorm:
			internal_element.Type = D3DDECLTYPE_UBYTE4N;
			break;
		case api::format::b8g8r8a8_unorm:
			internal_element.Type = D3DDECLTYPE_D3DCOLOR;
			break;
		case api::format::r10g10b10a2_uint:
			internal_element.Type = D3DDECLTYPE_UDEC3;
			break;
		case api::format::r10g10b10a2_unorm:
			internal_element.Type = D3DDECLTYPE_DEC3N;
			break;
		case api::format::r16g16_sint:
			internal_element.Type = D3DDECLTYPE_SHORT2;
			break;
		case api::format::r16g16_float:
			internal_element.Type = D3DDECLTYPE_FLOAT16_2;
			break;
		case api::format::r16g16_unorm:
			internal_element.Type = D3DDECLTYPE_USHORT2N;
			break;
		case api::format::r16g16_snorm:
			internal_element.Type = D3DDECLTYPE_SHORT2N;
			break;
		case api::format::r16g16b16a16_sint:
			internal_element.Type = D3DDECLTYPE_SHORT4;
			break;
		case api::format::r16g16b16a16_float:
			internal_element.Type = D3DDECLTYPE_FLOAT16_4;
			break;
		case api::format::r16g16b16a16_unorm:
			internal_element.Type = D3DDECLTYPE_USHORT4N;
			break;
		case api::format::r16g16b16a16_snorm:
			internal_element.Type = D3DDECLTYPE_SHORT4N;
			break;
		case api::format::r32_float:
			internal_element.Type = D3DDECLTYPE_FLOAT1;
			break;
		case api::format::r32g32_float:
			internal_element.Type = D3DDECLTYPE_FLOAT2;
			break;
		case api::format::r32g32b32_float:
			internal_element.Type = D3DDECLTYPE_FLOAT3;
			break;
		case api::format::r32g32b32a32_float:
			internal_element.Type = D3DDECLTYPE_FLOAT4;
			break;
		}

		if (strcmp(element.semantic, "POSITION") == 0)
			internal_element.Usage = D3DDECLUSAGE_POSITION;
		else if (strcmp(element.semantic, "BLENDWEIGHT") == 0)
			internal_element.Usage = D3DDECLUSAGE_BLENDWEIGHT;
		else if (strcmp(element.semantic, "BLENDINDICES") == 0)
			internal_element.Usage = D3DDECLUSAGE_BLENDINDICES;
		else if (strcmp(element.semantic, "NORMAL") == 0)
			internal_element.Usage = D3DDECLUSAGE_NORMAL;
		else if (strcmp(element.semantic, "PSIZE") == 0)
			internal_element.Usage = D3DDECLUSAGE_PSIZE;
		else if (strcmp(element.semantic, "TANGENT") == 0)
			internal_element.Usage = D3DDECLUSAGE_TANGENT;
		else if (strcmp(element.semantic, "BINORMAL") == 0)
			internal_element.Usage = D3DDECLUSAGE_BINORMAL;
		else if (strcmp(element.semantic, "TESSFACTOR") == 0)
			internal_element.Usage = D3DDECLUSAGE_TESSFACTOR;
		else if (strcmp(element.semantic, "POSITIONT") == 0)
			internal_element.Usage = D3DDECLUSAGE_POSITIONT;
		else if (strcmp(element.semantic, "COLOR") == 0)
			internal_element.Usage = D3DDECLUSAGE_COLOR;
		else if (strcmp(element.semantic, "FOG") == 0)
			internal_element.Usage = D3DDECLUSAGE_FOG;
		else if (strcmp(element.semantic, "DEPTH") == 0)
			internal_element.Usage = D3DDECLUSAGE_DEPTH;
		else if (strcmp(element.semantic, "SAMPLE") == 0)
			internal_element.Usage = D3DDECLUSAGE_SAMPLE;
		else
			internal_element.Usage = D3DDECLUSAGE_TEXCOORD;

		assert(element.semantic_index <= 256);
		internal_element.UsageIndex = static_cast<BYTE>(element.semantic_index);
	}

	internal_elements.push_back(D3DDECL_END());
}
reshade::api::pipeline_desc reshade::d3d9::convert_pipeline_desc(const D3DVERTEXELEMENT9 *elements)
{
	api::pipeline_desc desc = { api::pipeline_stage::input_assembler };

	for (UINT i = 0; i < 16 && elements != nullptr && elements->Stream != 0xFF; ++i, ++elements)
	{
		const auto &internal_element = *elements;
		api::input_layout_element &element = desc.graphics.input_layout[i];

		element.buffer_binding = internal_element.Stream;
		element.offset = internal_element.Offset;

		switch (internal_element.Type)
		{
		default:
			assert(false);
			[[fallthrough]];
		case D3DDECLTYPE_UNUSED:
			element.format = api::format::unknown;
			break;
		case D3DDECLTYPE_UBYTE4:
			element.format = api::format::r8g8b8a8_uint;
			break;
		case D3DDECLTYPE_UBYTE4N:
			element.format = api::format::r8g8b8a8_unorm;
			break;
		case D3DDECLTYPE_D3DCOLOR:
			element.format = api::format::b8g8r8a8_unorm;
			break;
		case D3DDECLTYPE_UDEC3:
			element.format = api::format::r10g10b10a2_uint;
			break;
		case D3DDECLTYPE_DEC3N:
			element.format = api::format::r10g10b10a2_unorm;
			break;
		case D3DDECLTYPE_SHORT2:
			element.format = api::format::r16g16_sint;
			break;
		case D3DDECLTYPE_FLOAT16_2:
			element.format = api::format::r16g16_float;
			break;
		case D3DDECLTYPE_USHORT2N:
			element.format = api::format::r16g16_unorm;
			break;
		case D3DDECLTYPE_SHORT2N:
			element.format = api::format::r16g16_snorm;
			break;
		case D3DDECLTYPE_SHORT4:
			element.format = api::format::r16g16b16a16_sint;
			break;
		case D3DDECLTYPE_FLOAT16_4:
			element.format = api::format::r16g16b16a16_float;
			break;
		case D3DDECLTYPE_USHORT4N:
			element.format = api::format::r16g16b16a16_unorm;
			break;
		case D3DDECLTYPE_SHORT4N:
			element.format = api::format::r16g16b16a16_snorm;
			break;
		case D3DDECLTYPE_FLOAT1:
			element.format = api::format::r32_float;
			break;
		case D3DDECLTYPE_FLOAT2:
			element.format = api::format::r32g32_float;
			break;
		case D3DDECLTYPE_FLOAT3:
			element.format = api::format::r32g32b32_float;
			break;
		case D3DDECLTYPE_FLOAT4:
			element.format = api::format::r32g32b32a32_float;
			break;
		}

		switch (internal_element.Usage)
		{
		case D3DDECLUSAGE_POSITION:
			element.semantic = "POSITION";
			break;
		case D3DDECLUSAGE_BLENDWEIGHT:
			element.semantic = "BLENDWEIGHT";
			break;
		case D3DDECLUSAGE_BLENDINDICES:
			element.semantic = "BLENDINDICES";
			break;
		case D3DDECLUSAGE_NORMAL:
			element.semantic = "NORMAL";
			break;
		case D3DDECLUSAGE_PSIZE:
			element.semantic = "PSIZE";
			break;
		case D3DDECLUSAGE_TANGENT:
			element.semantic = "TANGENT";
			break;
		case D3DDECLUSAGE_BINORMAL:
			element.semantic = "BINORMAL";
			break;
		case D3DDECLUSAGE_TESSFACTOR:
			element.semantic = "TESSFACTOR";
			break;
		case D3DDECLUSAGE_COLOR:
			element.semantic = "COLOR";
			break;
		case D3DDECLUSAGE_FOG:
			element.semantic = "FOG";
			break;
		case D3DDECLUSAGE_DEPTH:
			element.semantic = "DEPTH";
			break;
		case D3DDECLUSAGE_SAMPLE:
			element.semantic = "SAMPLE";
			break;
		case D3DDECLUSAGE_TEXCOORD:
			element.semantic = "TEXCOORD";
			break;
		}

		element.semantic_index = internal_element.UsageIndex;
	}

	return desc;
}

auto reshade::d3d9::convert_blend_op(D3DBLENDOP value) -> api::blend_op
{
	return static_cast<api::blend_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d9::convert_blend_op(api::blend_op value) -> D3DBLENDOP
{
	return static_cast<D3DBLENDOP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d9::convert_blend_factor(D3DBLEND value) -> api::blend_factor
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case D3DBLEND_ZERO:
		return api::blend_factor::zero;
	case D3DBLEND_ONE:
		return api::blend_factor::one;
	case D3DBLEND_SRCCOLOR:
		return api::blend_factor::src_color;
	case D3DBLEND_INVSRCCOLOR:
		return api::blend_factor::inv_src_color;
	case D3DBLEND_DESTCOLOR:
		return api::blend_factor::dst_color;
	case D3DBLEND_INVDESTCOLOR:
		return api::blend_factor::inv_dst_color;
	case D3DBLEND_SRCALPHA:
		return api::blend_factor::src_alpha;
	case D3DBLEND_INVSRCALPHA:
		return api::blend_factor::inv_src_alpha;
	case D3DBLEND_DESTALPHA:
		return api::blend_factor::dst_alpha;
	case D3DBLEND_INVDESTALPHA:
		return api::blend_factor::inv_dst_alpha;
	case D3DBLEND_BLENDFACTOR:
		return api::blend_factor::constant_color;
	case D3DBLEND_INVBLENDFACTOR:
		return api::blend_factor::inv_constant_color;
	case D3DBLEND_SRCALPHASAT:
		return api::blend_factor::src_alpha_sat;
	case D3DBLEND_SRCCOLOR2:
		return api::blend_factor::src1_color;
	case D3DBLEND_INVSRCCOLOR2:
		return api::blend_factor::inv_src1_color;
	}
}
auto reshade::d3d9::convert_blend_factor(api::blend_factor value) -> D3DBLEND
{
	switch (value)
	{
	default:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::zero:
		return D3DBLEND_ZERO;
	case api::blend_factor::one:
		return D3DBLEND_ONE;
	case api::blend_factor::src_color:
		return D3DBLEND_SRCCOLOR;
	case api::blend_factor::inv_src_color:
		return D3DBLEND_INVSRCCOLOR;
	case api::blend_factor::dst_color:
		return D3DBLEND_DESTCOLOR;
	case api::blend_factor::inv_dst_color:
		return D3DBLEND_INVDESTCOLOR;
	case api::blend_factor::src_alpha:
		return D3DBLEND_SRCALPHA;
	case api::blend_factor::inv_src_alpha:
		return D3DBLEND_INVSRCALPHA;
	case api::blend_factor::dst_alpha:
		return D3DBLEND_DESTALPHA;
	case api::blend_factor::inv_dst_alpha:
		return D3DBLEND_INVDESTALPHA;
	case api::blend_factor::constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::constant_color:
		return D3DBLEND_BLENDFACTOR;
	case api::blend_factor::inv_constant_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::inv_constant_color:
		return D3DBLEND_INVBLENDFACTOR;
	case api::blend_factor::src_alpha_sat:
		return D3DBLEND_SRCALPHASAT;
	case api::blend_factor::src1_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::src1_color:
		return D3DBLEND_SRCCOLOR2;
	case api::blend_factor::inv_src1_alpha:
		assert(false);
		[[fallthrough]];
	case api::blend_factor::inv_src1_color:
		return D3DBLEND_INVSRCCOLOR2;
	}
}
auto reshade::d3d9::convert_fill_mode(D3DFILLMODE value) -> api::fill_mode
{
	return static_cast<api::fill_mode>(D3DFILL_SOLID - value);
}
auto reshade::d3d9::convert_fill_mode(api::fill_mode value) -> D3DFILLMODE
{
	return static_cast<D3DFILLMODE>(D3DFILL_SOLID - static_cast<uint32_t>(value));
}
auto reshade::d3d9::convert_cull_mode(D3DCULL value, bool front_counter_clockwise) -> api::cull_mode
{
	if (value == D3DCULL_NONE)
		return api::cull_mode::none;
	return (value == D3DCULL_CCW && front_counter_clockwise) ? api::cull_mode::front : api::cull_mode::back;
}
auto reshade::d3d9::convert_cull_mode(api::cull_mode value, bool front_counter_clockwise) -> D3DCULL
{
	if (value == api::cull_mode::none)
		return D3DCULL_NONE;
	return (value == api::cull_mode::front) == front_counter_clockwise ? D3DCULL_CCW : D3DCULL_CW;
}
auto reshade::d3d9::convert_compare_op(D3DCMPFUNC value) -> api::compare_op
{
	return static_cast<api::compare_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d9::convert_compare_op(api::compare_op value) -> D3DCMPFUNC
{
	return static_cast<D3DCMPFUNC>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d9::convert_stencil_op(D3DSTENCILOP value) -> api::stencil_op
{
	return static_cast<api::stencil_op>(static_cast<uint32_t>(value) - 1);
}
auto reshade::d3d9::convert_stencil_op(api::stencil_op value) -> D3DSTENCILOP
{
	return static_cast<D3DSTENCILOP>(static_cast<uint32_t>(value) + 1);
}
auto reshade::d3d9::convert_query_type(api::query_type value) -> D3DQUERYTYPE
{
	switch (value)
	{
	case api::query_type::occlusion:
	case api::query_type::binary_occlusion:
		return D3DQUERYTYPE_OCCLUSION;
	case api::query_type::timestamp:
		return D3DQUERYTYPE_TIMESTAMP;
	default:
		assert(false);
		return static_cast<D3DQUERYTYPE>(0xFFFFFFFF);
	}
}

UINT reshade::d3d9::calc_vertex_from_prim_count(D3DPRIMITIVETYPE type, UINT count)
{
	switch (type)
	{
	default:
		return 0;
	case D3DPT_LINELIST:
		return count * 2;
	case D3DPT_LINESTRIP:
		return count + 1;
	case D3DPT_TRIANGLELIST:
		return count * 3;
	case D3DPT_TRIANGLESTRIP:
	case D3DPT_TRIANGLEFAN:
		return count + 2;
	}
}
UINT reshade::d3d9::calc_prim_from_vertex_count(D3DPRIMITIVETYPE type, UINT count)
{
	switch (type)
	{
	default:
		return 0;
	case D3DPT_LINELIST:
		return count / 2;
	case D3DPT_LINESTRIP:
		return count - 1;
	case D3DPT_TRIANGLELIST:
		return count / 3;
	case D3DPT_TRIANGLESTRIP:
	case D3DPT_TRIANGLEFAN:
		return count - 2;
	}
}
