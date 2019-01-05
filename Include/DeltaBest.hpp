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

#undef ENABLE_LOG               /* To enable file logging */

#if _WIN64
  #define LOG_FILE              "Bin64\\Plugins\\DeltaBest.log"
#else
  #define LOG_FILE              "Bin32\\Plugins\\DeltaBest.log"

#endif


#define DATA_PATH_FILE			"Core\\data.path"
#define BEST_LAP_DIR			"%s\\Userdata\\player\\Settings\\DeltaBest"
#define BEST_LAP_FILE			"%s\\%s_%s.lap"

/* Game phases -> info.mGamePhase */
#define GP_GREEN_FLAG           5
#define GP_YELLOW_FLAG		    6
#define GP_SESSION_OVER			8

#define COLOR_INTENSITY         0xF0

#define DEFAULT_FONT_SIZE       48
#define DEFAULT_FONT_NAME       "Arial Black"

#define DEFAULT_BAR_WIDTH       580
#define DEFAULT_BAR_HEIGHT      20
#define DEFAULT_BAR_TOP         130
#define DEFAULT_BAR_TIME_GUTTER 5

#define DEFAULT_TIME_WIDTH      128
#define DEFAULT_TIME_HEIGHT     35

/* Whether to use UpdateTelemetry() to achieve a better precision and
   faster updates to the delta time instead of every 0.2s that
   UpdateScoring() allows */
#define DEFAULT_HIRES_UPDATES   1

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif
typedef BOOL        (WINAPI * LPD3DPERF_QUERYREPEATFRAME)(void);
typedef VOID        (WINAPI * LPD3DPERF_SETOPTIONS)( DWORD dwOptions );
typedef DWORD       (WINAPI * LPD3DPERF_GETSTATUS)( void );
typedef HRESULT     (WINAPI * LPCREATEDXGIFACTORY)(REFIID, void ** );
typedef HRESULT     (WINAPI * LPD3D11CREATEDEVICE)( IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT32, D3D_FEATURE_LEVEL*, UINT, UINT32, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext** );


/* Toggle plugin with CTRL + a magic key. Reference:
http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731%28v=vs.85%29.aspx */
#define DEFAULT_MAGIC_KEY       (0x44)      /* "D" */
#define DEFAULT_RESET_KEY		(0x5A)      /* "Z" */
#define KEY_DOWN(k)             ((GetAsyncKeyState(k) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000))

#define FONT_NAME_MAXLEN 32

// This is used for the app to use the plugin for its intended purpose
class DeltaBestPlugin : public InternalsPluginV06
{

public:

    // Constructor/destructor
    DeltaBestPlugin() {}
    ~DeltaBestPlugin() {}

    // These are the functions derived from base class InternalsPlugin
    // that can be implemented.
    void Startup(long version);    // game startup
    void Shutdown() {};            // game shutdown

    void EnterRealtime();          // entering realtime
    void ExitRealtime();           // exiting realtime

    void StartSession();           // session has started
    void EndSession();             // session has ended

	void Load();                   // when a new track/car is loaded
	void Unload();                 // back to the selection screen

    // GAME OUTPUT
    long WantsTelemetryUpdates() { return 1; }             /* 1 = Player only */
    void UpdateTelemetry(const TelemInfoV01 &info);

    bool WantsGraphicsUpdates() { return true; }
    void UpdateGraphics(const GraphicsInfoV02 &info);

    bool WantsToDisplayMessage(MessageInfoV01 &msgInfo);

    // GAME INPUT
    bool HasHardwareInputs() { return false; }
    void UpdateHardware(const float fDT) { mET += fDT; }   // update the hardware with the time between frames
    void EnableHardware() { mEnabled = true; }             // message from game to enable hardware
    void DisableHardware() { mEnabled = false; }           // message from game to disable hardware
	
    // SCORING OUTPUT
    bool WantsScoringUpdates() { return true; }
    void UpdateScoring(const ScoringInfoV01 &info);

private:
    void WriteLog(const char * const msg);
	IDXGISwapChain1* getDX11SwapChain();
    void CreateSearchSwapChain(IDXGISwapChain1** tempSwapChain);
	IDXGISwapChain1* findChain(void* pvReplica, DWORD dwVTable);

    //
    // Current status
    //

    float mET;                          /* needed for the hardware example */
    bool mEnabled;                      /* needed for the hardware example */

};

inline int round(float x) { return int(floor(x + 0.5)); }
#endif // _INTERNALS_EXAMPLE_H