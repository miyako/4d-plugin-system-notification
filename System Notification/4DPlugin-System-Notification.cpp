/* --------------------------------------------------------------------------------
 #
 #  4DPlugin-System-Notification.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : System Notification
 #	author : miyako
 #	2022/01/20
 #  
 # --------------------------------------------------------------------------------*/

#include "4DPlugin-System-Notification.h"

#pragma mark -

static bool isSDI() {
    
#if VERSIONWIN
    
    PA_Variable args[5];

    args[0] = PA_CreateVariable(eVK_Longint);
    args[1] = PA_CreateVariable(eVK_Longint);
    args[2] = PA_CreateVariable(eVK_Longint);
    args[3] = PA_CreateVariable(eVK_Longint);
    args[4] = PA_CreateVariable(eVK_Longint);
    
    PA_SetLongintVariable(&args[4], (PA_long32)-1);

    PA_ExecuteCommandByID(443 /*GET WINDOW RECT*/, args, 5);

    return (
       (PA_GetLongintVariable(args[0]) == 0)
    && (PA_GetLongintVariable(args[1]) == 0)
    && (PA_GetLongintVariable(args[2]) == 0)
    && (PA_GetLongintVariable(args[3]) == 0));
    
    /* the variable belongs to 4D, no need to PA_ClearVariable (I think) */

#endif
    
    return false;
}

#if VERSIONWIN
static HWND getMDI() {
    
    PA_ulong32 version = PA_Get4DVersion();
    
    if (version >= 16)
        return (HWND)PA_GetMainWindowHWND();

    // Altura MAc2Win does not allow multiple instances of the same app
    // we can assume that the window class is the folder name of the application
    
    HWND mdi = NULL;
    wchar_t path[_MAX_PATH] = { 0 };
    wchar_t * applicationPath = wcscpy(path, (const wchar_t *)PA_GetApplicationFullPath().fString);
    
    //remove file name (4D.exe)
    PathRemoveFileSpec(path);
    //check instance as well, to be sure
    HINSTANCE h = (HINSTANCE)PA_Get4DHInstance();
    
    do {
        mdi = FindWindowEx(NULL, mdi, (LPCTSTR)path, NULL);
        if (mdi)
        {
            if (h == (HINSTANCE)GetWindowLongPtr(mdi, GWLP_HINSTANCE))
            {
                break;
            }
        }
    } while (mdi);
    
   return mdi;
}
#endif

#define CALLBACK_IN_NEW_PROCESS 0
#define CALLBACK_SLEEP_TIME 59

std::mutex globalMutex; /* for INSERT_RECORDS,UPDATE_RECORDS,DELETE_RECORDS */
std::mutex globalMutex1;/* for METHOD_PROCESS_ID */
std::mutex globalMutex2;/* for LISTENER_METHOD */
std::mutex globalMutex3;/* PROCESS_SHOULD_TERMINATE */
std::mutex globalMutex4;/* PROCESS_SHOULD_RESUME */

namespace SN
{
    //constants
process_name_t MONITOR_PROCESS_NAME = (PA_Unichar *)"$\0S\0L\0E\0E\0P\0_\0N\0O\0T\0I\0F\0I\0C\0A\0T\0I\0O\0N\0\0\0";
    process_stack_size_t MONITOR_PROCESS_STACK_SIZE = 0;
      
    //context management
    event_id_t CALLBACK_EVENT_ID = 0;
    std::vector<event_id_t>CALLBACK_EVENT_IDS;

    //callback management
    C_TEXT LISTENER_METHOD;
    process_number_t METHOD_PROCESS_ID = 0;
    bool PROCESS_SHOULD_TERMINATE = false;
    bool PROCESS_SHOULD_RESUME = false;

#if VERSIONMAC
    SNListener *listener = nil;
#endif
}

#if VERSIONWIN
namespace SN
{
    HWND MDI = NULL;

    WNDPROC originalWndProc = NULL;
    HPOWERNOTIFY notificationHandle = NULL;
    bool first_event_call = false;

