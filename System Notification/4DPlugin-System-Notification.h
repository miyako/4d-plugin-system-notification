/* --------------------------------------------------------------------------------
 #
 #	4DPlugin-System-Notification.h
 #	source generated by 4D Plugin Wizard
 #	Project : System Notification
 #	author : miyako
 #	2022/01/20
 #  
 # --------------------------------------------------------------------------------*/

#ifndef PLUGIN_SYSTEM_NOTIFICATION_H
#define PLUGIN_SYSTEM_NOTIFICATION_H

#include "4DPluginAPI.h"
#include "C_TEXT.h"
#include "C_LONGINT.h"

#include <mutex>

void listenerInit(void);
void listenerLoop(void);
void listenerLoopStart(void);
void listenerLoopFinish(void);
void listenerLoopExecute(void);
void listenerLoopExecuteMethod(void);

typedef PA_long32 process_number_t;
typedef PA_long32 process_stack_size_t;
typedef PA_long32 method_id_t;
typedef PA_Unichar* process_name_t;

#pragma mark -

void SN_Set_method(PA_PluginParameters params);
void SN_Get_method(PA_PluginParameters params);

typedef PA_long32 event_id_t;

#if VERSIONMAC
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#import <objc/runtime.h>
@interface SNListener : NSObject
{

}
- (id)init;
- (void)dealloc;
- (void)willSleep:(NSNotification *)notification;
- (void)didWake:(NSNotification *)notification;
- (void)screensDidSleep:(NSNotification *)notification;
- (void)screensDidWake:(NSNotification *)notification;
- (void)willPowerOff:(NSNotification *)notification;
- (void)call:(event_id_t)event;
- (void)screenIsLocked:(NSNotification *)notification;
- (void)screenIsUnlocked:(NSNotification *)notification;
- (void)screensaverDidstart:(NSNotification *)notification;
- (void)screensaverDidStop:(NSNotification *)notification;
@end
#else
#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Rpcrt4.lib")
#endif

#define SYSTEM_NOTIFICATION_NOT_FOLDER_ERROR (-1)
#define SYSTEM_NOTIFICATION_INVALID_PATH_ERROR (-2)
#define SYSTEM_NOTIFICATION_INVALID_METHOD_NAME_ERROR (-3)

#endif /* PLUGIN_SYSTEM_NOTIFICATION_H */