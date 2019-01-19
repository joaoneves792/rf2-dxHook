/*
rF2 Delta Best Plugin

Author: Cosimo Streppone <cosimo@streppone.it>
Date:   April/May 2014
URL:    http://isiforums.net/f/showthread.php/19517-Delta-Best-plugin-for-rFactor-2

*/


#include "DeltaBest.hpp"
#include "semaphore_shader.h"
#include <d3d11.h>
//#include <d3dcompiler.h>
#include <dxgi.h>
#include <cstdio>
#include <string>
#include <cmath>

#define BEA_ENGINE_STATIC  /* specify the usage of a static version of BeaEngine */
#define BEA_USE_STDCALL    /* specify the usage of a stdcall version of BeaEngine */
#include <BeaEngine.h>
 
#pragma comment(lib, "BeaEngine_s_d.lib")
//#pragma comment(lib, "BeaEngine64.lib")


#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
//#pragma comment(lib, "d3dcompiler_43.lib")



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


namespace semaphoreDX11{
    FILE* out_file = NULL;

    bool g_realtime = false;
    bool g_messageDisplayed = false;

	bool g_inPits = false;
	bool g_redlights = false;
	unsigned char g_redCount = 4;
	unsigned char g_redActive = 4;

    ID3D11Device* g_tempd3dDevice = NULL;
    ID3D11Device* g_d3dDevice = NULL;
    IDXGISwapChain* g_swapchain = NULL;
    ID3D11DeviceContext* g_context = NULL;
    