    void call(event_id_t event)
    {
        SN::CALLBACK_EVENT_ID = event;
        SN::CALLBACK_EVENT_IDS.push_back(SN::CALLBACK_EVENT_ID);
        listenerLoopExecute();
    }

LRESULT CALLBACK customWndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp){
    
    POWERBROADCAST_SETTING *pbs = (POWERBROADCAST_SETTING *)lp;
    
    switch (msg) {
        case WM_WTSSESSION_CHANGE:
            switch (wp) {
                case WTS_SESSION_LOCK:
                    SN::call(SN_On_Screen_Lock);//screen lock
                    break;
                case WTS_SESSION_UNLOCK:
                    SN::call(SN_On_Screen_Unlock);//screen unlock
                    break;
                case WTS_SESSION_REMOTE_CONTROL:
                case WTS_CONSOLE_CONNECT:
                case WTS_CONSOLE_DISCONNECT:
                case WTS_REMOTE_CONNECT:
                case WTS_REMOTE_DISCONNECT:
                case WTS_SESSION_LOGON:
                case WTS_SESSION_LOGOFF:
                default:
                    break;
            }
            break;
        case WM_QUERYENDSESSION:
            if(SN::PROCESS_SHOULD_TERMINATE){
                return true;
            }else{
                SN::call(SN_On_Before_Machine_Power_Off);
            }
            return false;//refuse to quit
            break;
        case WM_POWERBROADCAST:
            switch (wp) {
                case PBT_APMRESUMEAUTOMATIC://resume
                    SN::call(SN_On_After_Machine_Wake);
                    break;
                case PBT_APMSUSPEND://suspend
                    SN::call(SN_On_Before_Machine_Sleep);
                    break;
                case PBT_POWERSETTINGCHANGE:
                    if(pbs->PowerSetting == GUID_MONITOR_POWER_ON)
                    {
                        if(SN::first_event_call){
                            SN::first_event_call = false;
                        }else{
                        DWORD state = *((DWORD*)pbs->Data);
                        switch (state)
                        {
                        case 0:
                            SN::call(SN_On_Before_Screen_Sleep);//screen off
                            break;
                        case 1:
                            SN::call(SN_On_After_Screen_Wake);//screen on
                            break;
                        }
                        }
                    }
                    break;
                default:
                    break;
            }
            break;
        case WM_SYSCOMMAND:
            switch (wp) {
                case SC_SCREENSAVE:
                    SN::call(SN_On_Screensaver_Start);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    
    return CallWindowProc(SN::originalWndProc, wnd, msg, wp, lp);
}

}
#endif

static void listener_start() {
    
#if VERSIONMAC
    if(!SN::listener)
    {
        SN::listener = [[SNListener alloc]init];
    }
#else
    HWND hwnd = SN::MDI;
    if(hwnd != NULL) {
        SN::first_event_call = true;
        SN::notificationHandle = RegisterPowerSettingNotification(
			hwnd, &GUID_MONITOR_POWER_ON,                     
			DEVICE_NOTIFY_WINDOW_HANDLE);
        SN::originalWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)SN::customWndProc);
        WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
    }
#endif
}

static void listener_end() {
    
#if VERSIONMAC
    if(SN::listener) {
    /* must do this in main process */
    [SN::listener release];
    SN::listener = nil;
    }
#else
    HWND hwnd = SN::MDI;
	if (hwnd != NULL) {
		SN::first_event_call = true;
		SN::notificationHandle = RegisterPowerSettingNotification(hwnd, &GUID_MONITOR_POWER_ON, DEVICE_NOTIFY_WINDOW_HANDLE);
		SN::originalWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
		SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)SN::customWndProc);
        WTSUnRegisterSessionNotification(hwnd);
	}
#endif
    
}

#if VERSIONMAC

#if CGFLOAT_IS_DOUBLE
static IMP __orig_imp_applicationShouldTerminate;

NSApplicationTerminateReply __swiz_applicationShouldTerminate(id self, SEL _cmd, id sender) {
    
    [SN::listener call:SN_On_Before_Quit];
    
    return NSTerminateCancel;//refuse to quit
}

IMP __swiz_imp_applicationShouldTerminate = (IMP)__swiz_applicationShouldTerminate;
#else
AEEventHandlerUPP __upp_applicationShouldTerminate;
OSErr carbon_applicationShouldTerminate(const AppleEvent *appleEvt, AppleEvent* reply, UInt32 refcon) {
    
    [SN::listener call:SN_On_Before_Quit];
    
    return noErr;
}

static pascal OSErr HandleQuitMessage(const AppleEvent *appleEvt, AppleEvent *reply, long refcon) {
    
    return userCanceledErr;
}
#endif

@implementation SNListener

