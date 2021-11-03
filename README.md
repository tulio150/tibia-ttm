# tibia-ttm
Tibia Time Machine 6.2
by tulio150 - otland.net
------------------------

Tibia Time Machine is a program for recording and watching Tibia videos.

Supported clients:
-official clients of protocol 7.0 to 10.20
-some OTServer custom clients

Key features:
-Keeps separated configuration files for each Tibia version (You don't lose your hotkeys)
-Saves Tibia version and OpenTibia server information within videos
-You can start recording anytime
-Converts old TibiCAM and TibiaCam TV recordings
-Compatible with OpenTibia servers (opens otserv:// links)
-Can be used with another proxy
-Playing features:
	-Speed control (pause + slow motion 8x + fast forward 512x + packet-by-packet)
	-Skipping backwards, forwards and to game sessions
	-Video cutting and timing editor
	-Control video with the arrow keys from Tibia
	-Commands on Tibia console for fullscreen playing
	-Set video light for old Tibia versions

How to record a video:
-Open Tibia Time Machine
-Click on "Start" (hold shift to change client)
-Login with your account name and password

How to play a video:
-Double-click on the video (see below)
OR
-Drop a video file on Tibia Time Machine (drop multiple to append)
OR
-Open Tibia Time Macine, then click on "Open"
-Click on "Start"
-Login with empty Password

How to enable double-clicking on a video and otserv links:
-Open Tibia Time Machine
-Click on "Options", then click on "Register video files"

How to connect to another local proxy: (tibiacast, etc)
-Open Tibia Time Machine
-Click on "Options", then check "Use local proxy"
-Click on "Start"
-Wait for a message
-Activate your proxy (don't use auto-activate)
-Click on "OK"

Controls:
-The list box shows the recorded game sessions, in the format: "Number +Duration =End"
-"Delete" key: delete the selected session
-Shift + "Delete" key: delete all the sessions
-Right-click on the list for a menu

-"Esc" have the same effect as the left button
-"Enter" have the same effect as the right button

-While idle:
Button "Open": open a video
Button "Save": save the video
Button "Start": start OpenTibia
Button "Start" when Tibia already opened: start waiting for a login, so you can record or play a video
	-Will reconnect if already logged in
Double-click on the list: same effect as "Start"

-While recording:
Button "Cancel": cancel the recording and don't save it
Button "Stop": stop recording and save what you recorded
Logout: same effect as "Stop"

-While playing:
Button "Stop": stop playing the video
Button "Pause": pause the video
Button "Play": continue playing the video
Click on the list: skip right to the start of a session
Double-click on the list: skip to a session and continue playing at normal speed
Scroll bar: lets you skip back and forth on the video
Button "-": decrease playing speed (play a single packet when paused)
Button "+": increase playing speed
Mouse wheel: increase or decrease playing speed

-Commands from Tibia:
Ctrl + Arrows Up/Down: change playing speed
Ctrl + Arrows Left/Right: skip 15 seconds

-Console commands: (type them on the channels: "Default", "Local chat" or "Server Log")
play: set the speed to Playing
pause: pause the video
stop: stop playing the video
fast: increase playing speed
slow: decrease playing speed
fast [x]: set the speed to Fast x times (1 = x2, 9 = x512)
slow [x]: set the speed to Slow x times (1 = x2, 3 = x8)
start: skip to the start of the video
end: skip to the end of the video
skip: skip 1 minute
back: skip 1 minute back
skip [time]: skip the specified time (in seconds, minutes:seconds or hours:minutes:seconds)
back [time]: skip the specified time back
goto [time]: skip to the specified time mark
session: skip to the start of the current game session
session [x]: skip to the session number x (first is 0)
prev: skip to the previous game session
next: skip to the next game session
first: play the first packet of the session
last: play the last packet of the session
light: set full light in the client
light [x]: set light level x in the client
delete: delete the current game session from the video
cut-start: cut all video before the current time from the game session
cut-end: cut all remaining video from the game session
add-fast [time]: sets the next [time] to be played in 2x fast
add-slow [time]: sets the next [time] to be played in 2x slow
add-skip [time]: sets the next [time] to be skipped
add-delay [miliseconds]: insert a delay in the current video position
add-light [x]: inserts a light packet in the video

NOTE: In order to properly use the commands on Tibia 7.7 to 8.22 without getting "Account Data Warning" pop-ups, you can login with a random account number (ex: 123456) and an empty password to watch videos, or use auto play.