    typedef HRESULT(__stdcall *D3D11PresentHook) (IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
    D3D11PresentHook g_oldPresent;
    struct HookContext{
        BYTE original_code[64];
        SIZE_T dst_ptr;
        BYTE near_jmp[5];
    };
    HookContext* presenthook64;

    pD3DCompile pShaderCompiler = NULL;
    ID3D11VertexShader *g_pVS = NULL;
    ID3D11PixelShader *g_pPS;   
    ID3D11InputLayout *g_pLayout;
    ID3D11Buffer *g_pVBuffer;
    ID3D11Buffer *g_pViewportCBuffer;
	ID3D11Buffer *g_pLightColorCBuffer;
	ID3D11RasterizerState* g_rs;

}

using namespace semaphoreDX11;

void draw(){
	static unsigned char active = 4;
	static bool s_yellow = true;
	if(g_realtime){
		D3D11_MAPPED_SUBRESOURCE ms;
		if(g_inPits && !g_redlights && !s_yellow){
			s_yellow = true;
			g_context->Map(g_pLightColorCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
			cbLights* lightsDataPtr = (cbLights*)ms.pData;
			lightsDataPtr->color = 1.0f;
			lightsDataPtr->count = 4.0f;	
			g_context->Unmap(g_pLightColorCBuffer, NULL);
		}
		if(s_yellow && g_redlights){
			s_yellow = false;
			g_context->Map(g_pLightColorCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
			cbLights* lightsDataPtr = (cbLights*)ms.pData;
			lightsDataPtr->color = 0.0f;
			lightsDataPtr->count = 4.0f;	
			g_context->Unmap(g_pLightColorCBuffer, NULL);
		}
		if(g_redActive != g_redCount && g_redActive != active){
			active = g_redActive;
			g_context->Map(g_pLightColorCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
			cbLights* lightsDataPtr = (cbLights*)ms.pData;
			lightsDataPtr->color = 0.0f;
			lightsDataPtr->count = ceilf(((float)g_redActive/(float)g_redCount)*4.0f);
			if(lightsDataPtr->count < 1.0f){
				lightsDataPtr->count = 1.0f;
			}else if(lightsDataPtr->count > 4.0f){
				lightsDataPtr->count = 4.0f;
			}
			g_context->Unmap(g_pLightColorCBuffer, NULL);
		}
	}

	g_context->RSSetState(g_rs);
    g_context->VSSetShader(g_pVS, 0, 0);
    g_context->PSSetShader(g_pPS, 0, 0);
    g_context->IASetInputLayout(g_pLayout);
	g_context->PSSetConstantBuffers( 0, 1, &g_pViewportCBuffer);
    g_context->VSSetConstantBuffers( 0, 1, &g_pViewportCBuffer);
	g_context->PSSetConstantBuffers( 1, 1, &g_pLightColorCBuffer);
    g_context->VSSetConstantBuffers( 1, 1, &g_pLightColorCBuffer);
    UINT stride = sizeof(float)*3;
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, &g_pVBuffer, &stride, &offset);
    g_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
    g_context->Draw(6, 0);
	
}

HRESULT __stdcall hookedPresent(IDXGISwapChain* This, UINT SyncInterval, UINT Flags){
	if(g_realtime && pShaderCompiler && g_pVS && (g_inPits || g_redlights)){
		ID3D11PixelShader* oldPS;
		ID3D11VertexShader* oldVS;
		ID3D11ClassInstance* PSclassInstances[256]; // 256 is max according to PSSetShader documentation
		ID3D11ClassInstance* VSclassInstances[256];
		UINT psCICount = 256;
		UINT vsCICount = 256;
		ID3D11Buffer* oldVertexBuffers;
		UINT oldStrides;
		UINT oldOffsets;
		ID3D11InputLayout* oldLayout;
		D3D11_PRIMITIVE_TOPOLOGY oldTopo;
		ID3D11Buffer* oldPSConstantBuffer0;
		ID3D11Buffer* oldVSConstantBuffer0;
		ID3D11Buffer* oldPSConstantBuffer1;
		ID3D11Buffer* oldVSConstantBuffer1;
		ID3D11RasterizerState* rs;

		//What the hell is all this crap, you may ask?
		//Well rf2 seems to use gets to retrieve resources that were used when drawing the last frame
		//And then attemps to use them, if we dont revert all state back to how we found it, then there will be graphical glitches
		//Evidence of this? I found a pointer to g_PS in a trace of d3d11 calls outside of our hooked Present
		g_context->RSGetState(&rs);
		g_context->PSGetShader(&oldPS, PSclassInstances, &psCICount);
		g_context->VSGetShader(&oldVS, VSclassInstances, &vsCICount);
		g_context->IAGetInputLayout(&oldLayout);
		g_context->PSGetConstantBuffers(0, 1, &oldPSConstantBuffer0);
		g_context->VSGetConstantBuffers(0, 1, &oldVSConstantBuffer0);
		g_context->PSGetConstantBuffers(1, 1, &oldPSConstantBuffer1);
		g_context->VSGetConstantBuffers(1, 1, &oldVSConstantBuffer1);
		g_context->IAGetVertexBuffers(0, 1, &oldVertexBuffers, &oldStrides, &oldOffsets);
		g_context->IAGetPrimitiveTopology(&oldTopo);

		draw();//Actually do our stuff...

		g_context->RSSetState(rs);
		g_context->PSSetShader(oldPS, PSclassInstances, psCICount);
		g_context->VSSetShader(oldVS, VSclassInstances, vsCICount);
		g_context->IASetInputLayout(oldLayout);
		g_context->PSSetConstantBuffers(0, 1, &oldPSConstantBuffer0);
		g_context->VSSetConstantBuffers(0, 1, &oldVSConstantBuffer0);
		g_context->PSSetConstantBuffers(1, 1, &oldPSConstantBuffer1);
		g_context->VSSetConstantBuffers(1, 1, &oldVSConstantBuffer1);
		g_context->IASetVertexBuffers(0, 1, &oldVertexBuffers, &oldStrides, &oldOffsets);
		g_context->IASetPrimitiveTopology(oldTopo);
    }
    return g_oldPresent(This, SyncInterval, Flags);
}


DeltaBestPlugin::~DeltaBestPlugin(){
    SAFE_RELEASE(g_pLayout)
    SAFE_RELEASE(g_pVS)
    SAFE_RELEASE(g_pPS)
    SAFE_RELEASE(g_pVBuffer)
}

void DeltaBestPlugin::WriteLog(const char * const msg)
{
    if (out_file == NULL)
        out_file = fopen(LOG_FILE, "w");

    if (out_file != NULL){
        fprintf(out_file, "%s\n", msg);
        fflush(out_file);
    }
}


void DeltaBestPlugin::Load()
{
    WriteLog("--LOAD--");
    //D3DCompiler is a hot mess, basically there can be different versions of the dll depending on the windows version
    //rFactor2 is currently linked against 43, probably to support Windows 7, but that can change, so we try to find which one the
    //game has already loaded and use that one
#define DLL_COUNT 3
    if(!pShaderCompiler){
        static const char *d3dCompilerNames[] = {"d3dcompiler_43.dll", "d3dcompiler_46.dll", "d3dcompiler_47.dll"};
        HMODULE D3DCompilerModule = NULL;
        size_t i = 0;
        for (; i < DLL_COUNT; ++i){
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, d3dCompilerNames[i], &D3DCompilerModule)){
                break;
            }
        }
        if(D3DCompilerModule){
            WriteLog("Using:");
            WriteLog(d3dCompilerNames[i]);
            pShaderCompiler = (pD3DCompile)GetProcAddress(D3DCompilerModule, "D3DCompile");
        }else{
            WriteLog("Failed to load d3dcompiler");
        }
    }


    //Get a pointer to the SwapChain and Init the pipeline to draw the lights
    if(!g_swapchain){
        g_swapchain = getDX11SwapChain();
        if(g_swapchain){
            WriteLog("Found SwapChain");
            g_swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_d3dDevice);
            if(g_d3dDevice){
                WriteLog("Found DX11 device");
                g_d3dDevice->GetImmediateContext(&g_context);
                if(g_context){
                    WriteLog("Found Context");
                    InitPipeline();
                }
            }
            
        }
    }
}