- (id)init {
    
    if(!(self = [super init])) return self;
    
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace]notificationCenter];
    
    [center addObserver:self
               selector:@selector(willSleep:)
                   name:NSWorkspaceWillSleepNotification
                 object:nil
     ];
    
    [center addObserver:self
               selector:@selector(didWake:)
                   name:NSWorkspaceDidWakeNotification
                 object:nil
     ];
    
    [center addObserver:self
               selector:@selector(screensDidSleep:)
                   name:NSWorkspaceScreensDidSleepNotification
                 object:nil
     ];
    
    [center addObserver:self
               selector:@selector(screensDidWake:)
                   name:NSWorkspaceScreensDidWakeNotification
                 object:nil
     ];
    
    [center addObserver:self
               selector:@selector(willPowerOff:)
                   name:NSWorkspaceWillPowerOffNotification
                 object:nil
     ];
    
    NSDistributedNotificationCenter *defaultCenter = [NSDistributedNotificationCenter defaultCenter];
    
    [defaultCenter addObserver:self
                      selector:@selector(screensaverDidstart:)
                          name:@"com.apple.screensaver.didstart"
                        object:nil
     ];
    
    [defaultCenter addObserver:self
                      selector:@selector(screensaverDidStop:)
                          name:@"com.apple.screensaver.didstop"
                        object:nil
     ];
    
    [defaultCenter addObserver:self
                      selector:@selector(screenIsLocked:)
                          name:@"com.apple.screenIsLocked"
                        object:nil
     ];
    
    [defaultCenter addObserver:self
                      selector:@selector(screenIsUnlocked:)
                          name:@"com.apple.screenIsUnlocked"
                        object:nil
     ];
    
    /* swizzle */
#if CGFLOAT_IS_DOUBLE
    Class MainAppDelegateClass = [[[NSApplication sharedApplication]delegate]class];
    
    if(MainAppDelegateClass)
    {
        __orig_imp_applicationShouldTerminate =
        method_setImplementation(
                                                         class_getInstanceMethod(MainAppDelegateClass,
                                                                                                         @selector(applicationShouldTerminate:)),
                                                         __swiz_imp_applicationShouldTerminate);
    }
#else
    __upp_applicationShouldTerminate = NewAEEventHandlerUPP(HandleQuitMessage);
    AEInstallEventHandler(kCoreEventClass,
                                                kAEQuitApplication,
                                                __upp_applicationShouldTerminate, 0, false);
#endif

    return self;
}

- (void)dealloc {
    
    /* swizzle */
#if CGFLOAT_IS_DOUBLE
    Class MainAppDelegateClass = [[[NSApplication sharedApplication]delegate]class];
    
    if(MainAppDelegateClass)
    {
        method_setImplementation(
                                                         class_getInstanceMethod(MainAppDelegateClass, @selector(applicationShouldTerminate:)),
                                                         __orig_imp_applicationShouldTerminate);
    }
#else
    AERemoveEventHandler(kCoreEventClass,
                                             kAEQuitApplication,
                                             __upp_applicationShouldTerminate,
                                             false);
    DisposeAEEventHandlerUPP(__upp_applicationShouldTerminate);
#endif
    
    [[[NSWorkspace sharedWorkspace] notificationCenter]removeObserver:self];
    
    [[NSDistributedNotificationCenter defaultCenter]removeObserver:self];
    
    [super dealloc];
}

- (void)didWake:(NSNotification *)notification {
    
    [self call:SN_On_After_Machine_Wake];
}

- (void)willSleep:(NSNotification *)notification {
    
    [self call:SN_On_Before_Machine_Sleep];
}

- (void)willPowerOff:(NSNotification *)notification {
    
    [self call:SN_On_Before_Machine_Power_Off];
}

- (void)screensDidWake:(NSNotification *)notification {
    
    [self call:SN_On_After_Screen_Wake];
}

- (void)screensDidSleep:(NSNotification *)notification {
    
    [self call:SN_On_Before_Screen_Sleep];
}

- (void)screenIsLocked:(NSNotification *)notification {
    
    [self call:SN_On_Screen_Lock];
}

- (void)screenIsUnlocked:(NSNotification *)notification {
    
    [self call:SN_On_Screen_Unlock];
}

- (void)screensaverDidstart:(NSNotification *)notification {
    
    [self call:SN_On_Screensaver_Start];
}

- (void)screensaverDidStop:(NSNotification *)notification {
    
    [self call:10];
}

- (void)call:(event_id_t)event {
    
    SN::CALLBACK_EVENT_ID = event;
    SN::CALLBACK_EVENT_IDS.push_back(SN::CALLBACK_EVENT_ID);
    listenerLoopExecute();
}

@end

#endif

