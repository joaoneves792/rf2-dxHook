/*
rF2 Delta Best Plugin

Author: Cosimo Streppone <cosimo@streppone.it>
Date:   April/May 2014
URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/


#include "DeltaBest.hpp"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <cstdio>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")


// plugin information

extern "C" __declspec(dllexport)
	const char * __cdecl GetPluginName()                   { return PLUGIN_NAME; }

extern "C" __declspec(dllexport)
	PluginObjectType __cdecl GetPluginType()               { return PO_INTERNALS; }

extern "C" __declspec(dllexport)
	int __cdecl GetPluginVersion()                         { return 6; }

extern "C" __declspec(dllexport)
	PluginObject * __cdecl CreatePluginObject()            { return((PluginObject *) new DeltaBestPlugin); }

extern "C" __declspec(dllexport)
	void __cdecl DestroyPluginObject(PluginObject *obj)    { delete((DeltaBestPlugin *)obj); }

#define ENABLE_LOG 1


#ifdef ENABLE_LOG
FILE* out_file = NULL;
#endif

bool g_realtime = false;
bool g_messageDisplayed = false;

HWND g_HWND = NULL;

ID3D11Device* g_tempd3dDevice = NULL;
ID3D11Device* g_d3dDevice = NULL;
IDXGISwapChain* g_swapchain = NULL;
ID3D11DeviceContext* g_context = NULL;

/*
// DirectX 9 objects, to render some text on screen
LPD3DXFONT g_Font = NULL;
D3DXFONT_DESC FontDesc = {
	DEFAULT_FONT_SIZE, 0, 400, 0, false, DEFAULT_CHARSET,
	OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_PITCH, DEFAULT_FONT_NAME
};
RECT FontPosition, ShadowPosition;
LPD3DXSPRITE bar = NULL;
LPDIRECT3DTEXTURE9 texture = NULL;
*/
//
// DeltaBestPlugin class
//

void DeltaBestPlugin::WriteLog(const char * const msg)
{
#ifdef ENABLE_LOG
	if (out_file == NULL)
		out_file = fopen(LOG_FILE, "a");

	if (out_file != NULL){
		fprintf(out_file, "%s\n", msg);
		fflush(out_file);
	}
#endif 
}

void DeltaBestPlugin::Startup(long version)
{
#ifdef ENABLE_LOG
	WriteLog("--STARTUP--");
#endif 
}

void DeltaBestPlugin::StartSession()
{
#ifdef ENABLE_LOG
	WriteLog("--STARTSESSION--");
#endif 
}

void DeltaBestPlugin::EndSession()
{
#ifdef ENABLE_LOG
	WriteLog("--ENDSESSION--");
	if (out_file) {
		fclose(out_file);
		out_file = NULL;
	}

#endif 
}

void DeltaBestPlugin::Load()
{
#ifdef ENABLE_LOG
	WriteLog("--LOAD--");
#endif 

}

void DeltaBestPlugin::Unload()
{
#ifdef ENABLE_LOG
	WriteLog("--UNLOAD--");

#endif 
}

void DeltaBestPlugin::EnterRealtime()
{
	g_realtime = true;
#ifdef ENABLE_LOG
	WriteLog("---ENTERREALTIME---");
#endif 
	g_swapchain = getDX11SwapChain();
	if(g_swapchain){
		WriteLog("Found SwapChain");	
		g_swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_d3dDevice);
		if(g_d3dDevice){
			WriteLog("Found DX11 device");
			g_d3dDevice->GetImmediateContext(&g_context);
			if(g_context){
				WriteLog("Found Context");

				//g_swapchain->Present(0, 0);
			}
		}
	}
	if(!g_d3dDevice || !g_swapchain || !g_context)
		WriteLog("Failed to find dx11 resources");
	
}

void DeltaBestPlugin::ExitRealtime()
{
	g_realtime = false;
	//g_messageDisplayed = false;
#ifdef ENABLE_LOG
	WriteLog("---EXITREALTIME---");
#endif 
}

bool DeltaBestPlugin::WantsToDisplayMessage( MessageInfoV01 &msgInfo )
{
	if(g_realtime && !g_messageDisplayed){
		
		if(g_d3dDevice && g_swapchain){
			sprintf(msgInfo.mText, "Found Swap Chain");
		}else
			sprintf(msgInfo.mText, "NOT FOUND");

		g_messageDisplayed = true;
		return true;
	}
	return false;
}


void DeltaBestPlugin::UpdateTelemetry(const TelemInfoV01 &info){
	return;
}

void DeltaBestPlugin::UpdateScoring(const ScoringInfoV01 &info){
	return;
}

void DeltaBestPlugin::UpdateGraphics(const GraphicsInfoV02 &info){
	if(!g_HWND)
		g_HWND = info.mHWND;
}