void DeltaBestPlugin::EnterRealtime()
{
    g_realtime = true;
    WriteLog("---ENTERREALTIME---");

    if(!g_oldPresent){
        //Only hook present when entering realtime to prevent things like the steam overlay from overriding our hook
        DWORD_PTR* pSwapChainVtable = (DWORD_PTR*)g_swapchain;
        pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];
        g_oldPresent = (D3D11PresentHook)placeDetour((BYTE*)pSwapChainVtable[8], (BYTE*)hookedPresent);
        if(g_oldPresent){
            WriteLog("Present Hook successfull");
        }else{
            WriteLog("Unable to hook Present");
        }
    }

    if(!g_d3dDevice || !g_swapchain || !g_context)
        WriteLog("Failed to find dx11 resources");
	
	
}

void DeltaBestPlugin::ExitRealtime()
{
    g_realtime = false;
    //g_messageDisplayed = false;
    WriteLog("---EXITREALTIME---");
}

void DeltaBestPlugin::UpdateScoring( const ScoringInfoV01 &info ){
	long numVehicles = info.mNumVehicles;
    long i;
	for(i = 0; i < numVehicles; i++){
        if(info.mVehicle[i].mIsPlayer){
			g_inPits = info.mVehicle[i].mInPits;
            break;
		}
	}
	if(((info.mGamePhase == 0) && g_inPits) || info.mGamePhase == 4)
		g_redlights = true;
	else
		g_redlights = false;

	if(g_inPits){
		g_redCount = 4;
		g_redActive = 4;
	}else{
		g_redActive = info.mStartLight;
		g_redCount = info.mNumRedLights;
	}

}

bool DeltaBestPlugin::WantsToDisplayMessage( MessageInfoV01 &msgInfo )
{
	if(g_realtime && !g_messageDisplayed){
        
        if(g_d3dDevice && g_swapchain && g_oldPresent && g_pPS)
            sprintf(msgInfo.mText, "Semaphore DX11 OK");
        else
            sprintf(msgInfo.mText, "Semaphore DX11 FAILURE");

        g_messageDisplayed = true;
        return true;
    }
    return false;
}

const DWORD DeltaBestPlugin::DisasmRecalculateOffset(const SIZE_T srcaddress, const SIZE_T detourAddress){
    DISASM disasm;
    memset(&disasm, 0, sizeof(DISASM));
    disasm.EIP = (UIntPtr)srcaddress;
    disasm.Archi = 0x40;
    Disasm(&disasm);
    if(disasm.Instruction.BranchType == JmpType){
        DWORD originalOffset = *((DWORD*) ( ((BYTE*)srcaddress)+1 ));
        fprintf(out_file, "originalOffset: 0x%016x\n", originalOffset);
        DWORD64 hookedFunction = (DWORD64)(srcaddress+originalOffset);
        fprintf(out_file, "hooked pointer: 0x%016x\n", hookedFunction);
        DWORD64 newOffset = (DWORD64)(hookedFunction - detourAddress);
        fprintf(out_file, "new offset: 0x%016x\n", newOffset);
        fflush(out_file);
        return (DWORD)newOffset; //it has to fit in 32bits
    }else{
        return NULL;
    }

}
 
