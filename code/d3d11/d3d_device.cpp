#include "d3d_common.h"
#include "d3d_device.h"
#include "d3d_state.h"

QD3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext1* g_pImmediateContext = nullptr;
IDXGISwapChain1* g_pSwapChain = nullptr;
IDXGISwapChain4* g_pSwapChain4 = nullptr;

// Compute intersection of rectangles
inline int ComputeIntersectionArea(int ax1, int ay1, int ax2, int ay2, int bx1, int by1, int bx2, int by2)
{
    return max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1));
}


// new function to check if the current display is HDR capable 
bool CheckHDRCapability( int left, int top, int right, int bottom) {
    // Retrieve the current default adapter.
    IDXGIAdapter1 *dxgiAdapter;
    IDXGIFactory2 *dxgiFactory;

    QD3D::GetDxgiFactory(g_pDevice, &dxgiFactory);

    dxgiFactory->EnumAdapters1(0, &dxgiAdapter);

    // Iterate through the DXGI outputs associated with the DXGI adapter,
    // and find the output whose bounds have the greatest overlap with the
    // app window (i.e. the output for which the intersection area is the
    // greatest).

    UINT i = 0;
    IDXGIOutput *currentOutput;
    IDXGIOutput *bestOutput;
    float bestIntersectArea = -1;

    while (dxgiAdapter->EnumOutputs(i, &currentOutput) != DXGI_ERROR_NOT_FOUND)
    {
        // Get the retangle bounds of the app window
        int ax1 = left;
        int ay1 = top;
        int ax2 = right;
        int ay2 = bottom;

        // Get the rectangle bounds of current output
        DXGI_OUTPUT_DESC desc;
        currentOutput->GetDesc(&desc);
        RECT r = desc.DesktopCoordinates;
        int bx1 = r.left;
        int by1 = r.top;
        int bx2 = r.right;
        int by2 = r.bottom;

        // Compute the intersection
        int intersectArea = ComputeIntersectionArea(ax1, ay1, ax2, ay2, bx1, by1, bx2, by2);
        if (intersectArea > bestIntersectArea)
        {
            bestOutput = currentOutput;
            bestIntersectArea = static_cast<float>(intersectArea);
        }

        i++;
    }

    // Having determined the output (display) upon which the app is primarily being 
    // rendered, retrieve the HDR capabilities of that display by checking the color space.
    IDXGIOutput6 *output6;

    HRESULT hr = S_OK;
    // get Output6 interface

    // using __uuidof because apparently the lib in the current Windows 10 SDK version doesn't have the correct header in its build!!? WTF?
    // possible danger that bestOutput hasn't been allocated?
    hr = bestOutput->QueryInterface(__uuidof(IDXGIOutput6), (LPVOID*)&output6);

    DXGI_OUTPUT_DESC1 desc1;
    output6->GetDesc1(&desc1);

    // get the HDR colour space support from the output desc
    bool hdrCapable = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);

    SAFE_RELEASE(dxgiAdapter);
    SAFE_RELEASE(dxgiFactory);
    SAFE_RELEASE(currentOutput);
    SAFE_RELEASE(bestOutput);


    // return if the colour space from the current display supports HDR
    return hdrCapable;

}

