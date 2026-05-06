#include <windows.h>
#include <d3d11.h>

// Forward-declared globals from main.cpp
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern IDXGISwapChain* g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRenderTargetView;

bool CreateRenderTarget();

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    if (!CreateRenderTarget())
        return false;

    return true;
}

void CleanupRenderTarget()
{
    // Ensure device context unbinds all references to swap chain buffers before releasing render target view
    if (g_pd3dDeviceContext) {
        // Unbind from output merger
        ID3D11RenderTargetView* nullViews[1] = { NULL };
        g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        // Clear any other state that may hold references
        g_pd3dDeviceContext->ClearState();
        // Flush to ensure GPU is finished with resources
        g_pd3dDeviceContext->Flush();
    }

    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

bool CreateRenderTarget()
{
    if (!g_pSwapChain || !g_pd3dDevice) return false;
    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr) || pBackBuffer == NULL) return false;
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr) || g_mainRenderTargetView == NULL) return false;

    // Bind the created render target view so draw calls have a valid RTV bound.
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        // Optionally set viewport matching the render target size
        D3D11_TEXTURE2D_DESC desc;
        ID3D11Texture2D* pBB = NULL;
        if (SUCCEEDED(g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBB)) && pBB) {
            pBB->GetDesc(&desc);
            pBB->Release();
            D3D11_VIEWPORT vp;
            vp.Width = (FLOAT)desc.Width;
            vp.Height = (FLOAT)desc.Height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            vp.TopLeftX = 0;
            vp.TopLeftY = 0;
            g_pd3dDeviceContext->RSSetViewports(1, &vp);
        }
    }

    return true;
}

// Provide a main() entry point when the linker expects one (console subsystem). This forwards to WinMain.
int main(int, char**)
{
    // Retrieve WinMain from this translation unit (defined in src/main.cpp)
    extern int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