const unsigned int DeltaBestPlugin::DisasmLengthCheck(const SIZE_T address, const unsigned int jumplength){
        DISASM disasm;
        memset(&disasm, 0, sizeof(DISASM));
 
        disasm.EIP = (UIntPtr)address;
        disasm.Archi = 0x40;
 
        unsigned int processed = 0;
        while (processed < jumplength)
        {
            const int len = Disasm(&disasm);
            if (len == UNKNOWN_OPCODE)
            {
                ++disasm.EIP;
            }
            else if(disasm.Instruction.BranchType == JmpType)
            {
                //Bad news, someone got here first, we have a jump....
                WriteLog("Found a jump in function to detour...");
                WriteLog(disasm.CompleteInstr);
                hexDump("Dumping what was found in function...", (void*)address, 64);
                return -1;
            }
            else
            {
                WriteLog(disasm.CompleteInstr);
                processed += len;
                disasm.EIP += len;
            }
        }
 
        return processed;
}

void DeltaBestPlugin::hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    
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

//based on: https://www.unknowncheats.me/forum/d3d-tutorials-and-source/88369-universal-d3d11-hook.html by evolution536
void* DeltaBestPlugin::placeDetour(BYTE* src, BYTE* dest){
#define _PTR_MAX_VALUE 0x7FFFFFFEFFFF
//#define JMPLEN 6
#define JMPLEN 5

#ifdef LINUX
    MEMORY_BASIC_INFORMATION64 mbi = {0};
    for (DWORD* memptr = (DWORD*)0x10000; memptr < (DWORD*)_PTR_MAX_VALUE; memptr = (DWORD*)(mbi.BaseAddress + mbi.RegionSize)){    
        if(!VirtualQuery(reinterpret_cast<LPCVOID>(memptr),reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi),sizeof(MEMORY_BASIC_INFORMATION))) //Iterate memory by using VirtualQuery
            continue;
#else
    MEMORY_BASIC_INFORMATION mbi;
    for (SIZE_T memptr = (SIZE_T)src; memptr > (SIZE_T)src - 0x80000000; memptr = (SIZE_T)mbi.BaseAddress - 1){
        if (!VirtualQuery((LPCVOID)memptr, &mbi, sizeof(mbi)))
                break;
#endif
        if(mbi.State == MEM_FREE){
            if (presenthook64 = (HookContext*)VirtualAlloc((LPVOID)mbi.BaseAddress, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)){
                    break;
            }
        }
    }
 
    // If allocating a memory page failed, the function failed.
    if (!presenthook64)
        return NULL;

    // Save original code and apply detour. The detour code is:
    // push rax
    // movabs rax, 0xCCCCCCCCCCCCCCCC
    // xchg rax, [rsp]
    // ret
    // 1st calculate how much we need to backup of the src function
    // 2nd copy over the first bytes of the src before they are overritten to original_code
    // 3d copy the detour code to original_code
    // 4th copy the address of the src to the detour 0xCCCCCCCCCCCCCCCC
    BYTE detour[] = { 0x50, 0x48, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x87, 0x04, 0x24, 0xC3 };
    const int length = DisasmLengthCheck((SIZE_T)src, PRESENT_JUMP_LENGTH);
    if(length < 0){
        WriteLog("Bad news, someone hooked this function already!");
        //So we double hook it!!
        //just place a jump to the guys who already hooked it before (Assuming near jump hook, which is not good on x64...)
        DWORD newOffset = DisasmRecalculateOffset((SIZE_T)src, (SIZE_T)presenthook64->original_code);
        presenthook64->original_code[0] = 0xE9;
        memcpy(&presenthook64->original_code[1], &newOffset, sizeof(DWORD));
    }else{
        memcpy(presenthook64->original_code, src, length);
        memcpy(&presenthook64->original_code[length], detour, sizeof(detour));
        *(SIZE_T*)&presenthook64->original_code[length + 3] = (SIZE_T)src + length;
    }

    // Build a far jump to the destination function.
    //*(WORD*)&presenthook64->far_jmp = 0x25FF;
    //*(DWORD*)(presenthook64->far_jmp + 2) = (DWORD)((SIZE_T)presenthook64 - (SIZE_T)src + FIELD_OFFSET(HookContext, dst_ptr) - 6);
    //presenthook64->dst_ptr = (SIZE_T)dest;
    *(WORD*)&presenthook64->near_jmp = 0xE9;    
    *(DWORD*)(presenthook64->near_jmp + 1) = (DWORD)((SIZE_T)dest - (SIZE_T)src - JMPLEN);

     
    // Write the far/near jump code to the src function.
    DWORD flOld = 0;
    VirtualProtect(src, JMPLEN, PAGE_EXECUTE_READWRITE, &flOld);
    memcpy(src, presenthook64->near_jmp, JMPLEN);
    VirtualProtect(src, JMPLEN, flOld, &flOld);
    
    // Return a pointer to the code that will jump (using detour) to the src function
    return presenthook64->original_code;
}


