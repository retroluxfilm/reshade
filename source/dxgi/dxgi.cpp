/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "dxgi_swapchain.hpp"
#include "d3d10/d3d10_device.hpp"
#include "d3d11/d3d11_device.hpp"
#include "d3d12/d3d12_command_queue.hpp"
#include "dll_log.hpp" // Include late to get HRESULT log overloads
#include "ini_file.hpp"
#include "hook_manager.hpp"

extern bool is_windows7();

// Needs to be set whenever a DXGI call can end up in 'CDXGISwapChain::EnsureChildDeviceInternal', to avoid hooking internal D3D device creation
extern thread_local bool g_in_dxgi_runtime;

static void dump_format(DXGI_FORMAT format)
{
	const char *format_string = nullptr;
	switch (format)
	{
	case DXGI_FORMAT_UNKNOWN:
		format_string = "DXGI_FORMAT_UNKNOWN";
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		format_string = "DXGI_FORMAT_R8G8B8A8_UNORM";
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		format_string = "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM:
		format_string = "DXGI_FORMAT_B8G8R8A8_UNORM";
		break;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		format_string = "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		format_string = "DXGI_FORMAT_R10G10B10A2_UNORM";
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		format_string = "DXGI_FORMAT_R16G16B16A16_FLOAT";
		break;
	}

	if (format_string != nullptr)
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << format_string << " |";
	else
		LOG(INFO) << "  | Format                                  | " << std::setw(39) << format << " |";
}
static void dump_sample_desc(const DXGI_SAMPLE_DESC &desc)
{
	LOG(INFO) <<     "  | SampleCount                             | " << std::setw(39) << desc.Count   << " |";
	switch (desc.Quality)
	{
	case D3D11_CENTER_MULTISAMPLE_PATTERN:
		LOG(INFO) << "  | SampleQuality                           | D3D11_CENTER_MULTISAMPLE_PATTERN        |";
		break;
	case D3D11_STANDARD_MULTISAMPLE_PATTERN:
		LOG(INFO) << "  | SampleQuality                           | D3D11_STANDARD_MULTISAMPLE_PATTERN      |";
		break;
	default:
		LOG(INFO) << "  | SampleQuality                           | " << std::setw(39) << desc.Quality << " |";
		break;
	}
}

