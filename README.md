![version](https://img.shields.io/badge/version-19%2B-5682DF)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-system-notification)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-system-notification/total)

# 4d-plugin-system-notification
Register callback method for display sleep/wake, system sleep/wake, system reboot (mac) and logoff (windows).

**News**: 

``3.1`` added 4 new events for Mac:

* SN On Screensaver Stop
* SN On Screensaver Start
* SN On Screen Lock
* SN On Screen Unlock

## Examples

```
//install event handler method
//its signature should be C_LONGINT($1)

$success:=SN Set method ("system_event")

//when the user logs off, or the system decideds to reboot

//on Mac;
//all applications are asked to quit; you should handle the "On Before Quit" event.
//the system waits 60 secs for all apps to quit.

//on Windows;
//all applications are asked if it is OK to end the session; you should handle the "On Before Machine Power Off" event.
//the system displays "programs still running" screen and waits for your app to quit.

//platform considerations:
//on Mac, the plugin immediately sends "cancelled by user" event. you should call QUIT 4D if you are ready to quit.

//to quickly test screen sleep notifications:
//Mac: control+shift+power
//Windows: http://www.itworld.com/article/2699995/consumerization/how-to-quickly-put-your-monitor-to-sleep.html
```
  
* Callback method

```
C_LONGINT($1)

Case of 
: ($1=SN On After Machine Wake)
ALERT("On After Machine Wake")
: ($1=SN On Before Screen Sleep)
ALERT("On Before Screen Sleep")
: ($1=SN On After Screen Wake)
ALERT("On After Screen Wake")
: ($1=SN On Before Machine Sleep)
ALERT("On Before Machine Sleep")
: ($1=SN On Before Machine Power Off)
ALERT("On Before Machine Power Off")
: ($1=SN On Before Quit)
ALERT("On Before Quit")
End case 
```
