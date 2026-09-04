![version](https://img.shields.io/badge/version-17%2B-3E8B93)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-system-notification)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-system-notification/total)

# 4d-plugin-system-notification

This plugin lets a 4D database register a single project method as a callback for OS-level power, session, and screen-lock events — display sleep/wake, system sleep/wake, screen lock/unlock, screensaver start/stop, system reboot/logoff (Windows), and app quit (macOS). Internally it drives `NSWorkspace`/`NSDistributedNotificationCenter` notifications and Apple Event quit handling on macOS, and `WM_POWERBROADCAST` / `WM_WTSSESSION_CHANGE` / `WM_QUERYENDSESSION` / `WM_SYSCOMMAND` window messages (via window subclassing) plus `RegisterPowerSettingNotification` and `WTSRegisterSessionNotification` on Windows. Your callback method receives a single `Longint` identifying which event fired.

| Command | Returns | Purpose |
|---|---|---|
| [`SN Set method`](#sn-set-method) | Longint | Registers the project method to call on system events, and starts the listener. |
| [`SN Get method`](#sn-get-method) | Text | Returns the name of the currently registered callback method. |

**Platforms:** macOS (Intel & Apple Silicon) and Windows (64-bit), 4D v19 or later.

---

## Requirements & platform notes

- **Both commands are thread-safe** (per `manifest.json`) and can be called from any process, including worker/preemptive processes.
- **Only one callback method can be registered at a time.** The registered method name is stored in a single global slot; calling `SN Set method` again replaces the previous registration rather than adding a second listener. There is no "unregister" command — the only way to stop delivery is to register a different (or empty) method name.
- **The most important gotcha up front:** on both platforms the plugin actively **blocks** the OS shutdown/quit sequence until your callback tells it to stop. See the individual events below and the Error handling section — if you don't call `QUIT 4D` at the right point, the user's machine will not shut down/log off (Windows) or the app will refuse to quit (macOS).
- **On Windows**, the plugin installs its window hook (subclassing the main window, plus `RegisterPowerSettingNotification`/`WTSRegisterSessionNotification`) automatically when the plugin loads, not when you call `SN Set method`. It locates the application's main window itself; if that lookup ever fails (non-standard host window configuration), Windows-side events silently never fire — there is no error raised. Calling `SN Set method` only registers *which method* receives the already-flowing events; before you call it, any events that occur are simply not delivered anywhere.
- **On macOS**, the OS-level notification observers are installed only when `SN Set method` is first called (they are not active at plugin load).
- **`SN On Before Quit`** and **`SN On Screensaver Stop`** are macOS-only — see the [event constants](#event-constants-delivered-to-your-callback-method) table.

---

## SN Set method

### Syntax

```4d
SN Set method ( methodName ) -> Function result
```

| Parameter | Type | Description |
|---|---|---|
| `methodName` | Text | Name of the project method to call whenever a system notification event occurs. |
| Result | Longint | `1` if the listener was installed and the method registered; otherwise the command returns without explicitly setting a value (reads as `0`) — see Description. |

The manifest declares this command's compact syntax as `SN Set method(&T):L`, matching a single Text parameter and a Longint result.

### Description

Stores `methodName` as the callback and starts the background listener (a dedicated 4D process the plugin creates itself). Your method must declare a single `Longint` parameter:

```4d
C_LONGINT($1)
```

`$1` receives one of the [event constants](#event-constants-delivered-to-your-callback-method) below.

If a project/database method exists under `methodName`, the plugin resolves it to a method ID and calls it directly. If no method is found under that exact name, the plugin falls back to an internal 4D dispatch command that invokes a method by name — the exact behavior of that fallback for a name that resolves to nothing at all isn't something this review traced conclusively, so if you register a name with a typo, don't assume you'll get a 4D error; verify the callback is actually firing.

There is one edge case worth knowing: if `SN Set method` is called while the current process is already exiting, registration is skipped entirely and the command returns without explicitly setting a result — this is a silent no-op, not an error.

### Example

From the plugin's own test method (`SYSTEM_EVENT.4dm`):

```4d
//%attributes = {}
C_LONGINT:C283($1)

$path:=System folder:C487(Desktop:K41:16)+"test.txt"

If (Test path name:C476($path)=Is a document:K24:1)
    $doc:=Append document:C265($path)
Else 
    $doc:=Create document:C266($path)
End if 

SEND PACKET:C103($doc; String:C10($1)+"\r")

CLOSE DOCUMENT:C267($doc)

If ($1=SN On Before Machine Power Off)
    TRACE:C157
    QUIT 4D:C291
End if 
```

Registering it, and a generic `Case of` dispatcher in the style shown in the plugin's own README:

```4d
$success:=SN Set method ("SYSTEM_EVENT")
```

```4d
//callback method, e.g. "SYSTEM_EVENT"
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
        QUIT 4D
    : ($1=SN On Before Quit)  //macOS only
        ALERT("On Before Quit")
        QUIT 4D
End case 
```

---

## SN Get method

### Syntax

```4d
SN Get method -> Function result
```

| Parameter | Type | Description |
|---|---|---|
| Result | Text | Name of the currently registered callback method, or an empty string if none is registered. |

### Description

Returns whatever was last passed to `SN Set method`, with no side effects. Useful for confirming registration succeeded, or for restoring a previous callback after temporarily swapping it out.

### Example

```4d
$current:=SN Get method 
If ($current="")
    $success:=SN Set method ("SYSTEM_EVENT")
End if 
```

---

## Event constants (delivered to your callback method)

These are the values your callback's `$1` parameter is compared against. Use the named constants (`Case of ($1=SN On ...)`) rather than hardcoding numbers — this plugin's own README and test files never do the latter, and the underlying integer values aren't part of the plugin's documented, stable interface.

| Constant | Platforms | Fires when |
|---|---|---|
| `SN On After Machine Wake` | Windows + macOS | System resumes from sleep. |
| `SN On Before Machine Sleep` | Windows + macOS | System is about to sleep. |
| `SN On Before Machine Power Off` | Windows + macOS | **Windows:** the session is ending (logoff, shutdown, or restart) — the OS will not proceed until you call `QUIT 4D` here (or another notification cycle asks again). **macOS:** the system is about to power off/restart. |
| `SN On After Screen Wake` | Windows + macOS | Display power turns back on. |
| `SN On Before Screen Sleep` | Windows + macOS | Display is about to power off. |
| `SN On Screen Lock` | Windows + macOS | Screen/session is locked (Windows: `WTS_SESSION_LOCK`; macOS: `com.apple.screenIsLocked`). |
| `SN On Screen Unlock` | Windows + macOS | Screen/session is unlocked. |
| `SN On Screensaver Start` | Windows + macOS | Screensaver activates (Windows: `SC_SCREENSAVE`; macOS: `com.apple.screensaver.didstart`). |
| `SN On Before Quit` | **macOS only** | App-level quit was requested (⌘Q, Dock ▸ Quit, etc.). The plugin unconditionally cancels the quit — your callback must call `QUIT 4D` to actually let the app exit. |
| `SN On Screensaver Stop` | **macOS only** | Screensaver deactivates (`com.apple.screensaver.didstop`). No Windows equivalent is implemented by this plugin. |

---

## Error handling & troubleshooting

- **Windows shutdown/logoff will hang if you don't call `QUIT 4D`.** The plugin refuses `WM_QUERYENDSESSION` by default; your `SN On Before Machine Power Off` handler is the only way to let the session end. If your method is slow, errors out, or was never registered, the OS shutdown/logoff dialog will sit waiting.
- **macOS quit is always cancelled unless you call `QUIT 4D`.** Same shape as above, via `SN On Before Quit` — expect ⌘Q to appear to do nothing until you handle this.
- **Calling `SN Set method` during process exit is a silent no-op.** No error is raised; the method simply isn't registered. If registration seems to intermittently "not take," check whether you're calling it from a process that's already shutting down.
- **`SN On Before Quit` and `SN On Screensaver Stop` never fire on Windows.** Don't build cross-platform logic that assumes parity — check `SN On Before Machine Power Off` for the Windows equivalent of "the app/session is ending."
- **Windows event delivery depends on the plugin finding the application's main window at load time.** If that lookup fails (unusual for a standard 4D single-window host, but possible with non-standard embedding), no Windows-side event will ever fire, and nothing will report why.
- **Only one callback exists globally.** If you call `SN Set method` from two different places in your code expecting two independent listeners, the second call silently replaces the first.
- **An unresolved method name isn't guaranteed to raise a 4D error.** Verify a newly registered method actually fires (e.g. with a quick `ALERT`) rather than assuming a typo would be caught for you.

---

## Quick reference

```4d
//install
$success:=SN Set method ("SYSTEM_EVENT")

//check
$current:=SN Get method 

//callback: SYSTEM_EVENT
C_LONGINT($1)
Case of 
    : ($1=SN On Before Machine Power Off)  //Windows + macOS
        QUIT 4D
    : ($1=SN On Before Quit)  //macOS only
        QUIT 4D
    : ($1=SN On Screen Lock)
    : ($1=SN On Screen Unlock)
    : ($1=SN On Before Screen Sleep)
    : ($1=SN On After Screen Wake)
    : ($1=SN On Before Machine Sleep)
    : ($1=SN On After Machine Wake)
    : ($1=SN On Screensaver Start)
    : ($1=SN On Screensaver Stop)  //macOS only
End case 
```