static void dump_and_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC &desc)
{
	LOG(INFO) << "> Dumping swap chain description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Width                                   | " << std::setw(39) << desc.BufferDesc.Width   << " |";
	LOG(INFO) << "  | Height                                  | " << std::setw(39) << desc.BufferDesc.Height  << " |";
	LOG(INFO) << "  | RefreshRate                             | " << std::setw(19) << desc.BufferDesc.RefreshRate.Numerator << ' ' << std::setw(19) << desc.BufferDesc.RefreshRate.Denominator << " |";
	dump_format(desc.BufferDesc.Format);
	LOG(INFO) << "  | ScanlineOrdering                        | " << std::setw(39) << desc.BufferDesc.ScanlineOrdering   << " |";
	LOG(INFO) << "  | Scaling                                 | " << std::setw(39) << desc.BufferDesc.Scaling << " |";
	dump_sample_desc(desc.SampleDesc);
	LOG(INFO) << "  | BufferUsage                             | " << std::setw(39) << std::hex << desc.BufferUsage << std::dec << " |";
	LOG(INFO) << "  | BufferCount                             | " << std::setw(39) << desc.BufferCount  << " |";
	LOG(INFO) << "  | OutputWindow                            | " << std::setw(39) << desc.OutputWindow << " |";
	LOG(INFO) << "  | Windowed                                | " << std::setw(39) << (desc.Windowed ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | SwapEffect                              | " << std::setw(39) << desc.SwapEffect   << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << desc.Flags << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

#if RESHADE_ADDON
	reshade::api::resource_desc buffer_desc = {};
	buffer_desc.type = reshade::api::resource_type::texture_2d;
	buffer_desc.texture.width = desc.BufferDesc.Width;
	buffer_desc.texture.height = desc.BufferDesc.Height;
	buffer_desc.texture.depth_or_layers = 1;
	buffer_desc.texture.levels = 1;
	buffer_desc.texture.format = static_cast<reshade::api::format>(desc.BufferDesc.Format);
	buffer_desc.texture.samples = static_cast<uint16_t>(desc.SampleDesc.Count);
	buffer_desc.heap = reshade::api::memory_heap::gpu_only;

	if (desc.BufferUsage & DXGI_USAGE_SHADER_INPUT)
		buffer_desc.usage |= reshade::api::resource_usage::shader_resource;
	if (desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT)
		buffer_desc.usage |= reshade::api::resource_usage::render_target;
	if (desc.BufferUsage & DXGI_USAGE_SHARED)
		buffer_desc.flags |= reshade::api::resource_flags::shared;
	if (desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
		buffer_desc.usage |= reshade::api::resource_usage::unordered_access;

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(buffer_desc, desc.OutputWindow))
	{
		desc.BufferDesc.Width = buffer_desc.texture.width;
		desc.BufferDesc.Height = buffer_desc.texture.height;
		desc.BufferDesc.Format = static_cast<DXGI_FORMAT>(buffer_desc.texture.format);
		desc.SampleDesc.Count = buffer_desc.texture.samples;

		if ((buffer_desc.usage & reshade::api::resource_usage::shader_resource) != reshade::api::resource_usage::undefined)
			desc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
		if ((buffer_desc.usage & reshade::api::resource_usage::render_target) != reshade::api::resource_usage::undefined)
			desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		if ((buffer_desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared)
			desc.BufferUsage |= DXGI_USAGE_SHARED;
		if ((buffer_desc.usage & reshade::api::resource_usage::unordered_access) != reshade::api::resource_usage::undefined)
			desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;
	}
#endif

	if (reshade::global_config().get("APP", "ForceWindowed"))
	{
		desc.Windowed = TRUE;
	}
	if (reshade::global_config().get("APP", "ForceFullscreen"))
	{
		desc.Windowed = FALSE;
	}

	if (unsigned int force_resolution[2] = {};
		reshade::global_config().get("APP", "ForceResolution", force_resolution) &&
		force_resolution[0] != 0 && force_resolution[1] != 0)
	{
		desc.BufferDesc.Width = force_resolution[0];
		desc.BufferDesc.Height = force_resolution[1];
	}

	if (reshade::global_config().get("APP", "Force10BitFormat"))
	{
		desc.BufferDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	}
}
static void dump_and_modify_swapchain_desc(DXGI_SWAP_CHAIN_DESC1 &desc, DXGI_SWAP_CHAIN_FULLSCREEN_DESC &fullscreen_desc, HWND hwnd = nullptr)
{
	LOG(INFO) << "> Dumping swap chain description:";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Parameter                               | Value                                   |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";
	LOG(INFO) << "  | Width                                   | " << std::setw(39) << desc.Width   << " |";
	LOG(INFO) << "  | Height                                  | " << std::setw(39) << desc.Height  << " |";
	LOG(INFO) << "  | RefreshRate                             | " << std::setw(19) << fullscreen_desc.RefreshRate.Numerator << ' ' << std::setw(19) << fullscreen_desc.RefreshRate.Denominator << " |";
	dump_format(desc.Format);
	LOG(INFO) << "  | Stereo                                  | " << std::setw(39) << (desc.Stereo ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | ScanlineOrdering                        | " << std::setw(39) << fullscreen_desc.ScanlineOrdering << " |";
	LOG(INFO) << "  | Scaling                                 | " << std::setw(39) << fullscreen_desc.Scaling << " |";
	dump_sample_desc(desc.SampleDesc);
	LOG(INFO) << "  | BufferUsage                             | " << std::setw(39) << std::hex << desc.BufferUsage << std::dec << " |";
	LOG(INFO) << "  | BufferCount                             | " << std::setw(39) << desc.BufferCount << " |";
	LOG(INFO) << "  | Windowed                                | " << std::setw(39) << (fullscreen_desc.Windowed ? "TRUE" : "FALSE") << " |";
	LOG(INFO) << "  | SwapEffect                              | " << std::setw(39) << desc.SwapEffect  << " |";
	LOG(INFO) << "  | AlphaMode                               | " << std::setw(39) << desc.AlphaMode   << " |";
	LOG(INFO) << "  | Flags                                   | " << std::setw(39) << std::hex << desc.Flags << std::dec << " |";
	LOG(INFO) << "  +-----------------------------------------+-----------------------------------------+";

#if RESHADE_ADDON
	reshade::api::resource_desc buffer_desc = {};
	buffer_desc.type = reshade::api::resource_type::texture_2d;
	buffer_desc.texture.width = desc.Width;
	buffer_desc.texture.height = desc.Height;
	buffer_desc.texture.depth_or_layers = desc.Stereo ? 2 : 1;
	buffer_desc.texture.levels = 1;
	buffer_desc.texture.format = static_cast<reshade::api::format>(desc.Format);
	buffer_desc.texture.samples = static_cast<uint16_t>(desc.SampleDesc.Count);
	buffer_desc.heap = reshade::api::memory_heap::gpu_only;

	if (desc.BufferUsage & DXGI_USAGE_SHADER_INPUT)
		buffer_desc.usage |= reshade::api::resource_usage::shader_resource;
	if (desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT)
		buffer_desc.usage |= reshade::api::resource_usage::render_target;
	if (desc.BufferUsage & DXGI_USAGE_SHARED)
		buffer_desc.flags |= reshade::api::resource_flags::shared;
	if (desc.BufferUsage & DXGI_USAGE_UNORDERED_ACCESS)
		buffer_desc.usage |= reshade::api::resource_usage::unordered_access;

	if (reshade::invoke_addon_event<reshade::addon_event::create_swapchain>(buffer_desc, hwnd))
	{
		desc.Width = buffer_desc.texture.width;
		desc.Height = buffer_desc.texture.height;
		desc.Format = static_cast<DXGI_FORMAT>(buffer_desc.texture.format);
		desc.Stereo = buffer_desc.texture.depth_or_layers > 1;
		desc.SampleDesc.Count = buffer_desc.texture.samples;

		if ((buffer_desc.usage & reshade::api::resource_usage::shader_resource) != reshade::api::resource_usage::undefined)
			desc.BufferUsage |= DXGI_USAGE_SHADER_INPUT;
		if ((buffer_desc.usage & reshade::api::resource_usage::render_target) != reshade::api::resource_usage::undefined)
			desc.BufferUsage |= DXGI_USAGE_RENDER_TARGET_OUTPUT;
		if ((buffer_desc.flags & reshade::api::resource_flags::shared) == reshade::api::resource_flags::shared)
			desc.BufferUsage |= DXGI_USAGE_SHARED;
		if ((buffer_desc.usage & reshade::api::resource_usage::unordered_access) != reshade::api::resource_usage::undefined)
			desc.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;
	}
#endif

	if (reshade::global_config().get("APP", "ForceWindowed"))
	{
		fullscreen_desc.Windowed = TRUE;
	}
	if (reshade::global_config().get("APP", "ForceFullscreen"))
	{
		fullscreen_desc.Windowed = FALSE;
	}

	if (unsigned int force_resolution[2] = {};
		reshade::global_config().get("APP", "ForceResolution", force_resolution) &&
		force_resolution[0] != 0 && force_resolution[1] != 0)
	{
		desc.Width = force_resolution[0];
		desc.Height = force_resolution[1];
	}

	if (reshade::global_config().get("APP", "Force10BitFormat"))
	{
		desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	}
}

UINT query_device(IUnknown *&device, com_ptr<IUnknown> &device_proxy)
{
	if (com_ptr<D3D10Device> device_d3d10; SUCCEEDED(device->QueryInterface(&device_d3d10)))
	{
		device = device_d3d10->_orig; // Set device pointer back to original object so that the swap chain creation functions work as expected
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d10));
		return 10;
	}
	if (com_ptr<D3D11Device> device_d3d11; SUCCEEDED(device->QueryInterface(&device_d3d11)))
	{
		device = device_d3d11->_orig;
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(device_d3d11));
		return 11;
	}
	if (com_ptr<D3D12CommandQueue> command_queue_d3d12; SUCCEEDED(device->QueryInterface(&command_queue_d3d12)))
	{
		device = command_queue_d3d12->_orig;
		device_proxy = std::move(reinterpret_cast<com_ptr<IUnknown> &>(command_queue_d3d12));
		return 12;
	}

	// Did not find a hooked device
	return 0;
}

