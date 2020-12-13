#ifndef __D3D_DEVICE_H__
#define __D3D_DEVICE_H__



// struct for chromacities for HDR metadata
struct DisplayChromaticities
{
    float RedX;
    float RedY;
    float GreenX;
    float GreenY;
    float BlueX;
    float BlueY;
    float WhiteX;
    float WhiteY;
};

bool CheckHDRCapability(int left, int top, int right, int bottom);
bool DeviceStarted();
QD3D11Device* InitDevice(); // release when done
void DestroyDevice();
void InitSwapChain( IDXGISwapChain1* swapChain ); // adds ref
void DestroySwapChain();

void GetSwapChainDescFromConfig( DXGI_SWAP_CHAIN_DESC1* desc );

void CreateBuffers();
void DestroyBuffers();

#endif