void* DeltaBestPlugin::findInstance(void* pvReplica, DWORD dwVTable){
#ifdef _AMD64_
#define _PTR_MAX_VALUE 0x7FFFFFFEFFFF
MEMORY_BASIC_INFORMATION64 mbi = { 0 };
#else
#define _PTR_MAX_VALUE 0xFFE00000
MEMORY_BASIC_INFORMATION32 mbi = { 0 };
#endif
	
	for (DWORD* memptr = (DWORD*)0x10000; memptr < (DWORD*)_PTR_MAX_VALUE; memptr = (DWORD*)(mbi.BaseAddress + mbi.RegionSize)) //For x64 -> 0x10000 ->  0x7FFFFFFEFFFF
	{
		if (!VirtualQuery(reinterpret_cast<LPCVOID>(memptr),reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi),sizeof(MEMORY_BASIC_INFORMATION))) //Iterate memory by using VirtualQuery
			continue;
 
		if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS || mbi.Protect & PAGE_GUARD) //Filter Regions
			continue;
 
		DWORD* len = (DWORD*)(mbi.BaseAddress + mbi.RegionSize);	 //Do once
		DWORD dwVTableAddress;
		for (DWORD* current = (DWORD*)mbi.BaseAddress; current < len; ++current){								
			__try
 				{
 				    dwVTableAddress  = *(DWORD*)current;
 				}
 				__except (1)
 				{
  				continue;
				}
			
					
			if (dwVTableAddress == dwVTable)
			{
				if (current == (DWORD*)pvReplica){ WriteLog("Found fake"); continue; }	
				return ((void*)current);	
			}
		}
	}
	return NULL;

}

  void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    FILE* out_file = fopen("/home/joao/Desktop/wtf.log", "a");


    // Output description if given.
    if (desc != NULL)
      fprintf(out_file, "%s:\n", desc);

    if (len == 0) {
      fprintf(out_file, "  ZERO LENGTH\n");
      return;
    }
    if (len < 0) {
      fprintf(out_file, "  NEGATIVE LENGTH: %i\n",len);
      return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
      // Multiple of 16 means new line (with line offset).

      if ((i % 16) == 0) {
        // Just don't print ASCII for the zeroth line.
        if (i != 0)
          fprintf(out_file, "  %s\n", buff);

        // Output the offset.
        fprintf(out_file, "  %04x ", i);
      }

      // Now the hex code for the specific character.
      fprintf(out_file, " %02x", pc[i]);

      // And store a printable ASCII character for later.
      if ((pc[i] < 0x20) || (pc[i] > 0x7e))
        buff[i % 16] = '.';
      else
        buff[i % 16] = pc[i];
      buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
      fprintf(out_file, "   ");
      i++;
    }

    // And print the final ASCII bit.
    fprintf(out_file, "  %s\n", buff);
    fflush(out_file);
  }

void DeltaBestPlugin::CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext){
	HRESULT hr;
	D3D_FEATURE_LEVEL FeatureLevels[] ={
                    D3D_FEATURE_LEVEL_11_0,
	};
    UINT NumFeatureLevels = ARRAYSIZE( FeatureLevels );

    ID3D11DeviceContext* pd3dDeviceContext = NULL;
	hr = D3D11CreateDevice(NULL,                               
		    				D3D_DRIVER_TYPE_HARDWARE,
							( HMODULE )0,
							0,
							FeatureLevels,
							NumFeatureLevels,
							D3D11_SDK_VERSION,
							pDevice,
							&FeatureLevels[0],
							pContext);

	if(*pDevice)
		WriteLog("Created device OK");
	else{
		WriteLog("Failed to create device");
	}
	
}

void DeltaBestPlugin::CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain){
	HRESULT hr;

	IDXGIFactory1 * pIDXGIFactory1 = NULL;
	CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pIDXGIFactory1);
	if(pIDXGIFactory1)
		WriteLog("Create Factory OK");
	else{
		WriteLog("Failed to create factory1");
		return;
	}

	HWND hWnd = GetForegroundWindow();
	if ( hWnd == nullptr ){
		WriteLog("Failed to get HWND");
		return;
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE; //(GetWindowLong(hWnd, GWL_STYLE) & WS_POPUP) != 0 ? FALSE : TRUE;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	
	hexDump("Swap Chain Desc", &swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

	hr = pIDXGIFactory1->CreateSwapChain(device, &swapChainDesc, tempSwapChain);
		

	if(*tempSwapChain){
		WriteLog("Created SwapChain OK");
	}else{
		WriteLog("Failed to create SwapChain");
		fprintf(out_file, "HR: %d\n", hr);
		fflush(out_file);
	}
	

	SAFE_RELEASE(pIDXGIFactory1)
}
 
IDXGISwapChain* DeltaBestPlugin::getDX11SwapChain(){
	IDXGISwapChain* pSearchSwapChain = NULL;	
	ID3D11Device* pDevice = NULL;
	ID3D11DeviceContext* pContext = NULL;
	CreateSearchDevice(&pDevice, &pContext);
	WriteLog("Creating fake swapChain");
	CreateSearchSwapChain(pDevice, &pSearchSwapChain);
	if(pSearchSwapChain)
		WriteLog("Successfully created swap chain");
	else{
		WriteLog("FAILED TO CREATE SWAP CHAIN");
		return NULL;
	}
	IDXGISwapChain* pSwapChain = NULL;
 	DWORD pVtable = *(DWORD*)pSearchSwapChain;
	pSwapChain = (IDXGISwapChain*) findInstance( pSearchSwapChain, pVtable );
	
	SAFE_RELEASE(pSearchSwapChain)	
	SAFE_RELEASE(pContext)
	SAFE_RELEASE(pDevice)

	return pSwapChain;
 }