template <typename T>
static void init_swapchain_proxy(T *&swapchain, UINT direct3d_version, const com_ptr<IUnknown> &device_proxy, DXGI_USAGE usage)
{
	DXGISwapChain *swapchain_proxy = nullptr;

	if ((usage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0)
	{
		LOG(WARN) << "Skipping swap chain due to missing 'DXGI_USAGE_RENDER_TARGET_OUTPUT' flag.";
	}
	else if (direct3d_version == 10)
	{
		const com_ptr<D3D10Device> &device = reinterpret_cast<const com_ptr<D3D10Device> &>(device_proxy);

		swapchain_proxy = new DXGISwapChain(device.get(), swapchain); // Overwrite returned swap chain pointer with hooked object
	}
	else if (direct3d_version == 11)
	{
		const com_ptr<D3D11Device> &device = reinterpret_cast<const com_ptr<D3D11Device> &>(device_proxy);

		swapchain_proxy = new DXGISwapChain(device.get(), swapchain);
	}
	else if (direct3d_version == 12)
	{
		if (com_ptr<IDXGISwapChain3> swapchain3; SUCCEEDED(swapchain->QueryInterface(&swapchain3)))
		{
			const com_ptr<D3D12CommandQueue> &command_queue = reinterpret_cast<const com_ptr<D3D12CommandQueue> &>(device_proxy);

			swapchain_proxy = new DXGISwapChain(command_queue.get(), swapchain3.get());
		}
		else
		{
			LOG(WARN) << "Skipping swap chain because it is missing support for the IDXGISwapChain3 interface.";
		}
	}
	else
	{
		LOG(WARN) << "Skipping swap chain because it was created without a (hooked) Direct3D device.";
	}

	if (swapchain_proxy != nullptr)
	{
		reshade::global_config().get("APP", "ForceVSync", swapchain_proxy->_force_vsync);
		reshade::global_config().get("APP", "ForceResolution", swapchain_proxy->_force_resolution);
		reshade::global_config().get("APP", "Force10BitFormat", swapchain_proxy->_force_10_bit_format);

#if RESHADE_VERBOSE_LOG
		LOG(INFO) << "Returning IDXGISwapChain" << swapchain_proxy->_interface_version << " object " << swapchain_proxy << '.';
#endif
		swapchain = swapchain_proxy;
	}
}

HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory *pFactory, IUnknown *pDevice, DXGI_SWAP_CHAIN_DESC *pDesc, IDXGISwapChain **ppSwapChain)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(IDXGIFactory_CreateSwapChain, vtable_from_instance(pFactory) + 10)(pFactory, pDevice, pDesc, ppSwapChain);

	LOG(INFO) << "Redirecting " << "IDXGIFactory::CreateSwapChain" << '('
		<<   "this = " << pFactory
		<< ", pDevice = " << pDevice
		<< ", pDesc = " << pDesc
		<< ", ppSwapChain = " << ppSwapChain
		<< ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC desc = *pDesc;
	dump_and_modify_swapchain_desc(desc);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDXGIFactory_CreateSwapChain, vtable_from_instance(pFactory) + 10)(pFactory, pDevice, &desc, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "IDXGIFactory::CreateSwapChain" << " failed with error code " << hr << '.';
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}

HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2 *pFactory, IUnknown *pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1 *pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(IDXGIFactory2_CreateSwapChainForHwnd, vtable_from_instance(pFactory) + 15)(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

	LOG(INFO) << "Redirecting " << "IDXGIFactory2::CreateSwapChainForHwnd" << '('
		<<   "this = " << pFactory
		<< ", pDevice = " << pDevice
		<< ", hWnd = " << hWnd
		<< ", pDesc = " << pDesc
		<< ", pFullscreenDesc = " << pFullscreenDesc
		<< ", pRestrictToOutput = " << pRestrictToOutput
		<< ", ppSwapChain = " << ppSwapChain
		<< ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {};
	if (pFullscreenDesc != nullptr)
		fullscreen_desc = *pFullscreenDesc;
	else
		fullscreen_desc.Windowed = TRUE;
	dump_and_modify_swapchain_desc(desc, fullscreen_desc, hWnd);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForHwnd, vtable_from_instance(pFactory) + 15)(pFactory, pDevice, hWnd, &desc, fullscreen_desc.Windowed ? nullptr : &fullscreen_desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "IDXGIFactory2::CreateSwapChainForHwnd" << " failed with error code " << hr << '.';
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2 *pFactory, IUnknown *pDevice, IUnknown *pWindow, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(IDXGIFactory2_CreateSwapChainForCoreWindow, vtable_from_instance(pFactory) + 16)(pFactory, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);

	LOG(INFO) << "Redirecting " << "IDXGIFactory2::CreateSwapChainForCoreWindow" << '('
		<<   "this = " << pFactory
		<< ", pDevice = " << pDevice
		<< ", pWindow = " << pWindow
		<< ", pDesc = " << pDesc
		<< ", pRestrictToOutput = " << pRestrictToOutput
		<< ", ppSwapChain = " << ppSwapChain
		<< ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc; // UWP applications cannot be set into fullscreen mode
	dump_and_modify_swapchain_desc(desc, fullscreen_desc);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForCoreWindow, vtable_from_instance(pFactory) + 16)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "IDXGIFactory2::CreateSwapChainForCoreWindow" << " failed with error code " << hr << '.';
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2 *pFactory, IUnknown *pDevice, const DXGI_SWAP_CHAIN_DESC1 *pDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1 **ppSwapChain)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(IDXGIFactory2_CreateSwapChainForComposition, vtable_from_instance(pFactory) + 24)(pFactory, pDevice, pDesc, pRestrictToOutput, ppSwapChain);

	LOG(INFO) << "Redirecting " << "IDXGIFactory2::CreateSwapChainForComposition" << '('
		<<   "this = " << pFactory
		<< ", pDevice = " << pDevice
		<< ", pDesc = " << pDesc
		<< ", pRestrictToOutput = " << pRestrictToOutput
		<< ", ppSwapChain = " << ppSwapChain
		<< ')' << " ...";

	if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
		return DXGI_ERROR_INVALID_CALL;

	DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc; // Composition swap chains cannot be set into fullscreen mode
	dump_and_modify_swapchain_desc(desc, fullscreen_desc);

	com_ptr<IUnknown> device_proxy;
	const UINT direct3d_version = query_device(pDevice, device_proxy);

	g_in_dxgi_runtime = true;
	const HRESULT hr = reshade::hooks::call(IDXGIFactory2_CreateSwapChainForComposition, vtable_from_instance(pFactory) + 24)(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);
	g_in_dxgi_runtime = false;
	if (FAILED(hr))
	{
		LOG(WARN) << "IDXGIFactory2::CreateSwapChainForComposition" << " failed with error code " << hr << '.';
		return hr;
	}

	init_swapchain_proxy(*ppSwapChain, direct3d_version, device_proxy, desc.BufferUsage);

	return hr;
}

HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory(REFIID riid, void **ppFactory)
{
	LOG(INFO) << "Redirecting " << "CreateDXGIFactory" << '(' << "riid = " << riid << ", ppFactory = " << ppFactory << ')' << " ...";
	LOG(INFO) << "> Passing on to " << "CreateDXGIFactory1" << ':';

	// DXGI 1.1 should always be available, so to simplify code just call 'CreateDXGIFactory' which is otherwise identical
	return CreateDXGIFactory1(riid, ppFactory);
}
HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void **ppFactory)
{
	if (g_in_dxgi_runtime)
		return reshade::hooks::call(CreateDXGIFactory1)(riid, ppFactory);

	LOG(INFO) << "Redirecting " << "CreateDXGIFactory1" << '(' << "riid = " << riid << ", ppFactory = " << ppFactory << ')' << " ...";

	const HRESULT hr = reshade::hooks::call(CreateDXGIFactory1)(riid, ppFactory);
	if (FAILED(hr))
	{
		LOG(WARN) << "CreateDXGIFactory1" << " failed with error code " << hr << '.';
		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(IDXGIFactory_CreateSwapChain));

	// Check for DXGI 1.2 support and install IDXGIFactory2 hooks if it exists
	if (com_ptr<IDXGIFactory2> factory2; SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", vtable_from_instance(factory2.get()), 15, reinterpret_cast<reshade::hook::address>(IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", vtable_from_instance(factory2.get()), 16, reinterpret_cast<reshade::hook::address>(IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", vtable_from_instance(factory2.get()), 24, reinterpret_cast<reshade::hook::address>(IDXGIFactory2_CreateSwapChainForComposition));
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning IDXGIFactory object " << factory << '.';
#endif
	return hr;
}
HOOK_EXPORT HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory)
{
	// IDXGIFactory  {7B7166EC-21C7-44AE-B21A-C9AE321AE369}
	// IDXGIFactory1 {770AAE78-F26F-4DBA-A829-253C83D1B387}
	// IDXGIFactory2 {50C83A1C-E072-4C48-87B0-3630FA36A6D0}
	// IDXGIFactory3 {25483823-CD46-4C7D-86CA-47AA95B837BD}
	// IDXGIFactory4 {1BC6EA02-EF36-464F-BF0C-21CA39E5168A}
	// IDXGIFactory5 {7632E1f5-EE65-4DCA-87FD-84CD75F8838D}
	// IDXGIFactory6 {C1B6694F-FF09-44A9-B03C-77900A0A1D17}

	LOG(INFO) << "Redirecting " << "CreateDXGIFactory2" << '(' << "Flags = " << std::hex << Flags << std::dec << ", riid = " << riid << ", ppFactory = " << ppFactory << ')' << " ...";

	static const auto trampoline = is_windows7() ? nullptr : reshade::hooks::call(CreateDXGIFactory2);

	// CreateDXGIFactory2 is not available on Windows 7, so fall back to CreateDXGIFactory1 if the application calls it
	// This needs to happen because some applications only check if CreateDXGIFactory2 exists, which is always the case if they load ReShade, to decide whether to call it or CreateDXGIFactory1
	if (trampoline == nullptr)
	{
		LOG(INFO) << "> Passing on to " << "CreateDXGIFactory1" << ':';

		return CreateDXGIFactory1(riid, ppFactory);
	}

	const HRESULT hr = trampoline(Flags, riid, ppFactory);
	if (FAILED(hr))
	{
		LOG(WARN) << "CreateDXGIFactory2" << " failed with error code " << hr << '.';
		return hr;
	}

	IDXGIFactory *const factory = static_cast<IDXGIFactory *>(*ppFactory);

	reshade::hooks::install("IDXGIFactory::CreateSwapChain", vtable_from_instance(factory), 10, reinterpret_cast<reshade::hook::address>(IDXGIFactory_CreateSwapChain));

	if (com_ptr<IDXGIFactory2> factory2; SUCCEEDED(factory->QueryInterface(&factory2)))
	{
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForHwnd", vtable_from_instance(factory2.get()), 15, reinterpret_cast<reshade::hook::address>(IDXGIFactory2_CreateSwapChainForHwnd));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForCoreWindow", vtable_from_instance(factory2.get()), 16, reinterpret_cast<reshade::hook::address>(IDXGIFactory2_CreateSwapChainForCoreWindow));
		reshade::hooks::install("IDXGIFactory2::CreateSwapChainForComposition", vtable_from_instance(factory2.get()), 24, reinterpret_cast<reshade::hook::address>(IDXGIFactory2_CreateSwapChainForComposition));
	}

#if RESHADE_VERBOSE_LOG
	LOG(INFO) << "Returning IDXGIFactory object " << factory << '.';
#endif
	return hr;
}

HOOK_EXPORT HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void **pDebug)
{
	const auto trampoline = reshade::hooks::call(DXGIGetDebugInterface1);

	// DXGIGetDebugInterface1 is not available on Windows 7, so act as if Windows SDK is not installed
	if (trampoline == nullptr)
		return E_NOINTERFACE;

	return trampoline(Flags, riid, pDebug);
}

HOOK_EXPORT HRESULT WINAPI DXGIDeclareAdapterRemovalSupport()
{
	const auto trampoline = reshade::hooks::call(DXGIDeclareAdapterRemovalSupport);

	// DXGIDeclareAdapterRemovalSupport is supported on Windows 10 version 1803 and up, silently ignore on older systems
	if (trampoline == nullptr)
		return S_OK;

	return trampoline();
}
