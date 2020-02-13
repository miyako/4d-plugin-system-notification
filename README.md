# 4d-plugin-system-notification
Register callback method for display sleep/wake, system sleep/wake, system reboot (mac) and logoff (windows).

### Platform

| carbon | cocoa | win32 | win64 |
|:------:|:-----:|:---------:|:---------:|
||<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|<img src="https://cloud.githubusercontent.com/assets/1725068/22371562/1b091f0a-e4db-11e6-8458-8653954a7cce.png" width="24" height="24" />|

### Version

<img width="32" height="32" src="https://user-images.githubusercontent.com/1725068/73986501-15964580-4981-11ea-9ac1-73c5cee50aae.png"> <img src="https://user-images.githubusercontent.com/1725068/73987971-db2ea780-4984-11ea-8ada-e25fb9c3cf4e.png" width="32" height="32" />

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
