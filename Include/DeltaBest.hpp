/*
rF2 Delta Best Plugin

Author: Cosimo Streppone <cosimo@streppone.it>
Date:   April/May 2014
URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/

#ifndef _INTERNALS_EXAMPLE_H
#define _INTERNALS_EXAMPLE_H

#include "InternalsPlugin.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>

#define PLUGIN_NAME             "rF2 Delta Best - 2017.02.25"
#define DELTA_BEST_VERSION      "v24/Nola"
#define LINUX 
#undef LINUX

#if _WIN64
  #define LOG_FILE              "Bin64\\Plugins\\DeltaBest.log"
#else
  #define LOG_FILE              "Bin32\\Plugins\\DeltaBest.log"

#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

typedef HRESULT (WINAPI *pD3DCompile)
    (LPCVOID                         pSrcData,
     SIZE_T                          SrcDataSize,
     LPCSTR                          pFileName,
     CONST D3D_SHADER_MACRO*         pDefines,
     ID3DInclude*                    pInclude,
     LPCSTR                          pEntrypoint,
     LPCSTR                          pTarget,
     UINT                            Flags1,
     UINT                            Flags2,
     ID3DBlob**                      ppCode,
     ID3DBlob**                      ppErrorMsgs);


#define PRESENT_INDEX 8
//16 because we are 64bit
#define PRESENT_JUMP_LENGTH 16 

__declspec(align(16))
struct cbViewport{
    float width;
    float height;
};

__declspec(align(16))
struct cbLights{
    float color;
	float count;
};

class DeltaBestPlugin : public InternalsPluginV06
{

public:

    // Constructor/destructor
    DeltaBestPlugin() {}
    ~DeltaBestPlugin();


    void EnterRealtime();
    void ExitRealtime();

    void Load();                   // when a new track/car is loaded


    bool WantsToDisplayMessage(MessageInfoV01 &msgInfo);
    bool WantsScoringUpdates() { return( true ); }      // whether we want scoring updates
    void UpdateScoring( const ScoringInfoV01 &info );  // update plugin with scoring info (approximately five times per second)

private:
    void WriteLog(const char * const msg);
    void hexDump (char *desc, void *addr, int len);
    const unsigned int DisasmLengthCheck(const SIZE_T address, const unsigned int jumplength);
    const DWORD DisasmRecalculateOffset(const SIZE_T srcaddress, const SIZE_T detourAddress);

    IDXGISwapChain* getDX11SwapChain();
    void CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain, HWND hwnd);
    void CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext);
    void CreateInvisibleWindow(HWND* hwnd);
    void* findSwapChainInstance(void* pvReplica, DWORD dwVTable);
    void* placeDetour(BYTE* src, BYTE* dest);

    void InitPipeline();
};
#endif // _INTERNALS_EXAMPLE_H