static void generateUuid(C_TEXT &returnValue) {
    
#if VERSIONMAC
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
    returnValue.setUTF16String([[[NSUUID UUID]UUIDString]stringByReplacingOccurrencesOfString:@"-" withString:@""]);
#else
    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    NSString *uuid_str = (NSString *)CFUUIDCreateString(kCFAllocatorDefault, uuid);
    returnValue.setUTF16String([uuid_str stringByReplacingOccurrencesOfString:@"-" withString:@""]);
#endif
#else
    RPC_WSTR str;
    UUID uuid;
    int len;
    wchar_t *buffer;
    if(UuidCreate(&uuid) == RPC_S_OK){
        if(UuidToString(&uuid, &str)==RPC_S_OK){
            len = (wcslen((const wchar_t *)str)*2)+2;
            buffer = (wchar_t *)malloc(len);if(buffer){
                memset(buffer,0x00, len);
                memcpy(buffer, str, len);
                _wcsupr((wchar_t *)buffer);
                CUTF16String uid = (const PA_Unichar *)buffer;
                returnValue.setUTF16String(&uid);
                free(buffer);
            }
            RpcStringFree(&str);
        }
    }
#endif
}

void listenerLoop() {
    
    if(1)
    {
        std::lock_guard<std::mutex> lock(globalMutex3);
        
        SN::PROCESS_SHOULD_TERMINATE = false;
    }
    
    /* Current process returns 0 for PA_NewProcess */
    PA_long32 currentProcessNumber = PA_GetCurrentProcessNumber();
    
    while(!PA_IsProcessDying())
    {
        PA_YieldAbsolute();
        
        bool PROCESS_SHOULD_RESUME;
        bool PROCESS_SHOULD_TERMINATE;
        
        if(1)
        {
            PROCESS_SHOULD_RESUME = SN::PROCESS_SHOULD_RESUME;
            PROCESS_SHOULD_TERMINATE = SN::PROCESS_SHOULD_TERMINATE;
        }
        
        if(PROCESS_SHOULD_RESUME)
        {
            size_t EVENT_IDS;
            
            if(1)
            {
                std::lock_guard<std::mutex> lock(globalMutex);
                
                EVENT_IDS = SN::CALLBACK_EVENT_IDS.size();
            }
            
            while(EVENT_IDS)
            {
                PA_YieldAbsolute();
                
                if(CALLBACK_IN_NEW_PROCESS)
                {
                    C_TEXT processName;
                    generateUuid(processName);
                    PA_NewProcess((void *)listenerLoopExecuteMethod,
                                  SN::MONITOR_PROCESS_STACK_SIZE,
                                  (PA_Unichar *)processName.getUTF16StringPtr());
                }else
                {
                     listenerLoopExecuteMethod();
                }
                
                if(PROCESS_SHOULD_TERMINATE)
                    break;
                
                if(1)
                {
                    std::lock_guard<std::mutex> lock(globalMutex);
                    
                    EVENT_IDS = SN::CALLBACK_EVENT_IDS.size();
                    PROCESS_SHOULD_TERMINATE = SN::PROCESS_SHOULD_TERMINATE;
                }
            }
            
            if(1)
            {
                std::lock_guard<std::mutex> lock(globalMutex4);
                
                SN::PROCESS_SHOULD_RESUME = false;
            }
            
        }else
        {
            PA_PutProcessToSleep(currentProcessNumber, CALLBACK_SLEEP_TIME);
        }
        
        if(1)
        {
            PROCESS_SHOULD_TERMINATE = SN::PROCESS_SHOULD_TERMINATE;
        }
        
        if(PROCESS_SHOULD_TERMINATE)
            break;
    }
    
    if(1)
    {
        std::lock_guard<std::mutex> lock(globalMutex);
        
        SN::CALLBACK_EVENT_IDS.clear();
    }
    
    if(1)
    {
        std::lock_guard<std::mutex> lock(globalMutex2);
        
        SN::LISTENER_METHOD.setUTF16String((PA_Unichar *)"\0\0", 0);
    }

    if(1)
    {
        std::lock_guard<std::mutex> lock(globalMutex1);
        
        SN::METHOD_PROCESS_ID = 0;
    }
    
#if VERSIONMAC
    PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listener_end, NULL);
#else
//	listener_end();
#endif

    PA_KillProcess();
}

void listenerLoopStart() {
    
    if(!SN::METHOD_PROCESS_ID)
    {
        std::lock_guard<std::mutex> lock(globalMutex1);
        
        SN::METHOD_PROCESS_ID = PA_NewProcess((void *)listenerLoop,
                                              SN::MONITOR_PROCESS_STACK_SIZE,
                                              SN::MONITOR_PROCESS_NAME);
    }
}

