/*
rF2 Delta Best Plugin

Author: Cosimo Streppone <cosimo@streppone.it>
Date:   April/May 2014
URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/


#include "DeltaBest.hpp"
#include <d3d11.h>
#include <cstdio>

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

ID3D11Device* g_d3dDevice = NULL;
HMODULE g_d3d11 = NULL;
LPD3D11CREATEDEVICE g_DynamicD3D11CreateDevice;

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

	g_d3d11 = LoadLibraryW(L"d3d11.dll" );
	if( g_d3d11 != NULL )
    {
		g_DynamicD3D11CreateDevice = ( LPD3D11CREATEDEVICE )GetProcAddress( g_d3d11, "D3D11CreateDevice" );
		if(g_DynamicD3D11CreateDevice)
			WriteLog("Dynamic D11 create successfull");
    }

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
	g_d3dDevice = getDX11Device();
	if(g_d3dDevice)
		WriteLog("Found DX11 device");
	else
		WriteLog("NOT FOUND");
	g_d3dDevice->GetFeatureLevel();
	/*HRESULT hr;
	const D3D_DRIVER_TYPE devTypeArray[] =
    {
        D3D_DRIVER_TYPE_HARDWARE
    };
	const UINT devTypeArrayCount = sizeof( devTypeArray ) / sizeof( devTypeArray[0] );
	for( UINT iDeviceType = 0; iDeviceType < devTypeArrayCount; iDeviceType++ ){

		// Fill struct w/ AdapterOrdinal and D3D_DRIVER_TYPE
        //pDeviceInfo->AdapterOrdinal = pAdapterInfo->AdapterOrdinal;
        //pDeviceInfo->DeviceType = devTypeArray[iDeviceType];

        D3D_FEATURE_LEVEL FeatureLevels[] =
        {
					D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1,
                    D3D_FEATURE_LEVEL_10_0,
                    D3D_FEATURE_LEVEL_9_3,
                    D3D_FEATURE_LEVEL_9_2,
                    D3D_FEATURE_LEVEL_9_1
        };
        UINT NumFeatureLevels = ARRAYSIZE( FeatureLevels );

        // Call D3D11CreateDevice to ensure that this is a D3D11 device.
        ID3D11Device* pd3dDevice = NULL;
        ID3D11DeviceContext* pd3dDeviceContext = NULL;
        IDXGIAdapter* pAdapter = NULL;
		hr = g_DynamicD3D11CreateDevice(pAdapter,                               
							            devTypeArray[iDeviceType],
                                        ( HMODULE )0,
                                        0,
                                        FeatureLevels,
                                        NumFeatureLevels,
                                        D3D11_SDK_VERSION,
                                        &pd3dDevice,
						                &FeatureLevels[0],
                                        &pd3dDeviceContext );
		if( FAILED( hr )){
            continue;
        }
		
		WriteLog("Got a device!");
		
		IDXGIDevice1* pDXGIDev = NULL;
        hr = pd3dDevice->QueryInterface( __uuidof( IDXGIDevice1 ), ( LPVOID* )&pDXGIDev );
        if( SUCCEEDED( hr ) && pDXGIDev )
        {
            pDXGIDev->GetAdapter( &pAdapter );
			WriteLog("Got an Adapter!");
			SAFE_RELEASE(pAdapter);
	        SAFE_RELEASE(pDXGIDev);
        }

        SAFE_RELEASE( pd3dDevice );
		SAFE_RELEASE( pd3dDeviceContext );  
	}*/
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
		
		if(g_d3dDevice)
			sprintf(msgInfo.mText, "Found DX11 device");
		else
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

ID3D11Device* DeltaBestPlugin::findDev(void* pvReplica, DWORD dwVTable){
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
				if (current == (DWORD*)pvReplica){ WriteLog("Found fake"); continue; } //we don't want to hook the tempSwapChain	
				return ((ID3D11Device*)current);	//If found we hook Present in swapChain		
			}
		}
	}
	return NULL;

}

void DeltaBestPlugin::CreateSearchDevice(ID3D11Device** device){
	HRESULT hr;
	D3D_FEATURE_LEVEL FeatureLevels[] ={
					D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1,
                    D3D_FEATURE_LEVEL_10_0,
                    D3D_FEATURE_LEVEL_9_3,
                    D3D_FEATURE_LEVEL_9_2,
                    D3D_FEATURE_LEVEL_9_1
   };
    UINT NumFeatureLevels = ARRAYSIZE( FeatureLevels );

    // Call D3D11CreateDevice to ensure that this is a D3D11 device.
    ID3D11DeviceContext* pd3dDeviceContext = NULL;
    IDXGIAdapter* pAdapter = NULL;
	hr = g_DynamicD3D11CreateDevice(pAdapter,                               
		    						D3D_DRIVER_TYPE_HARDWARE,
									( HMODULE )0,
									0,
									FeatureLevels,
									NumFeatureLevels,
									D3D11_SDK_VERSION,
									device,
									&FeatureLevels[0],
									&pd3dDeviceContext );
	SAFE_RELEASE(pd3dDeviceContext)
	SAFE_RELEASE(pAdapter)
	
}
 
ID3D11Device* DeltaBestPlugin::getDX11Device(){
	ID3D11Device* pSearchDevice = NULL;
 
	WriteLog("Creating fake device");
	CreateSearchDevice(&pSearchDevice);
	if(pSearchDevice)
		WriteLog("Successfully created device");
	else
		WriteLog("FAILED TO CREATE DEVICE");

	//app.log("Created Search Device! (0x%X, 0x%X)\n", pSearchD3D, pSearchDevice);
 
	ID3D11Device* pDevice = NULL;
 
	DWORD pVtable = *(DWORD*)pSearchDevice;
	//pVtable = (DWORD*)pVtable[0];

	pDevice = (ID3D11Device*) findDev( pSearchDevice, pVtable );
	

	//app.log("Found Device (0x%X)\n", pDevice);
 
	SAFE_RELEASE(pSearchDevice)
 
	return pDevice;
 }