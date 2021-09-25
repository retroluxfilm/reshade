/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d10_device.hpp"
#include "dxgi/dxgi_device.hpp"
#include "d3d10_impl_type_convert.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "com_utils.hpp"

D3D10Device::D3D10Device(IDXGIDevice1 *dxgi_device, ID3D10Device1 *original) :
	device_impl(original),
	_dxgi_device(new DXGIDevice(dxgi_device, this))
{
	assert(_orig != nullptr);

	// Add proxy object to the private data of the device, so that it can be retrieved again when only the original device is available
	D3D10Device *const device_proxy = this;
	_orig->SetPrivateData(__uuidof(D3D10Device), sizeof(device_proxy), &device_proxy);
}

bool D3D10Device::check_and_upgrade_interface(REFIID riid)
{
	if (riid == __uuidof(this) ||
		// IUnknown is handled by DXGIDevice
		riid == __uuidof(ID3D10Device) ||
		riid == __uuidof(ID3D10Device1))
		return true;

	return false;
}

#if RESHADE_ADDON
void D3D10Device::invoke_bind_vertex_buffers_event(UINT first, UINT count, ID3D10Buffer *const *buffers, const UINT *strides, const UINT *offsets)
{
	assert(count <= D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);

	if (!reshade::has_addon_event<reshade::addon_event::bind_vertex_buffers>())
		return;

#ifndef WIN64
	reshade::api::resource buffer_handles[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		buffer_handles[i] = { reinterpret_cast<uintptr_t>(buffers[i]) };
#else
	static_assert(sizeof(*buffers) == sizeof(reshade::api::resource));
	const auto buffer_handles = reinterpret_cast<const reshade::api::resource *>(buffers);
#endif

	uint64_t offsets_64[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		offsets_64[i] = offsets[i];

	reshade::invoke_addon_event<reshade::addon_event::bind_vertex_buffers>(this, first, count, buffer_handles, offsets_64, strides);
}
void D3D10Device::invoke_bind_samplers_event(reshade::api::shader_stage stage, UINT first, UINT count, ID3D10SamplerState *const *objects)
{
	assert(count <= D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT);

	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

#ifndef WIN64
	reshade::api::sampler descriptors[D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		descriptors[i] = { reinterpret_cast<uintptr_t>(objects[i]) };
#else
	static_assert(sizeof(*objects) == sizeof(reshade::api::sampler));
	const auto descriptors = reinterpret_cast<const reshade::api::sampler *>(objects);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, stage, _global_pipeline_layout, 0,
		reshade::api::descriptor_set_update(first, count, reshade::api::descriptor_type::sampler, descriptors));
}
void D3D10Device::invoke_bind_shader_resource_views_event(reshade::api::shader_stage stage, UINT first, UINT count, ID3D10ShaderResourceView *const *objects)
{
	assert(count <= D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

#ifndef WIN64
	reshade::api::resource_view descriptors[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
		descriptors[i] = { reinterpret_cast<uintptr_t>(objects[i]) };
#else
	static_assert(sizeof(*objects) == sizeof(reshade::api::resource_view));
	const auto descriptors = reinterpret_cast<const reshade::api::resource_view *>(objects);
#endif

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, stage, _global_pipeline_layout, 1,
		reshade::api::descriptor_set_update(first, count, reshade::api::descriptor_type::shader_resource_view, descriptors));
}
void D3D10Device::invoke_bind_constant_buffers_event(reshade::api::shader_stage stage, UINT first, UINT count, ID3D10Buffer *const *objects)
{
	assert(count <= D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);

	if (!reshade::has_addon_event<reshade::addon_event::push_descriptors>())
		return;

	reshade::api::buffer_range descriptors[D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
	for (UINT i = 0; i < count; ++i)
	{
		descriptors[i] = {
			reshade::api::resource { reinterpret_cast<uintptr_t>(objects[i]) },
			0,
			std::numeric_limits<uint64_t>::max() };
	}

	reshade::invoke_addon_event<reshade::addon_event::push_descriptors>(this, stage, _global_pipeline_layout, 2,
		reshade::api::descriptor_set_update(first, count, reshade::api::descriptor_type::constant_buffer, descriptors));
}
#endif

HRESULT STDMETHODCALLTYPE D3D10Device::QueryInterface(REFIID riid, void **ppvObj)
{
	if (ppvObj == nullptr)
		return E_POINTER;

	if (check_and_upgrade_interface(riid))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}

	// Note: Objects must have an identity, so use DXGIDevice for IID_IUnknown
	// See https://docs.microsoft.com/windows/desktop/com/rules-for-implementing-queryinterface
	if (_dxgi_device->check_and_upgrade_interface(riid))
	{
		_dxgi_device->AddRef();
		*ppvObj = _dxgi_device;
		return S_OK;
	}

	return _orig->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D10Device::AddRef()
{
	_orig->AddRef();

	// Add references to other objects that are coupled with the device
	_dxgi_device->AddRef();

	return InterlockedIncrement(&_ref);
}
ULONG   STDMETHODCALLTYPE D3D10Device::Release()
{
	// Release references to other objects that are coupled with the device
	_dxgi_device->Release();

	const ULONG ref = InterlockedDecrement(&_ref);
	if (ref != 0)
		return _orig->Release(), ref;

	const auto orig = _orig;
#if RESHADE_VERBOSE_LOG
	LOG(DEBUG) << "Destroying " << "ID3D10Device1" << " object " << this << " (" << orig << ").";
#endif
	delete this;

	const ULONG ref_orig = orig->Release();
	if (ref_orig != 0) // Verify internal reference count
		LOG(WARN) << "Reference count for " << "ID3D10Device1" << " object " << this << " (" << orig << ") is inconsistent (" << ref_orig << ").";
	return 0;
}

void    STDMETHODCALLTYPE D3D10Device::VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
#if RESHADE_ADDON
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::vertex, StartSlot, NumBuffers, ppConstantBuffers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
#if RESHADE_ADDON
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::pixel, StartSlot, NumViews, ppShaderResourceViews);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetShader(ID3D10PixelShader *pPixelShader)
{
	_orig->PSSetShader(pPixelShader);
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::pixel_shader, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pPixelShader) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
#if RESHADE_ADDON
	invoke_bind_samplers_event(reshade::api::shader_stage::pixel, StartSlot, NumSamplers, ppSamplers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShader(ID3D10VertexShader *pVertexShader)
{
	_orig->VSSetShader(pVertexShader);
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::vertex_shader, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pVertexShader) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0))
		return;
#endif
	_orig->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}
void    STDMETHODCALLTYPE D3D10Device::Draw(UINT VertexCount, UINT StartVertexLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, VertexCount, 1, StartVertexLocation, 0))
		return;