//based on https://www.unknowncheats.me/forum/d3d-tutorials-source/121840-hook-directx-11-dynamically.html by smallC
void* DeltaBestPlugin::findSwapChainInstance(void* pvReplica, DWORD dwVTable){
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
 
        DWORD* len = (DWORD*)(mbi.BaseAddress + mbi.RegionSize);     //Do once
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
                __try{
                    IDXGISwapChain* sc = (IDXGISwapChain*)current;
                    ID3D11Device* dev = NULL;
                    sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev);
                }__except(1){
                    WriteLog("Found a bad pointer");
                    continue;
                }
                return ((void*)current);    
            }
        }
    }
    return NULL;

}

void DeltaBestPlugin::CreateInvisibleWindow(HWND* hwnd){
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"FY";
    RegisterClassExW(&wc);
    *hwnd = CreateWindowExW(0,
                             L"FY",
                             L"null",
                             WS_OVERLAPPEDWINDOW,
                             300, 300,
                             640, 480,
                             nullptr,
                             nullptr,
                             nullptr,
                             nullptr);
}

void DeltaBestPlugin::CreateSearchDevice(ID3D11Device** pDevice, ID3D11DeviceContext** pContext){
    HRESULT hr;

    ID3D11DeviceContext* pd3dDeviceContext = NULL;
    hr = D3D11CreateDevice(NULL,                               
                            D3D_DRIVER_TYPE_HARDWARE,
                            ( HMODULE )0,
                            0,
                            NULL,
                            0,
                            D3D11_SDK_VERSION,
                            pDevice,
                            NULL,
                            pContext);

    if(*pDevice)
        WriteLog("Created device OK");
    else{
        WriteLog("Failed to create device");
    }
    
}

void DeltaBestPlugin::CreateSearchSwapChain(ID3D11Device* device, IDXGISwapChain** tempSwapChain, HWND hwnd){
    HRESULT hr;

    IDXGIFactory1 * pIDXGIFactory1 = NULL;
    CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pIDXGIFactory1);
    if(pIDXGIFactory1)
        WriteLog("Create Factory OK");
    else{
        WriteLog("Failed to create factory1");
        return;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE; //(GetWindowLong(hWnd, GWL_STYLE) & WS_POPUP) != 0 ? FALSE : TRUE;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    hr = pIDXGIFactory1->CreateSwapChain(device, &swapChainDesc, tempSwapChain);
        

    if(*tempSwapChain){
        WriteLog("Created SwapChain OK");
    }else{
        WriteLog("Failed to create SwapChain");
    }
    
    SAFE_RELEASE(pIDXGIFactory1)
}
 
IDXGISwapChain* DeltaBestPlugin::getDX11SwapChain(){
    IDXGISwapChain* pSearchSwapChain = NULL;    
    ID3D11Device* pDevice = NULL;
    ID3D11DeviceContext* pContext = NULL;
    
    WriteLog("Creating D3D11 Device");
    CreateSearchDevice(&pDevice, &pContext);
    if(!pDevice)
        return NULL;

    WriteLog("Getting window handle");
    HWND hWnd;
    CreateInvisibleWindow(&hWnd);
    if (hWnd == NULL){
        WriteLog("Failed to get HWND");
        return NULL;
    }

    WriteLog("Creating fake swapChain");
    CreateSearchSwapChain(pDevice, &pSearchSwapChain, hWnd);
    if(pSearchSwapChain)
        WriteLog("Successfully created swap chain");
    else{
        WriteLog("FAILED TO CREATE SWAP CHAIN");
        return NULL;
    }
    IDXGISwapChain* pSwapChain = NULL;
    DWORD pVtable = *(DWORD*)pSearchSwapChain;
    pSwapChain = (IDXGISwapChain*) findSwapChainInstance( pSearchSwapChain, pVtable );
    if(pSwapChain)
        WriteLog("Found Swapchain");
    else
        WriteLog("Unable to find Swapchain");

    SAFE_RELEASE(pSearchSwapChain)    
    SAFE_RELEASE(pContext)
    SAFE_RELEASE(pDevice)

    return pSwapChain;
 }


