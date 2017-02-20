/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : System Notification
 #	author : miyako
 #	2016/04/15
 #
 # --------------------------------------------------------------------------------*/


#include "4DPluginAPI.h"
#include "4DPlugin.h"

#pragma mark -

namespace SN
{
	
	process_name_t MONITOR_PROCESS_NAME = (PA_Unichar *)"$\0S\0L\0E\0E\0P\0_\0N\0O\0T\0I\0F\0I\0C\0A\0T\0I\0O\0N\0\0\0";
	process_number_t MONITOR_PROCESS_ID = 0;
	process_stack_size_t MONITOR_PROCESS_STACK_SIZE = 0;
	method_id_t CALLBACK_METHOD_ID = 0;
	event_id_t CALLBACK_EVENT_ID = 0;
	C_TEXT LISTENER_METHOD;
	bool MONITOR_PROCESS_SHOULD_TERMINATE;
	std::vector<event_id_t>CALLBACK_EVENT_IDS;
	bool termination_not_allowed = false;
	bool monitor_process_running = false;
#if VERSIONMAC
	Listener *listener = nil;
	IMP applicationShouldTerminate_original;
	IMP applicationShouldTerminate_swizzled;
	AEEventHandlerUPP applicationShouldTerminate_upp;
#else
	HWND windowRef = NULL;
	WNDPROC originalWndProc = NULL;
	HPOWERNOTIFY notificationHandle = NULL;
	bool first_event_call = false;

	void call(event_id_t event)
	{
		SN::CALLBACK_EVENT_ID = event;
		SN::CALLBACK_EVENT_IDS.push_back(SN::CALLBACK_EVENT_ID);
		PA_UnfreezeProcess(SN::MONITOR_PROCESS_ID);
	}
	
	HWND getHWND()
	{
		//the window class is the folder name of the application
		HWND mdi = NULL;
		wchar_t path[_MAX_PATH] = {0};
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
	
	LRESULT CALLBACK customWndProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp){
		
		POWERBROADCAST_SETTING *pbs = (POWERBROADCAST_SETTING *)lp;
		
		switch (msg) {
			case WM_QUERYENDSESSION:
				SN::call(3);
				return false;//refuse to quit
				break;
			case WM_POWERBROADCAST:
				switch (wp) {
					case PBT_APMRESUMEAUTOMATIC://resume
						SN::call(1);
						break;
					case PBT_APMSUSPEND://suspend
						SN::call(2);
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
								SN::call(5);//screen off
								break;
							case 1:
								SN::call(4);//screen on
								break;
							}							
							}
						}
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
	
#endif
	
}

#if VERSIONMAC

@implementation Listener

- (id)init
{
	if(!(self = [super init])) return self;
	
	NSNotificationCenter *center = [[NSWorkspace sharedWorkspace]notificationCenter];
	
	[center
	 addObserver:self
	 selector: @selector(willSleep:)
	 name:NSWorkspaceWillSleepNotification
	 object:nil];
	
	[center
	 addObserver:self
	 selector: @selector(didWake:)
	 name:NSWorkspaceDidWakeNotification
	 object:nil];
	
	[center
	 addObserver:self
	 selector: @selector(screensDidSleep:)
	 name:NSWorkspaceScreensDidSleepNotification
	 object:nil];
	
	[center
	 addObserver:self
	 selector: @selector(screensDidWake:)
	 name:NSWorkspaceScreensDidWakeNotification
	 object:nil];
	
	[center
	 addObserver:self
	 selector: @selector(willPowerOff:)
	 name:NSWorkspaceWillPowerOffNotification
	 object:nil];
	
	return self;
}
- (void)dealloc
{
	[[[NSWorkspace sharedWorkspace] notificationCenter]removeObserver:self];
	SN::CALLBACK_EVENT_IDS.clear();
	[super dealloc];
}
- (void)didWake:(NSNotification *)notification
{
	[self call:1];
}
- (void)willSleep:(NSNotification *)notification
{
	[self call:2];
}
//it seems we don't reveice this notification if we handle kAEQuitApplication
- (void)willPowerOff:(NSNotification *)notification
{
	[self call:3];
}
- (void)screensDidWake:(NSNotification *)notification
{
	[self call:4];
}
- (void)screensDidSleep:(NSNotification *)notification
{
	[self call:5];
}

- (void)call:(event_id_t)event
{
	SN::CALLBACK_EVENT_ID = event;
	SN::CALLBACK_EVENT_IDS.push_back(SN::CALLBACK_EVENT_ID);
	PA_UnfreezeProcess(SN::MONITOR_PROCESS_ID);
}
@end