#endif
	_orig->Draw(VertexCount, StartVertexLocation);
}
void    STDMETHODCALLTYPE D3D10Device::PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
#if RESHADE_ADDON
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::pixel, StartSlot, NumBuffers, ppConstantBuffers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetInputLayout(ID3D10InputLayout *pInputLayout)
{
	_orig->IASetInputLayout(pInputLayout);
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::input_assembler, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pInputLayout) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets)
{
	_orig->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
#if RESHADE_ADDON
	invoke_bind_vertex_buffers_event(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetIndexBuffer(ID3D10Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset)
{
	_orig->IASetIndexBuffer(pIndexBuffer, Format, Offset);
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_index_buffer>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pIndexBuffer) }, Offset, Format == DXGI_FORMAT_R16_UINT ? 2 : 4);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw_indexed>(this, IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation))
		return;
#endif
	_orig->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void    STDMETHODCALLTYPE D3D10Device::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation))
		return;
#endif
	_orig->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer *const *ppConstantBuffers)
{
	_orig->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
#if RESHADE_ADDON
	invoke_bind_constant_buffers_event(reshade::api::shader_stage::geometry, StartSlot, NumBuffers, ppConstantBuffers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShader(ID3D10GeometryShader *pShader)
{
	_orig->GSSetShader(pShader);
#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this, reshade::api::pipeline_stage::geometry_shader, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pShader) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY Topology)
{
	_orig->IASetPrimitiveTopology(Topology);

#if RESHADE_ADDON
	static_assert(
		(DWORD)reshade::api::primitive_topology::point_list         == D3D10_PRIMITIVE_TOPOLOGY_POINTLIST &&
		(DWORD)reshade::api::primitive_topology::line_list          == D3D10_PRIMITIVE_TOPOLOGY_LINELIST &&
		(DWORD)reshade::api::primitive_topology::line_strip         == D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP &&
		(DWORD)reshade::api::primitive_topology::triangle_list      == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST &&
		(DWORD)reshade::api::primitive_topology::triangle_strip     == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP &&
		(DWORD)reshade::api::primitive_topology::line_list_adj      == D3D10_PRIMITIVE_TOPOLOGY_LINELIST_ADJ &&
		(DWORD)reshade::api::primitive_topology::line_strip_adj     == D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ &&
		(DWORD)reshade::api::primitive_topology::triangle_list_adj  == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ &&
		(DWORD)reshade::api::primitive_topology::triangle_strip_adj == D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ);

	const reshade::api::dynamic_state state = reshade::api::dynamic_state::primitive_topology;
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, reinterpret_cast<const uint32_t *>(&Topology));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
#if RESHADE_ADDON
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::vertex, StartSlot, NumViews, ppShaderResourceViews);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
#if RESHADE_ADDON
	invoke_bind_samplers_event(reshade::api::shader_stage::vertex, StartSlot, NumSamplers, ppSamplers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::SetPredication(ID3D10Predicate *pPredicate, BOOL PredicateValue)
{
	_orig->SetPredication(pPredicate, PredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	_orig->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
#if RESHADE_ADDON
	invoke_bind_shader_resource_views_event(reshade::api::shader_stage::geometry, StartSlot, NumViews, ppShaderResourceViews);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState *const *ppSamplers)
{
	_orig->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
#if RESHADE_ADDON
	invoke_bind_samplers_event(reshade::api::shader_stage::geometry, StartSlot, NumSamplers, ppSamplers);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetRenderTargets(UINT NumViews, ID3D10RenderTargetView *const *ppRenderTargetViews, ID3D10DepthStencilView *pDepthStencilView)
{
	_orig->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);

#if RESHADE_ADDON
	assert(NumViews <= D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT);

	if (!reshade::has_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>())
		return;

#ifndef WIN64
	reshade::api::resource_view rtvs[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	for (UINT i = 0; i < NumViews; ++i)
		rtvs[i] = reshade::api::resource_view { reinterpret_cast<uintptr_t>(ppRenderTargetViews[i]) };
#else
	static_assert(sizeof(*ppRenderTargetViews) == sizeof(reshade::api::resource_view));
	const auto rtvs = reinterpret_cast<const reshade::api::resource_view *>(ppRenderTargetViews);
#endif

	reshade::invoke_addon_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(this, NumViews, rtvs, reshade::api::resource_view { reinterpret_cast<uintptr_t>(pDepthStencilView) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetBlendState(ID3D10BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask)
{
	_orig->OMSetBlendState(pBlendState, BlendFactor, SampleMask);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this,
		reshade::api::pipeline_stage::output_merger, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pBlendState) });

	const reshade::api::dynamic_state states[2] = { reshade::api::dynamic_state::blend_constant, reshade::api::dynamic_state::sample_mask };
	const uint32_t values[2] = {
		(BlendFactor == nullptr) ? 0xFFFFFFFF : // Default blend factor is { 1, 1, 1, 1 }
			((static_cast<uint32_t>(BlendFactor[0] * 255.f) & 0xFF)) |
			((static_cast<uint32_t>(BlendFactor[1] * 255.f) & 0xFF) << 8) |
			((static_cast<uint32_t>(BlendFactor[2] * 255.f) & 0xFF) << 16) |
			((static_cast<uint32_t>(BlendFactor[3] * 255.f) & 0xFF) << 24), SampleMask };
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 2, states, values);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::OMSetDepthStencilState(ID3D10DepthStencilState *pDepthStencilState, UINT StencilRef)
{
	_orig->OMSetDepthStencilState(pDepthStencilState, StencilRef);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this,
		reshade::api::pipeline_stage::depth_stencil, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pDepthStencilState) });

	const reshade::api::dynamic_state state = reshade::api::dynamic_state::stencil_reference_value;
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline_states>(this, 1, &state, &StencilRef);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::SOSetTargets(UINT NumBuffers, ID3D10Buffer *const *ppSOTargets, const UINT *pOffsets)
{
	_orig->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::DrawAuto()
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::draw>(this, 0, 0, 0, 0))
		return;
#endif
	_orig->DrawAuto();
}
void    STDMETHODCALLTYPE D3D10Device::RSSetState(ID3D10RasterizerState *pRasterizerState)
{
	_orig->RSSetState(pRasterizerState);

#if RESHADE_ADDON
	reshade::invoke_addon_event<reshade::addon_event::bind_pipeline>(this,
		reshade::api::pipeline_stage::rasterizer, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pRasterizerState) });
#endif
}
void    STDMETHODCALLTYPE D3D10Device::RSSetViewports(UINT NumViewports, const D3D10_VIEWPORT *pViewports)
{
	_orig->RSSetViewports(NumViewports, pViewports);

#if RESHADE_ADDON
	assert(NumViewports <= D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);

	if (!reshade::has_addon_event<reshade::addon_event::bind_viewports>())
		return;

	float viewport_data[6 * D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	for (UINT i = 0, k = 0; i < NumViewports; ++i, k += 6)
	{
		viewport_data[k + 0] = static_cast<float>(pViewports[i].TopLeftX);
		viewport_data[k + 1] = static_cast<float>(pViewports[i].TopLeftY);
		viewport_data[k + 2] = static_cast<float>(pViewports[i].Width);
		viewport_data[k + 3] = static_cast<float>(pViewports[i].Height);
		viewport_data[k + 4] = pViewports[i].MinDepth;
		viewport_data[k + 5] = pViewports[i].MaxDepth;
	}

	reshade::invoke_addon_event<reshade::addon_event::bind_viewports>(this, 0, NumViewports, viewport_data);
#endif
}
void    STDMETHODCALLTYPE D3D10Device::RSSetScissorRects(UINT NumRects, const D3D10_RECT *pRects)
{
	_orig->RSSetScissorRects(NumRects, pRects);

#if RESHADE_ADDON
	static_assert(sizeof(D3D10_RECT) == (sizeof(int32_t) * 4));

	reshade::invoke_addon_event<reshade::addon_event::bind_scissor_rects>(this, 0, NumRects, reinterpret_cast<const int32_t *>(pRects));
#endif
}
void    STDMETHODCALLTYPE D3D10Device::CopySubresourceRegion(ID3D10Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D10Resource *pSrcResource, UINT SrcSubresource, const D3D10_BOX *pSrcBox)
{
#if RESHADE_ADDON
	assert(pDstResource != nullptr && pSrcResource != nullptr);

	if (reshade::has_addon_event<reshade::addon_event::copy_buffer_region>() ||
		reshade::has_addon_event<reshade::addon_event::copy_texture_region>())
	{
		D3D10_RESOURCE_DIMENSION type = D3D10_RESOURCE_DIMENSION_UNKNOWN;
		pDstResource->GetType(&type);

		if (type == D3D10_RESOURCE_DIMENSION_BUFFER)
		{
			assert(SrcSubresource == 0 && DstSubresource == 0);

			if (reshade::invoke_addon_event<reshade::addon_event::copy_buffer_region>(this,
				reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, pSrcBox != nullptr ? pSrcBox->left : 0,
				reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstX, pSrcBox != nullptr ? pSrcBox->right - pSrcBox->left : std::numeric_limits<uint64_t>::max()))
				return;
		}
		else
		{
			int32_t dst_box[6] = { static_cast<int32_t>(DstX), static_cast<int32_t>(DstY), static_cast<int32_t>(DstZ) };
			if (pSrcBox != nullptr)
			{
				dst_box[3] = dst_box[0] + pSrcBox->right - pSrcBox->left;
				dst_box[4] = dst_box[1] + pSrcBox->bottom - pSrcBox->top;
				dst_box[5] = dst_box[2] + pSrcBox->back - pSrcBox->front;
			}
			else
			{
				// TODO: Destination box size is not implemented (would have to get it from the resource)
				assert(DstX == 0 && DstY == 0 && DstZ == 0);
			}

			static_assert(sizeof(D3D10_BOX) == (sizeof(int32_t) * 6));

			if (reshade::invoke_addon_event<reshade::addon_event::copy_texture_region>(this,
				reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, SrcSubresource, reinterpret_cast<const int32_t *>(pSrcBox),
				reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, DstX != 0 || DstY != 0 || DstZ != 0 ? dst_box : nullptr, reshade::api::filter_mode::min_mag_mip_point))
				return;
		}
	}
#endif

	_orig->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox);
}
void    STDMETHODCALLTYPE D3D10Device::CopyResource(ID3D10Resource *pDstResource, ID3D10Resource *pSrcResource)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::copy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }))
		return;