void listenerLoopFinish() {
    
    if(SN::METHOD_PROCESS_ID)
    {
        if(1)
        {
            std::lock_guard<std::mutex> lock(globalMutex3);
            
            SN::PROCESS_SHOULD_TERMINATE = true;
        }
        
        PA_YieldAbsolute();
        
        if(1)
        {
            std::lock_guard<std::mutex> lock(globalMutex4);

            SN::PROCESS_SHOULD_RESUME = true;
        }
    }
}

void listenerLoopExecute() {
    
    if(1)
    {
        std::lock_guard<std::mutex> lock(globalMutex3);
        
        SN::PROCESS_SHOULD_TERMINATE = false;
    }
    
    if(1)
    {
        std::lock_guard<std::mutex> lock(globalMutex4);
        
        SN::PROCESS_SHOULD_RESUME = true;
    }

}

void listenerLoopExecuteMethod() {
    
    std::vector<event_id_t>::iterator e = SN::CALLBACK_EVENT_IDS.begin();
    event_id_t event = (*e) - 1;
    
    method_id_t methodId = PA_GetMethodID((PA_Unichar *)SN::LISTENER_METHOD.getUTF16StringPtr());
    
    if(methodId)
    {
        PA_Variable    params[1];
        params[0] = PA_CreateVariable(eVK_Longint);
        PA_SetLongintVariable(&params[0], event);

        SN::CALLBACK_EVENT_IDS.erase(e);
        
        PA_ExecuteMethodByID(methodId, params, 1);
        
        PA_ClearVariable(&params[0]);
    }else{
        PA_Variable    params[2];
        params[1] = PA_CreateVariable(eVK_Longint);
        PA_SetLongintVariable(&params[1], event);
        
        params[0] = PA_CreateVariable(eVK_Unistring);
        PA_Unistring method = PA_CreateUnistring((PA_Unichar *)SN::LISTENER_METHOD.getUTF16StringPtr());
        PA_SetStringVariable(&params[0], &method);
        
        SN::CALLBACK_EVENT_IDS.erase(e);
        
        PA_ExecuteCommandByID(1007, params, 2);
        
        PA_ClearVariable(&params[0]);
        PA_ClearVariable(&params[1]);
    }
    
}

#define MAX_PROCESS_NAME 256

bool IsProcessOnExit()
{
    std::vector<PA_Unichar> name(MAX_PROCESS_NAME);
    
    PA_long32 state, time;
    
    PA_GetProcessInfo(PA_GetCurrentProcessNumber(), &name[0], &state, &time);
    CUTF16String procName(&name[0]);
    CUTF16String exitProcName((PA_Unichar *)"$\0x\0x\0\0\0");
    return (!procName.compare(exitProcName));
}

static void OnStartup()
{

}

static void OnCloseProcess()
{
    if(IsProcessOnExit())
    {
        listenerLoopFinish();
    }
}

void PluginMain(PA_long32 selector, PA_PluginParameters params) {
    
	try
	{
        switch(selector)
        {
            case kInitPlugin :
            case kServerInitPlugin :
                OnStartup();
#if VERSIONWIN
                SN::MDI = getMDI();
                listener_start();
#endif
                break;
                
            case kCloseProcess :
                OnCloseProcess();
                break;
            
            case kDeinitPlugin :
            case kServerDeinitPlugin :
#if VERSIONWIN
                listener_end();
#endif
                break;
                
			// --- System Notification
            
			case 1 :
				SN_Set_method(params);
				break;
			case 2 :
				SN_Get_method(params);
				break;

        }

	}
	catch(...)
	{

	}
}

#pragma mark -

void SN_Set_method(PA_PluginParameters params) {

    PackagePtr pParams = (PackagePtr)params->fParameters;
    sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
    
    C_TEXT Param1;
    C_LONGINT returnValue;
        
    if(!IsProcessOnExit())
    {
        if(1)
        {
            std::lock_guard<std::mutex> lock(globalMutex2);
            
            SN::LISTENER_METHOD.fromParamAtIndex(pParams, 1);
        }
        
#if VERSIONMAC
        PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listener_start, NULL);
#else
//		listener_start();
#endif
        listenerLoopStart();
        returnValue.setIntValue(1);
    }
    
    returnValue.setReturn(pResult);
}

void SN_Get_method(PA_PluginParameters params) {
    
    sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
    
    if(1)
    {
        SN::LISTENER_METHOD.setReturn(pResult);
    }
    
}