NSApplicationTerminateReply swizzle_applicationShouldTerminate(id self, SEL _cmd)
{
	[SN::listener call:6];
	
	//https://blog.newrelic.com/2014/04/16/right-way-to-swizzle/
	
		return NSTerminateNow;
}

OSErr carbon_applicationShouldTerminate(const AppleEvent *appleEvt, AppleEvent* reply, UInt32 refcon)
{
	[SN::listener call:6];
	
	return noErr;
}

#endif

#pragma mark -

void generateUuid(C_TEXT &returnValue)
{
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

#pragma mark -

bool IsProcessOnExit()
{
	C_TEXT name;
	PA_long32 state, time;
	PA_GetProcessInfo(PA_GetCurrentProcessNumber(), name, &state, &time);
	CUTF16String procName(name.getUTF16StringPtr());
	CUTF16String exitProcName((PA_Unichar *)"$\0x\0x\0\0\0");
	return (!procName.compare(exitProcName));
}

void OnStartup()
{
#if VERSIONMAC
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		SN::applicationShouldTerminate_swizzled = (IMP)swizzle_applicationShouldTerminate;
	});
#else
	SN::windowRef = SN::getHWND();
#endif
}

void OnCloseProcess()
{
	if(IsProcessOnExit())
	{
#if VERSIONWIN
		//should NOT do this in the main process
		listenerLoopFinish();
#else
		PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listenerLoopFinish, NULL);
//		[[NSApplication sharedApplication]replyToApplicationShouldTerminate:YES];
#endif
	}
}

#pragma mark -

void listenerLoop()
{
	SN::MONITOR_PROCESS_SHOULD_TERMINATE = false;
	
	while(!SN::MONITOR_PROCESS_SHOULD_TERMINATE)
	{
		PA_YieldAbsolute();
		while(SN::CALLBACK_EVENT_IDS.size())
		{
			PA_YieldAbsolute();
			C_TEXT processName;
			generateUuid(processName);
			PA_NewProcess((void *)listenerLoopExecute,
										SN::MONITOR_PROCESS_STACK_SIZE,
										(PA_Unichar *)processName.getUTF16StringPtr());
			
			if(SN::MONITOR_PROCESS_SHOULD_TERMINATE)
				break;
		}
		
		if(!SN::MONITOR_PROCESS_SHOULD_TERMINATE){
			PA_FreezeProcess(PA_GetCurrentProcessNumber());
		}else{
			SN::MONITOR_PROCESS_ID = 0;
		}
	}
	PA_KillProcess();
}

void listenerLoopStart()
{
	if(!SN::MONITOR_PROCESS_ID)
	{
#if VERSIONMAC
		SN::listener = [[Listener alloc]init];
#else
		SN::first_event_call = true;
		SN::notificationHandle = RegisterPowerSettingNotification(
																															SN::windowRef, &GUID_MONITOR_POWER_ON,
																															DEVICE_NOTIFY_WINDOW_HANDLE);
		
		SN::originalWndProc = (WNDPROC)GetWindowLongPtr(SN::windowRef, GWLP_WNDPROC);
		SetWindowLongPtr(SN::windowRef, GWLP_WNDPROC, (LONG_PTR)SN::customWndProc);
#endif
		SN::MONITOR_PROCESS_ID = PA_NewProcess((void *)listenerLoop, SN::MONITOR_PROCESS_STACK_SIZE, SN::MONITOR_PROCESS_NAME);
	}
#if VERSIONMAC
	//swizzled nsapplication class gives no breathing space to call method; use apple event
	/*
	Method applicationShouldTerminate = class_getInstanceMethod([[[NSApplication sharedApplication]delegate]class],
																				@selector(applicationShouldTerminate:));
	
	if(applicationShouldTerminate)
	{
		SN::applicationShouldTerminate_original =
		method_setImplementation(applicationShouldTerminate, SN::applicationShouldTerminate_swizzled);
	}else
	*/
	{
		//if we process the quit event to suspend shut down;
		//the app will not be running by the time NSWorkspaceWillPowerOffNotification is posted
		SN::applicationShouldTerminate_upp = NewAEEventHandlerUPP((AEEventHandlerProcPtr)carbon_applicationShouldTerminate);
		AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, SN::applicationShouldTerminate_upp, 0, false);
	}
#endif
}