#endif
	_orig->CopyResource(pDstResource, pSrcResource);
}
void    STDMETHODCALLTYPE D3D10Device::UpdateSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, const D3D10_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch)
{
#if RESHADE_ADDON
	assert(pDstResource != nullptr);

	if (reshade::has_addon_event<reshade::addon_event::update_buffer_region>() ||
		reshade::has_addon_event<reshade::addon_event::update_texture_region>())
	{
		D3D10_RESOURCE_DIMENSION type = D3D10_RESOURCE_DIMENSION_UNKNOWN;
		pDstResource->GetType(&type);

		if (type == D3D10_RESOURCE_DIMENSION_BUFFER)
		{
			assert(DstSubresource == 0);

			if (reshade::invoke_addon_event<reshade::addon_event::update_buffer_region>(this,
				pSrcData,
				reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) },
				pDstBox != nullptr ? pDstBox->left : 0,
				pDstBox != nullptr ? pDstBox->right - pDstBox->left : SrcRowPitch))
				return;
		}
		else
		{
			static_assert(sizeof(D3D10_BOX) == (sizeof(int32_t) * 6));

			if (reshade::invoke_addon_event<reshade::addon_event::update_texture_region>(this,
				reshade::api::subresource_data { const_cast<void *>(pSrcData), SrcRowPitch, SrcDepthPitch },
				reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, reinterpret_cast<const int32_t *>(pDstBox)))
				return;
		}
	}
