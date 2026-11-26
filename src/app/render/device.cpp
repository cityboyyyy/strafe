#include "device.h"

#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace menu::render
{
    namespace
    {
        ComPtr<ID3D11Device> g_device;
        ComPtr<ID3D11DeviceContext> g_context;
        ComPtr<IDXGISwapChain1> g_swap_chain;
        ComPtr<IDCompositionDevice> g_composition;
        ComPtr<IDCompositionTarget> g_target;
        ComPtr<IDCompositionVisual> g_visual;
        ComPtr<ID3D11RenderTargetView> g_back_buffer;
        ComPtr<ID3D11SamplerState> g_mip_sampler;
        bool g_occluded = false;

        void create_back_buffer()
        {
            ComPtr<ID3D11Texture2D> buffer;
            if (SUCCEEDED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&buffer))))
                g_device->CreateRenderTargetView(buffer.Get(), nullptr, &g_back_buffer);
        }
    }

    bool create(HWND hwnd)
    {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        auto make = [&](D3D_DRIVER_TYPE type, UINT device_flags)
        {
            return D3D11CreateDevice(nullptr, type, nullptr, device_flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &g_device, nullptr, &g_context);
        };

        HRESULT hr = make(D3D_DRIVER_TYPE_HARDWARE, flags);
        if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG))
            hr = make(D3D_DRIVER_TYPE_HARDWARE, flags & ~D3D11_CREATE_DEVICE_DEBUG);
        if (FAILED(hr))
            hr = make(D3D_DRIVER_TYPE_WARP, D3D11_CREATE_DEVICE_BGRA_SUPPORT);
        if (FAILED(hr))
            return false;

        ComPtr<IDXGIDevice> dxgi_device;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        if (FAILED(g_device.As(&dxgi_device)) || FAILED(dxgi_device->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))))
            return false;

        RECT client{};
        GetClientRect(hwnd, &client);

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = static_cast<UINT>(client.right - client.left);
        desc.Height = static_cast<UINT>(client.bottom - client.top);
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (FAILED(factory->CreateSwapChainForComposition(g_device.Get(), &desc, nullptr, &g_swap_chain)))
            return false;

        if (FAILED(DCompositionCreateDevice(dxgi_device.Get(), IID_PPV_ARGS(&g_composition))))
            return false;
        if (FAILED(g_composition->CreateTargetForHwnd(hwnd, TRUE, &g_target)))
            return false;
        if (FAILED(g_composition->CreateVisual(&g_visual)))
            return false;
        g_visual->SetContent(g_swap_chain.Get());
        g_target->SetRoot(g_visual.Get());
        if (FAILED(g_composition->Commit()))
            return false;

        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        sampler.MipLODBias = -0.5f;
        if (FAILED(g_device->CreateSamplerState(&sampler, &g_mip_sampler)))
            return false;

        create_back_buffer();
        return g_back_buffer != nullptr;
    }

    void destroy()
    {
        if (g_context)
            g_context->ClearState();
        g_back_buffer.Reset();
        g_mip_sampler.Reset();
        g_visual.Reset();
        g_target.Reset();
        g_composition.Reset();
        g_swap_chain.Reset();
        g_context.Reset();
        g_device.Reset();
    }

    void resize(std::uint32_t width, std::uint32_t height)
    {
        g_back_buffer.Reset();
        g_swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        create_back_buffer();
    }

    bool occluded()
    {
        if (g_occluded && g_swap_chain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
            return true;

        g_occluded = false;
        return false;
    }

    void begin(const float clear[4])
    {
        ID3D11RenderTargetView* target = g_back_buffer.Get();
        g_context->OMSetRenderTargets(1, &target, nullptr);
        g_context->ClearRenderTargetView(target, clear);
    }

    void present()
    {
        g_occluded = g_swap_chain->Present(1, 0) == DXGI_STATUS_OCCLUDED;
    }

    ID3D11Device* device()
    {
        return g_device.Get();
    }

    ID3D11DeviceContext* context()
    {
        return g_context.Get();
    }

    ID3D11SamplerState* mip_sampler()
    {
        return g_mip_sampler.Get();
    }
}