void listenerLoopFinish()
{
	if(SN::MONITOR_PROCESS_ID)
	{
		//set flags
		SN::MONITOR_PROCESS_SHOULD_TERMINATE = true;
		PA_YieldAbsolute();
		SN::LISTENER_METHOD.setUTF16String((PA_Unichar *)"\0\0", 0);
		SN::CALLBACK_METHOD_ID = 0;
#if VERSIONMAC
		[SN::listener release];
		SN::listener = nil;
#endif
//on v16 (32 or 64) the code is not waking the listener loop; just exit
//tell listener to die
//		while(SN::MONITOR_PROCESS_ID)
//		{
//			PA_YieldAbsolute();
			PA_UnfreezeProcess(SN::MONITOR_PROCESS_ID);
		
//#if VERSIONMAC
//			SN::MONITOR_PROCESS_ID = 0;
//#endif
//		}
	}
#if VERSIONMAC
	//swizzled nsapplication class gives no breathing space to call method; use apple event instead
	/*
	Method applicationShouldTerminate = class_getInstanceMethod([[[NSApplication sharedApplication]delegate]class],
																				@selector(applicationShouldTerminate:));
	
	if(applicationShouldTerminate)
	{
		method_setImplementation(applicationShouldTerminate, SN::applicationShouldTerminate_original);
	}else
	*/
	{
		DisposeAEEventHandlerUPP(SN::applicationShouldTerminate_upp);
	}
#else
	UnregisterPowerSettingNotification(SN::notificationHandle);
	SN::notificationHandle = NULL;
	SetWindowLongPtr(SN::windowRef, GWLP_WNDPROC, (LONG_PTR)SN::originalWndProc);
#endif
	SN::monitor_process_running = false;
}

void listenerLoopExecute()
{
	std::vector<event_id_t>::iterator e = SN::CALLBACK_EVENT_IDS.begin();
	event_id_t event = (*e) - 1;
	
	if(SN::CALLBACK_METHOD_ID)
	{
		PA_Variable	params[1];
		params[0] = PA_CreateVariable(eVK_Longint);
		PA_SetLongintVariable(&params[0], event);
		//the method could be paused or traced
		SN::CALLBACK_EVENT_IDS.erase(e);
		PA_ExecuteMethodByID(SN::CALLBACK_METHOD_ID, params, 1);
		PA_ClearVariable(&params[0]);
	}else{
		//the method could have been removed
		SN::CALLBACK_EVENT_IDS.erase(e);
	}
}

#pragma mark -

void PluginMain(PA_long32 selector, PA_PluginParameters params)
{
	try
	{
		PA_long32 pProcNum = selector;
		sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
		PackagePtr pParams = (PackagePtr)params->fParameters;

		CommandDispatcher(pProcNum, pResult, pParams);
	}
	catch(...)
	{

	}
}

void CommandDispatcher (PA_long32 pProcNum, sLONG_PTR *pResult, PackagePtr pParams)
{
	switch(pProcNum)
	{
			
		case kInitPlugin :
		case kServerInitPlugin :
			OnStartup();
			break;
			
		case kCloseProcess :
			OnCloseProcess();
			break;
			
// --- Notification

		case 1 :
			SN_Set_method(pResult, pParams);
			break;

		case 2 :
			SN_Get_method(pResult, pParams);
			break;

	}
}

// --------------------------------- Notification ---------------------------------


void SN_Set_method(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT Param1;
	C_LONGINT returnValue;
	
	Param1.fromParamAtIndex(pParams, 1);
	
	if(!Param1.getUTF16Length())
	{
		//empty string passed
		returnValue.setIntValue(1);
		if(SN::LISTENER_METHOD.getUTF16Length())
		{
#if VERSIONWIN
			//should NOT do this in the main process
			listenerLoopFinish();
#else
			PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listenerLoopFinish, NULL);
#endif
		}
	}else
	{
		int methodId = PA_GetMethodID((PA_Unichar *)Param1.getUTF16StringPtr());
		if(methodId)
		{
			returnValue.setIntValue(1);
			if(methodId != SN::CALLBACK_METHOD_ID)
			{
				SN::LISTENER_METHOD.setUTF16String(Param1.getUTF16StringPtr(), Param1.getUTF16Length());
				SN::CALLBACK_METHOD_ID = methodId;
				if(!SN::monitor_process_running)
				{
					SN::monitor_process_running = true;
#if VERSIONWIN
					//should NOT do this in the main process
					listenerLoopStart();
#else
					PA_RunInMainProcess((PA_RunInMainProcessProcPtr)listenerLoopStart, NULL);
#endif
				}
			}
		}
	}
	returnValue.setReturn(pResult);
}

void SN_Get_method(sLONG_PTR *pResult, PackagePtr pParams)
{
	SN::LISTENER_METHOD.setReturn(pResult);
}