#endif

	_orig->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
void    STDMETHODCALLTYPE D3D10Device::ClearRenderTargetView(ID3D10RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::clear_render_target_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(pRenderTargetView) }, ColorRGBA, 0, nullptr))
		return;
#endif
	_orig->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}
void    STDMETHODCALLTYPE D3D10Device::ClearDepthStencilView(ID3D10DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
#if RESHADE_ADDON
	static_assert(
		(UINT)reshade::api::attachment_type::depth   == (D3D10_CLEAR_DEPTH << 1) &&
		(UINT)reshade::api::attachment_type::stencil == (D3D10_CLEAR_STENCIL << 1));

	if (reshade::invoke_addon_event<reshade::addon_event::clear_depth_stencil_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(pDepthStencilView) }, static_cast<reshade::api::attachment_type>(ClearFlags << 1), Depth, Stencil, 0, nullptr))
		return;
#endif
	_orig->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
void    STDMETHODCALLTYPE D3D10Device::GenerateMips(ID3D10ShaderResourceView *pShaderResourceView)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::generate_mipmaps>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(pShaderResourceView) }))
		return;
#endif
	_orig->GenerateMips(pShaderResourceView);
}
void    STDMETHODCALLTYPE D3D10Device::ResolveSubresource(ID3D10Resource *pDstResource, UINT DstSubresource, ID3D10Resource *pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
#if RESHADE_ADDON
	if (reshade::invoke_addon_event<reshade::addon_event::resolve_texture_region>(this,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pSrcResource) }, SrcSubresource, nullptr,
		reshade::api::resource { reinterpret_cast<uintptr_t>(pDstResource) }, DstSubresource, nullptr, reshade::d3d10::convert_format(Format)))
		return;
