# ViscaJoy
Win32 app for controlling PTZ cameras via a flight controller joystick

**VISCA** is a standard RS232 based protocol for moving, panning, tilting and zooming various cameras.  
**DirectInput** is a DirectX component that gets input from, among other things, flight-controller-style joysticks.


# Requirements
* Windows with DirectX support

* DirectInput library files.  I believe these can be obtained from the [Microsoft Windows SDK](https://www.microsoft.com/en-us/download/details.aspx?id=8279), "Headers and Libraries.zip".  In particular, you need DXLib\dinput8.lib and DXLib\dxguid.lib

* [Damien Douxchamps' libVISCA library](https://damien.douxchamps.net/libvisca)

* A FC-style joystick (tested with Logitech Wingman Attack)

* 1-8 VISCA compatible PTZ camera(s), (tested with the Sony EVI-D30)


# Building
* Change the #define COM_PORT to the port your camera is on. Currently set to COM1
* Provide DirectInput library files.  
* Provide libVISCA library (libvisca_win32.c, libvisca.c)


# Operation

*You must have your camera(s) and joystick plugged in before launching the program.*

**Program window: Info Display**
The program UI is dreadfully basic.  It displays various data about:
* The currently selected camera
* The position of the joystick
* The current speed of tilting, panning, zooming
* Autofocus status

**Program window: Recenter**
You can press spacebar to re-center to the joystick current position.

**Program window: Reset**
Pressing the "0" key will send the currently selected camera to the "home" position, AKA 0,0

**Program window: Store Preset**
Holding SHIFT while pressing any of keys 1-9 will record the currently selected camera's position in that preset

**Program window: Recall Preset**
Pressing any of keys 1-9 will send the currently selected camera to the saved position for that preset

**Joystick control: Change Camera**
The directional clicker on the top of the stick changes which camera you are addressing.  It starts at 0 and increments in a clockwise direction up to 7.

**Joystick control: Pan, Tilt**
(Hopefully) as logic would dictate, moving the stick in the X and Y directions changes the panning and tilting of the camera accordingly.  Leaning the stick forward causes the camera to move up (much to the dismay of aviators), and vice-versa for down.

**Joystick control: Zoom**
Twisting the joystick clockwise will cause the currently selected camera to start zooming in.
Twisting the joystick counter-clockwise will cause the currently selected camera to start zooming out.

**Joystick control: Autofocus engage**
There is a button/control on the joystick that is assigned to this.  Unfortunately I can't remember which one.

**Joystick control: Backlight -**
There is a button/control on the joystick that is assigned to this.  Unfortunately I can't remember which one.

**Joystick control: Backlight +**
There is a button/control on the joystick that is assigned to this.  Unfortunately I can't remember which one.