void DeltaBestPlugin::InitPipeline(){
    ID3D10Blob *VS, *PS;
    ID3D10Blob *pErrors;
    std::string shader = std::string(semaphore_shader);
    
    if(!pShaderCompiler)
        return;

    WriteLog("Compiling shader");
    (pShaderCompiler)((LPCVOID) shader.c_str(), shader.length(), NULL, NULL, NULL, "VShader", "vs_4_0", 0, 0, &VS, &pErrors);
    if(pErrors)
        WriteLog(static_cast<char*>(pErrors->GetBufferPointer()));

    (pShaderCompiler)((LPCVOID) shader.c_str(), shader.length(), NULL, NULL, NULL, "PShader", "ps_4_0", 0, 0, &PS, &pErrors);
    if(pErrors)
        WriteLog(static_cast<char*>(pErrors->GetBufferPointer()));

    if(pErrors)
        return;
    
    g_d3dDevice->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &g_pVS);
    g_d3dDevice->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &g_pPS);



    WriteLog("Creating buffers");
    D3D11_BUFFER_DESC vbd, viewport_cbd, color_cbd;
    ZeroMemory(&vbd, sizeof(vbd));
    vbd.Usage = D3D11_USAGE_DYNAMIC;                // write access access by CPU and GPU
    vbd.ByteWidth = sizeof(float) * 3 * 4;          // size is the VERTEX struct * 3
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // use as a vertex buffer
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // allow CPU to write in buffer
    g_d3dDevice->CreateBuffer(&vbd, NULL, &g_pVBuffer);       // create the buffer

	ZeroMemory(&viewport_cbd, sizeof(viewport_cbd));
	viewport_cbd.Usage = D3D11_USAGE_DYNAMIC;                
	viewport_cbd.ByteWidth = sizeof(cbViewport);
    viewport_cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;      
    viewport_cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_d3dDevice->CreateBuffer(&viewport_cbd, NULL, &g_pViewportCBuffer);

	ZeroMemory(&color_cbd, sizeof(color_cbd));
	color_cbd.Usage = D3D11_USAGE_DYNAMIC;                
    color_cbd.ByteWidth = sizeof(cbLights);
    color_cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;      
    color_cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_d3dDevice->CreateBuffer(&color_cbd, NULL, &g_pLightColorCBuffer);


    float quad_vertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
        
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };


    WriteLog("Filling buffers");
    D3D11_MAPPED_SUBRESOURCE ms;
    g_context->Map(g_pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);   // map the buffer
    memcpy(ms.pData, quad_vertices, sizeof(quad_vertices));                // copy the data
    g_context->Unmap(g_pVBuffer, NULL);

    D3D11_INPUT_ELEMENT_DESC ied[] ={
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    WriteLog("Setting layout of shader");
    g_d3dDevice->CreateInputLayout(ied, 1, VS->GetBufferPointer(), VS->GetBufferSize(), &g_pLayout);

    WriteLog("Mapping Constant Buffer info");
    DXGI_SWAP_CHAIN_DESC pDesc;
	g_swapchain->GetDesc(&pDesc);
    g_context->Map(g_pViewportCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
	cbViewport* viewportDataPtr = (cbViewport*)ms.pData;
	viewportDataPtr->width = (float)(pDesc.BufferDesc.Width);
	viewportDataPtr->height = (float)(pDesc.BufferDesc.Height);
	g_context->Unmap(g_pViewportCBuffer, NULL);
	g_context->Map(g_pLightColorCBuffer, NULL, D3D11_MAP_WRITE_DISCARD,  NULL, &ms);
	cbLights* lightsDataPtr = (cbLights*)ms.pData;
	lightsDataPtr->color = 0.0f;
	lightsDataPtr->count = 4.0f;	
    g_context->Unmap(g_pLightColorCBuffer, NULL);

	WriteLog("Creating rasterizer state");
	D3D11_RASTERIZER_DESC rsDesc;
	ZeroMemory(&rsDesc, sizeof(D3D11_RASTERIZER_DESC));
	rsDesc.CullMode = D3D11_CULL_NONE;
	rsDesc.FillMode = D3D11_FILL_SOLID;

	g_d3dDevice->CreateRasterizerState(&rsDesc, &g_rs);
}