// Set HDR meta data for output display to master the content and the luminance values of the content
void SetHDRMetaData(float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
{

    // Display chromacities copied from MS DX12 HDR example
    static const DisplayChromaticities DisplayChromaticityList[] =
    {
        { 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // Display Gamut Rec709 
        { 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // Display Gamut Rec2020
    };



    // Select the chromaticity based on the selected HDR format
    cvar_t* hdrFormat = Cvar_Get("r_hdrformat", "1", 0);

    int selectedChroma = hdrFormat->integer;


    // Set HDR meta data
    const DisplayChromaticities& Chroma = DisplayChromaticityList[selectedChroma];
    DXGI_HDR_METADATA_HDR10 HDR10MetaData = {};
    HDR10MetaData.RedPrimary[0] = static_cast<UINT16>(Chroma.RedX * 50000.0f);
    HDR10MetaData.RedPrimary[1] = static_cast<UINT16>(Chroma.RedY * 50000.0f);
    HDR10MetaData.GreenPrimary[0] = static_cast<UINT16>(Chroma.GreenX * 50000.0f);
    HDR10MetaData.GreenPrimary[1] = static_cast<UINT16>(Chroma.GreenY * 50000.0f);
    HDR10MetaData.BluePrimary[0] = static_cast<UINT16>(Chroma.BlueX * 50000.0f);
    HDR10MetaData.BluePrimary[1] = static_cast<UINT16>(Chroma.BlueY * 50000.0f);
    HDR10MetaData.WhitePoint[0] = static_cast<UINT16>(Chroma.WhiteX * 50000.0f);
    HDR10MetaData.WhitePoint[1] = static_cast<UINT16>(Chroma.WhiteY * 50000.0f);
    HDR10MetaData.MaxMasteringLuminance = static_cast<UINT>(MaxOutputNits * 10000.0f);
    HDR10MetaData.MinMasteringLuminance = static_cast<UINT>(MinOutputNits * 10000.0f);
    HDR10MetaData.MaxContentLightLevel = static_cast<UINT16>(MaxCLL);
    HDR10MetaData.MaxFrameAverageLightLevel = static_cast<UINT16>(MaxFALL);
    g_pSwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &HDR10MetaData);
}

bool DeviceStarted()
{
    return g_pDevice != nullptr;
}

QD3D11Device* InitDevice()
{
    Com_Memset( &g_BufferState, 0, sizeof( g_BufferState ) );

#ifdef WIN8
    g_BufferState.featureLevel = D3D_FEATURE_LEVEL_9_1;
#else
    g_BufferState.featureLevel = D3D_FEATURE_LEVEL_11_1; 
#endif
	HRESULT hr = QD3D::CreateDefaultDevice(
		D3D_DRIVER_TYPE_HARDWARE, 
		&g_pDevice, 
		&g_pImmediateContext, 
		&g_BufferState.featureLevel);
    if (FAILED(hr) || !g_pDevice || !g_pImmediateContext)
	{
        ri.Error( ERR_FATAL, "Failed to create Direct3D 11 device: 0x%08x.\n", hr );
        return nullptr;
	}

    ri.Printf( PRINT_ALL, "... feature level %d\n", g_BufferState.featureLevel );

    g_pDevice->AddRef();
    return g_pDevice;
}

void InitSwapChain( IDXGISwapChain1* swapChain )
{
    HRESULT hr = S_OK;
    swapChain->AddRef();
    g_pSwapChain = swapChain; 
    g_pSwapChain->GetDesc1( &g_BufferState.swapChainDesc );

    // Create SwapChain4 interface
    hr = g_pSwapChain->QueryInterface(IID_IDXGISwapChain4, (LPVOID*)&g_pSwapChain4);

    // Enable HDR colour space for the swap chain if set to HDR10
    cvar_t* hdrFormat = Cvar_Get("r_hdrformat", "1", 0);

    if (hdrFormat->integer == QD3D_HDR_FORMAT_HDR10) {
        g_pSwapChain4->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    }

    if (FAILED(hr)) {
        exit(1);
    }

    // Set HDR metadata for the display display max brightness, display min brightness, content max brightness and highest average frame brightness
    // Read from CVARs

    cvar_t* displayMax = Cvar_Get("r_maxoutput", "1000.0", 0);
    cvar_t* displayMin = Cvar_Get("r_minoutput", "0.0", 0);
    cvar_t* maxCLL = Cvar_Get("r_maxcll", "1000.0", 0);
    cvar_t* maxFALL = Cvar_Get("r_maxfall", "800.0", 0);

    SetHDRMetaData(displayMax->value, displayMin->value, maxCLL->value, maxFALL->value);

    // Create D3D objects
    DestroyBuffers();
    CreateBuffers();

    // Clear the targets
    FLOAT clearCol[4] = { 0, 0, 0, 0 };
    g_pImmediateContext->ClearRenderTargetView( g_BufferState.backBufferView, clearCol );
    g_pImmediateContext->ClearDepthStencilView( g_BufferState.depthBufferView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0 );

    // Clear the screen immediately
    g_pSwapChain->Present( 0, 0 );
}

void DestroyDevice()
{
    SAFE_RELEASE(g_pImmediateContext);
    SAFE_RELEASE(g_pDevice);
}

void DestroySwapChain()
{
    SAFE_RELEASE(g_pSwapChain);
    SAFE_RELEASE(g_pSwapChain4);
    Com_Memset( &g_BufferState, 0, sizeof( g_BufferState ) );
}

void GetSwapChainDescFromConfig( DXGI_SWAP_CHAIN_DESC1* scDesc )
{
    // Get best possible swapchain first
    QD3D::GetBestQualitySwapChainDesc( g_pDevice, scDesc );

#ifndef _ARM_
    // Clamp the max MSAA to user settings
    cvar_t* d3d_multisamples = Cvar_Get( "d3d_multisamples", "32", CVAR_ARCHIVE | CVAR_LATCH );
    if ( d3d_multisamples->integer > 0 && scDesc->SampleDesc.Count > d3d_multisamples->integer )
        scDesc->SampleDesc.Count = d3d_multisamples->integer;
#endif
}



