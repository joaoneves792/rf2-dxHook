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
#define DXVK 1

#if _WIN64
  #define LOG_FILE              "Bin64\\Plugins\\DeltaBest.log"
#else
  #define LOG_FILE              "Bin32\\Plugins\\DeltaBest.log"

#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#define PRESENT_INDEX 8
//16 because we are 64bit
#define PRESENT_JUMP_LENGTH 16 

class DeltaBestPlugin : public InternalsPluginV06
{

public:

    // Constructor/destructor
    DeltaBestPlugin() {}
    ~DeltaBestPlugin() {}


    void EnterRealtime();
	void ExitRealtime();

	void Load();                   // when a new track/car is loaded


    bool WantsToDisplayMessage(MessageInfoV01 &msgInfo);

private:
    void WriteLog(const char * const msg);
	IDXGISwapChain* getDX11SwapChain();
    void CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain, HWND hwnd);
	void CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext);
	void CreateInvisibleWindow(HWND* hwnd);
	void* findInstance(void* pvReplica, DWORD dwVTable);
	void* hookVMT(BYTE* src, BYTE* dest);

    //
    // Current status
    //

    float mET;                          /* needed for the hardware example */
    bool mEnabled;                      /* needed for the hardware example */

};

inline int round(float x) { return int(floor(x + 0.5)); }
#endif // _INTERNALS_EXAMPLE_H