#endif
	_orig->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetShader(ID3D10PixelShader **ppPixelShader)
{
	_orig->PSGetShader(ppPixelShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetShader(ID3D10VertexShader **ppVertexShader)
{
	_orig->VSGetShader(ppVertexShader);
}
void    STDMETHODCALLTYPE D3D10Device::PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetInputLayout(ID3D10InputLayout **ppInputLayout)
{
	_orig->IAGetInputLayout(ppInputLayout);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets)
{
	_orig->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetIndexBuffer(ID3D10Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset)
{
	_orig->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D10Buffer **ppConstantBuffers)
{
	_orig->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetShader(ID3D10GeometryShader **ppGeometryShader)
{
	_orig->GSGetShader(ppGeometryShader);
}
void    STDMETHODCALLTYPE D3D10Device::IAGetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY *pTopology)
{
	_orig->IAGetPrimitiveTopology(pTopology);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::GetPredication(ID3D10Predicate **ppPredicate, BOOL *pPredicateValue)
{
	_orig->GetPredication(ppPredicate, pPredicateValue);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D10ShaderResourceView **ppShaderResourceViews)
{
	_orig->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
void    STDMETHODCALLTYPE D3D10Device::GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D10SamplerState **ppSamplers)
{
	_orig->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetRenderTargets(UINT NumViews, ID3D10RenderTargetView **ppRenderTargetViews, ID3D10DepthStencilView **ppDepthStencilView)
{
	_orig->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetBlendState(ID3D10BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask)
{
	_orig->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
void    STDMETHODCALLTYPE D3D10Device::OMGetDepthStencilState(ID3D10DepthStencilState **ppDepthStencilState, UINT *pStencilRef)
{
	_orig->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
void    STDMETHODCALLTYPE D3D10Device::SOGetTargets(UINT NumBuffers, ID3D10Buffer **ppSOTargets, UINT *pOffsets)
{
	_orig->SOGetTargets(NumBuffers, ppSOTargets, pOffsets);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetState(ID3D10RasterizerState **ppRasterizerState)
{
	_orig->RSGetState(ppRasterizerState);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetViewports(UINT *NumViewports, D3D10_VIEWPORT *pViewports)
{
	_orig->RSGetViewports(NumViewports, pViewports);
}
void    STDMETHODCALLTYPE D3D10Device::RSGetScissorRects(UINT *NumRects, D3D10_RECT *pRects)
{
	_orig->RSGetScissorRects(NumRects, pRects);
}
HRESULT STDMETHODCALLTYPE D3D10Device::GetDeviceRemovedReason()
{
	return _orig->GetDeviceRemovedReason();
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetExceptionMode(UINT RaiseFlags)
{
	return _orig->SetExceptionMode(RaiseFlags);
}
UINT    STDMETHODCALLTYPE D3D10Device::GetExceptionMode()
{
	return _orig->GetExceptionMode();
}
HRESULT STDMETHODCALLTYPE D3D10Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
	return _orig->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
	return _orig->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D10Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
	return _orig->SetPrivateDataInterface(guid, pData);
}
void    STDMETHODCALLTYPE D3D10Device::ClearState()
{
	_orig->ClearState();
	// TODO: Call events with cleared state
}
void    STDMETHODCALLTYPE D3D10Device::Flush()
{
	_orig->Flush();
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBuffer(const D3D10_BUFFER_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Buffer **ppBuffer)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppBuffer == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBuffer(pDesc, pInitialData, nullptr);

	D3D10_BUFFER_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		static_assert(sizeof(*pInitialData) == sizeof(reshade::api::subresource_data));

		initial_data.resize(1);
		initial_data[0] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(&initial_data);
	}
#endif

	const HRESULT hr = _orig->CreateBuffer(pDesc, pInitialData, ppBuffer);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppBuffer) });

		register_destruction_callback(*ppBuffer, [this, resource = *ppBuffer]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateBuffer" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture1D(const D3D10_TEXTURE1D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture1D **ppTexture1D)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture1D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture1D(pDesc, pInitialData, nullptr);

	D3D10_TEXTURE1D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		// Allocate sufficient space in the array, in case an add-on changes the texture description, but wants to upload initial data still
		initial_data.resize(D3D10_REQ_MIP_LEVELS * D3D10_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc->MipLevels * pDesc->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture1D) });

		register_destruction_callback(*ppTexture1D, [this, resource = *ppTexture1D]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateTexture1D" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture2D(const D3D10_TEXTURE2D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture2D **ppTexture2D)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture2D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture2D(pDesc, pInitialData, nullptr);

	D3D10_TEXTURE2D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D10_REQ_MIP_LEVELS * D3D10_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION);
		for (UINT i = 0; i < pDesc->MipLevels * pDesc->ArraySize; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture2D) });

		register_destruction_callback(*ppTexture2D, [this, resource = *ppTexture2D]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateTexture2D" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateTexture3D(const D3D10_TEXTURE3D_DESC *pDesc, const D3D10_SUBRESOURCE_DATA *pInitialData, ID3D10Texture3D **ppTexture3D)
{
#if RESHADE_ADDON
	if (pDesc == nullptr)
		return E_INVALIDARG;
	if (ppTexture3D == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateTexture3D(pDesc, pInitialData, nullptr);

	D3D10_TEXTURE3D_DESC internal_desc = *pDesc;
	auto desc = reshade::d3d10::convert_resource_desc(internal_desc);

	std::vector<reshade::api::subresource_data> initial_data;
	if (pInitialData != nullptr)
	{
		initial_data.resize(D3D10_REQ_MIP_LEVELS);
		for (UINT i = 0; i < pDesc->MipLevels; ++i)
			initial_data[i] = *reinterpret_cast<const reshade::api::subresource_data *>(pInitialData + i);
	}

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource>(this, desc, initial_data.data(), reshade::api::resource_usage::general))
	{
		reshade::d3d10::convert_resource_desc(internal_desc);
		pDesc = &internal_desc;
		pInitialData = reinterpret_cast<const D3D10_SUBRESOURCE_DATA *>(initial_data.data());
	}
#endif

	const HRESULT hr = _orig->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource>(
			this, desc, reinterpret_cast<const reshade::api::subresource_data *>(pInitialData), reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(*ppTexture3D) });

		register_destruction_callback(*ppTexture3D, [this, resource = *ppTexture3D]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateTexture3D" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc, ID3D10ShaderResourceView **ppShaderResourceView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppShaderResourceView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateShaderResourceView(pResource, pDesc, nullptr);

	D3D10_SHADER_RESOURCE_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateShaderResourceView(pResource, pDesc, ppShaderResourceView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppShaderResourceView) });

		register_destruction_callback(*ppShaderResourceView, [this, resource_view = *ppShaderResourceView]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource_view) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateShaderResourceView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRenderTargetView(ID3D10Resource *pResource, const D3D10_RENDER_TARGET_VIEW_DESC *pDesc, ID3D10RenderTargetView **ppRenderTargetView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppRenderTargetView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRenderTargetView(pResource, pDesc, nullptr);

	D3D10_RENDER_TARGET_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D10_RENDER_TARGET_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_RTV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateRenderTargetView(pResource, pDesc, ppRenderTargetView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::render_target, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppRenderTargetView) });

		register_destruction_callback(*ppRenderTargetView, [this, resource_view = *ppRenderTargetView]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource_view) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateRenderTargetView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilView(ID3D10Resource *pResource, const D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc, ID3D10DepthStencilView **ppDepthStencilView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppDepthStencilView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDepthStencilView(pResource, pDesc, nullptr);

	D3D10_DEPTH_STENCIL_VIEW_DESC internal_desc = pDesc != nullptr ? *pDesc : D3D10_DEPTH_STENCIL_VIEW_DESC { DXGI_FORMAT_UNKNOWN, D3D10_DSV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::depth_stencil, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppDepthStencilView) });

		register_destruction_callback(*ppDepthStencilView, [this, resource_view = *ppDepthStencilView]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource_view) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateDepthStencilView" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateInputLayout(const D3D10_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D10InputLayout **ppInputLayout)
{
#if RESHADE_ADDON
	if (ppInputLayout == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, nullptr);

	auto desc = reshade::d3d10::convert_pipeline_desc(pInputElementDescs, NumElements);
	desc.graphics.vertex_shader.code = pShaderBytecodeWithInputSignature;
	desc.graphics.vertex_shader.code_size = BytecodeLength;

	std::vector<D3D10_INPUT_ELEMENT_DESC> internal_elements;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d10::convert_pipeline_desc(desc, internal_elements);
		pInputElementDescs = internal_elements.data();
		NumElements = static_cast<UINT>(internal_elements.size());
		pShaderBytecodeWithInputSignature = desc.graphics.vertex_shader.code;
		BytecodeLength = desc.graphics.vertex_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppInputLayout) });

		register_destruction_callback(*ppInputLayout, [this, pipeline = *ppInputLayout]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateInputLayout" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader)
{
#if RESHADE_ADDON
	if (ppVertexShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::vertex_shader };
	desc.graphics.vertex_shader.code = pShaderBytecode;
	desc.graphics.vertex_shader.code_size = BytecodeLength;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		pShaderBytecode = desc.graphics.vertex_shader.code;
		BytecodeLength  = desc.graphics.vertex_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppVertexShader) });

		register_destruction_callback(*ppVertexShader, [this, pipeline = *ppVertexShader]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateVertexShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader **ppGeometryShader)
{
#if RESHADE_ADDON
	if (ppGeometryShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::geometry_shader };
	desc.graphics.geometry_shader.code = pShaderBytecode;
	desc.graphics.geometry_shader.code_size = BytecodeLength;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		pShaderBytecode = desc.graphics.geometry_shader.code;
		BytecodeLength  = desc.graphics.geometry_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, ppGeometryShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppGeometryShader) });

		register_destruction_callback(*ppGeometryShader, [this, pipeline = *ppGeometryShader]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateGeometryShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength, const D3D10_SO_DECLARATION_ENTRY *pSODeclaration, UINT NumEntries, UINT OutputStreamStride, ID3D10GeometryShader **ppGeometryShader)
{
#if RESHADE_ADDON
	if (ppGeometryShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, OutputStreamStride, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::geometry_shader };
	desc.graphics.geometry_shader.code = pShaderBytecode;
	desc.graphics.geometry_shader.code_size = BytecodeLength;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		pShaderBytecode = desc.graphics.geometry_shader.code;
		BytecodeLength  = desc.graphics.geometry_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, OutputStreamStride, ppGeometryShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppGeometryShader) });

		register_destruction_callback(*ppGeometryShader, [this, pipeline = *ppGeometryShader]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateGeometryShaderWithStreamOutput" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader **ppPixelShader)
{
#if RESHADE_ADDON
	if (ppPixelShader == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, nullptr);

	reshade::api::pipeline_desc desc = { reshade::api::pipeline_stage::pixel_shader };
	desc.graphics.pixel_shader.code = pShaderBytecode;
	desc.graphics.pixel_shader.code_size = BytecodeLength;

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		pShaderBytecode = desc.graphics.pixel_shader.code;
		BytecodeLength  = desc.graphics.pixel_shader.code_size;
	}
#endif

	const HRESULT hr = _orig->CreatePixelShader(pShaderBytecode, BytecodeLength, ppPixelShader);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppPixelShader) });

		register_destruction_callback(*ppPixelShader, [this, pipeline = *ppPixelShader]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreatePixelShader" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState(const D3D10_BLEND_DESC *pBlendStateDesc, ID3D10BlendState **ppBlendState)
{
#if RESHADE_ADDON
	if (ppBlendState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBlendState(pBlendStateDesc, nullptr);

	D3D10_BLEND_DESC internal_desc = {};
	auto desc = reshade::d3d10::convert_pipeline_desc(pBlendStateDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d10::convert_pipeline_desc(desc, internal_desc);
		pBlendStateDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateBlendState(pBlendStateDesc, ppBlendState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppBlendState) });

		register_destruction_callback(*ppBlendState, [this, pipeline = *ppBlendState]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateBlendState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateDepthStencilState(const D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc, ID3D10DepthStencilState **ppDepthStencilState)
{
#if RESHADE_ADDON
	if (ppDepthStencilState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateDepthStencilState(pDepthStencilDesc, nullptr);

	D3D10_DEPTH_STENCIL_DESC internal_desc = {};
	auto desc = reshade::d3d10::convert_pipeline_desc(pDepthStencilDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d10::convert_pipeline_desc(desc, internal_desc);
		pDepthStencilDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppDepthStencilState) });

		register_destruction_callback(*ppDepthStencilState, [this, pipeline = *ppDepthStencilState]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateDepthStencilState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateRasterizerState(const D3D10_RASTERIZER_DESC *pRasterizerDesc, ID3D10RasterizerState **ppRasterizerState)
{
#if RESHADE_ADDON
	if (ppRasterizerState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateRasterizerState(pRasterizerDesc, nullptr);

	D3D10_RASTERIZER_DESC internal_desc = {};
	auto desc = reshade::d3d10::convert_pipeline_desc(pRasterizerDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d10::convert_pipeline_desc(desc, internal_desc);
		pRasterizerDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppRasterizerState) });

		register_destruction_callback(*ppRasterizerState, [this, pipeline = *ppRasterizerState]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateRasterizerState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateSamplerState(const D3D10_SAMPLER_DESC *pSamplerDesc, ID3D10SamplerState **ppSamplerState)
{
#if RESHADE_ADDON
	if (pSamplerDesc == nullptr)
		return E_INVALIDARG;
	if (ppSamplerState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateSamplerState(pSamplerDesc, nullptr);

	D3D10_SAMPLER_DESC internal_desc = *pSamplerDesc;
	auto desc = reshade::d3d10::convert_sampler_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_sampler>(this, desc))
	{
		reshade::d3d10::convert_sampler_desc(desc, internal_desc);
		pSamplerDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_sampler>(this, desc, reshade::api::sampler { reinterpret_cast<uintptr_t>(*ppSamplerState) });

		register_destruction_callback(*ppSamplerState, [this, sampler = *ppSamplerState]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_sampler>(this, reshade::api::sampler { reinterpret_cast<uintptr_t>(sampler) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::CreateSamplerState" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateQuery(const D3D10_QUERY_DESC *pQueryDesc, ID3D10Query **ppQuery)
{
	return _orig->CreateQuery(pQueryDesc, ppQuery);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreatePredicate(const D3D10_QUERY_DESC *pPredicateDesc, ID3D10Predicate **ppPredicate)
{
	return _orig->CreatePredicate(pPredicateDesc, ppPredicate);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateCounter(const D3D10_COUNTER_DESC *pCounterDesc, ID3D10Counter **ppCounter)
{
	return _orig->CreateCounter(pCounterDesc, ppCounter);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport)
{
	return _orig->CheckFormatSupport(Format, pFormatSupport);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT *pNumQualityLevels)
{
	return _orig->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}
void    STDMETHODCALLTYPE D3D10Device::CheckCounterInfo(D3D10_COUNTER_INFO *pCounterInfo)
{
	_orig->CheckCounterInfo(pCounterInfo);
}
HRESULT STDMETHODCALLTYPE D3D10Device::CheckCounter(const D3D10_COUNTER_DESC *pDesc, D3D10_COUNTER_TYPE *pType, UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits, UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength)
{
	return _orig->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
UINT    STDMETHODCALLTYPE D3D10Device::GetCreationFlags()
{
	return _orig->GetCreationFlags();
}
HRESULT STDMETHODCALLTYPE D3D10Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource)
{
	const HRESULT hr = _orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		const auto resource = static_cast<ID3D10Resource *>(*ppResource);
		reshade::api::resource_desc desc;

		if (com_ptr<ID3D10Buffer> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D10_BUFFER_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
		}
		if (com_ptr<ID3D10Texture1D> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D10_TEXTURE1D_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
		}
		if (com_ptr<ID3D10Texture2D> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D10_TEXTURE2D_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
		}
		if (com_ptr<ID3D10Texture3D> resource_impl;
			SUCCEEDED(resource->QueryInterface(&resource_impl)))
		{
			D3D10_TEXTURE3D_DESC internal_desc;
			resource_impl->GetDesc(&internal_desc);
			desc = reshade::d3d10::convert_resource_desc(internal_desc);
		}

		assert((desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared);

		reshade::invoke_addon_event<reshade::addon_event::init_resource>(this, desc, nullptr, reshade::api::resource_usage::general, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });

		register_destruction_callback(resource, [this, resource]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(resource) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device::OpenSharedResource" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
void    STDMETHODCALLTYPE D3D10Device::SetTextFilterSize(UINT Width, UINT Height)
{
	_orig->SetTextFilterSize(Width, Height);
}
void    STDMETHODCALLTYPE D3D10Device::GetTextFilterSize(UINT *pWidth, UINT *pHeight)
{
	_orig->GetTextFilterSize(pWidth, pHeight);
}

HRESULT STDMETHODCALLTYPE D3D10Device::CreateShaderResourceView1(ID3D10Resource *pResource, const D3D10_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D10ShaderResourceView1 **ppShaderResourceView)
{
#if RESHADE_ADDON
	if (pResource == nullptr)
		return E_INVALIDARG;
	if (ppShaderResourceView == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateShaderResourceView1(pResource, pDesc, nullptr);

	D3D10_SHADER_RESOURCE_VIEW_DESC1 internal_desc = pDesc != nullptr ? *pDesc : D3D10_SHADER_RESOURCE_VIEW_DESC1 { DXGI_FORMAT_UNKNOWN, D3D10_1_SRV_DIMENSION_UNKNOWN };
	auto desc = reshade::d3d10::convert_resource_view_desc(internal_desc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_resource_view>(this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc))
	{
		reshade::d3d10::convert_resource_view_desc(desc, internal_desc);
		pDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateShaderResourceView1(pResource, pDesc, ppShaderResourceView);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_resource_view>(
			this, reshade::api::resource { reinterpret_cast<uintptr_t>(pResource) }, reshade::api::resource_usage::shader_resource, desc, reshade::api::resource_view { reinterpret_cast<uintptr_t>(*ppShaderResourceView) });

		register_destruction_callback(*ppShaderResourceView, [this, resource_view = *ppShaderResourceView]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_resource_view>(this, reshade::api::resource_view { reinterpret_cast<uintptr_t>(resource_view) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device1::CreateShaderResourceView1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE D3D10Device::CreateBlendState1(const D3D10_BLEND_DESC1 *pBlendStateDesc, ID3D10BlendState1 **ppBlendState)
{
#if RESHADE_ADDON
	if (ppBlendState == nullptr) // This can happen when application only wants to validate input parameters
		return _orig->CreateBlendState1(pBlendStateDesc, nullptr);

	D3D10_BLEND_DESC1 internal_desc = {};
	auto desc = reshade::d3d10::convert_pipeline_desc(pBlendStateDesc);

	if (reshade::invoke_addon_event<reshade::addon_event::create_pipeline>(this, desc))
	{
		reshade::d3d10::convert_pipeline_desc(desc, internal_desc);
		pBlendStateDesc = &internal_desc;
	}
#endif

	const HRESULT hr = _orig->CreateBlendState1(pBlendStateDesc, ppBlendState);
	if (SUCCEEDED(hr))
	{
#if RESHADE_ADDON
		reshade::invoke_addon_event<reshade::addon_event::init_pipeline>(this, desc, reshade::api::pipeline { reinterpret_cast<uintptr_t>(*ppBlendState) });

		register_destruction_callback(*ppBlendState, [this, pipeline = *ppBlendState]() {
			reshade::invoke_addon_event<reshade::addon_event::destroy_pipeline>(this, reshade::api::pipeline { reinterpret_cast<uintptr_t>(pipeline) });
		});
#endif
	}
	else
	{
#if RESHADE_VERBOSE_LOG
		LOG(WARN) << "ID3D10Device1::CreateBlendState1" << " failed with error code " << hr << '.';
#endif
	}

	return hr;
}
D3D10_FEATURE_LEVEL1 STDMETHODCALLTYPE D3D10Device::GetFeatureLevel()
{
	return _orig->GetFeatureLevel();
}
