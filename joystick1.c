/*
* VISCAJoy
* Win32 app for controlling PTZ cameras via a flight controller joystick
* Author: Scott McGrath
* Date: 9/28/2012
* https://github.com/HighTechHarmony/ViscaJoy
*/

#include <windows.h>

#define DIRECTINPUT_VERSION 0x800
#include "dinput.h"
#include <stdio.h>
#include <fcntl.h> /* File control definitions */
#include <errno.h> /* Error number definitions */
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include "Joystick1.h"
#include "libvisca.h"

#define SCREEN_X   400
#define SCREEN_Y   400
#define EVI_D30
#define FUDGE_FACTOR_PAN 3000
#define FUDGE_FACTOR_TILT 3000
#define FUDGE_FACTOR_ZOOM 6000
#define FUDGE_FACTOR_SPEED 100

#define PAN_FACTOR 1500  //Range of X to panning
#define TILT_FACTOR 1500  //Range of Y to tilting
#define COM_PORT "COM1" // Set this to the com port your camera is on (todo: read this from command line or menu selection)

HWND VISCAJoy_Window;
HINSTANCE hInstance;
char VISCAJoyClass[]="DICWindow";
char version[] = "v1.31";
static int quit = 0;

int stop_threads = 0;
int joystick_thread_dead = 0;


//-------------
//Functions
//-------------

int init_dinput(HINSTANCE);
int center_joystick();
void shutdown_dinput(void);
void show_data (HDC hdc);
void show_dinput_status(HDC hdc);
BOOL _EXPORT FAR PASCAL AboutDlgProc( HWND hwnd, unsigned msg,
                                UINT wparam, LONG lparam );
int init_visca (char *serial_port);
BOOL CALLBACK dinput_enum_callback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
char program_action[256]="";

//--------------
//Custom datatypes
//---------------
                                
typedef struct {
    int center_x;
    int center_y;
    int center_slider;
    int center_z;

    int x;
    int y;
    int z;
    int slider;
    int button_direction;
    
    int address;    
    int pan;
    int tilt;
    int zoom;
    int speed;
    BOOL autofocus;
    char action[256];
} joystick_data;

joystick_data joystick_a;
joystick_data joystick_b;

typedef struct {
        int pan;
        int tilt;
        uint16_t zoom;
        BOOL set;
} camera_position;

camera_position preset[7][10];    // Camera #, Preset #


//---------------------------------------------------------------------------------------
// DirectInput Variables
//---------------------------------------------------------------------------------------
IDirectInput8 *dinput;
IDirectInputDevice8 *keyboard;
IDirectInputDevice8 *mouse;
IDirectInputDevice8 *joystick;

//unsigned char keymap[256];
//DIMOUSESTATE2 mousedata;
DIJOYSTATE2 joydata;


VISCAInterface_t iface;
VISCACamera_t camera;

int camera_num;
uint8_t value;
uint16_t zoom;
int pan_pos, tilt_pos;


//---------------------------------------------------------------------------------------
// Thread to read data back from the joystick controller(s) and do stuff with it
//---------------------------------------------------------------------------------------
unsigned WINAPI read_joystick (void)
{
        HRESULT err;
        static DIJOYSTATE2 oldjoydata;
        uint16_t value16;

//        int x, y;
        int precision=1000;

        char text[256];
//        static int old_address;

        center_joystick();

        while (!stop_threads) {
            // Read the joystick
            if (joystick)
            {
                oldjoydata=joydata;
                err = IDirectInputDevice8_Poll(joystick);
                err = IDirectInputDevice8_GetDeviceState(joystick, sizeof joydata, &joydata);
            }

            if (err)  {// Assume we need to reacquire
              IDirectInputDevice8_Acquire(joystick);
            }

            if (joystick) {
                err = IDirectInputDevice8_Poll(joystick);
                err = IDirectInputDevice8_GetDeviceState(joystick, sizeof joydata, &joydata);

                    joystick_a.x = joydata.lX;
                    joystick_a.y = joydata.lY;
                    joystick_a.z = joydata.lRz;
                    joystick_a.slider = joydata.rglSlider[0];

                // At this point, we have old data in oldjoydata, new data in joydata.


// Zoom speed based on slider
// Now controlled by Z axis

/*                if (abs(joydata.rglSlider[0] - oldjoydata.rglSlider[0]) > FUDGE_FACTOR_SPEED) {
                    joystick_a.slider = joydata.rglSlider[0];
                    joystick_a.speed =(int)joydata.rglSlider[0]/3000;
                    if (joystick_a.speed < 2) {joystick_a.speed=2;}
                    if (joystick_a.speed > 7) {joystick_a.speed=7;}
                    VISCA_set_zoom_tele_speed(&iface, &camera, joystick_a.speed);
                    VISCA_set_zoom_wide_speed(&iface, &camera, joystick_a.speed);
                }
*/

// Zoom
                    sprintf(text, "Zoom W/T Raw: %d  Center: %d Conditioned: %d", joydata.lRz,joystick_a.center_z,(int)(joydata.lRz/precision));
                    if ((joydata.lRz - joystick_a.center_z) > FUDGE_FACTOR_ZOOM) {
                        joystick_a.zoom=abs(joystick_a.center_z-joydata.lRz)/2500;
                        if (joystick_a.zoom > 7) {joystick_a.zoom = 7;}  // Limit the max value
                        if (joystick_a.zoom < 2) {joystick_a.zoom = 2;}  // Limit the min value
                        VISCA_set_zoom_wide_speed(&iface, &camera, joystick_a.zoom);
//                        VISCA_set_zoom_wide(&iface, &camera);
                        sprintf(joystick_a.action,"Zoom wide");
                    }

                    else if (joydata.lRz - joystick_a.center_z < -FUDGE_FACTOR_ZOOM) {
                        joystick_a.zoom=abs(joystick_a.center_z-joydata.lRz)/2500;
                        if (joystick_a.zoom > 7) {joystick_a.zoom = 7;}  // Limit the max value
                        if (joystick_a.zoom < 2) {joystick_a.zoom = 2;}  // Limit the min value
                        VISCA_set_zoom_tele_speed(&iface, &camera, joystick_a.zoom);
//                        VISCA_set_zoom_tele(&iface, &camera);
                        sprintf(joystick_a.action,"Zoom tele");
                    }

                    else {VISCA_set_zoom_stop(&iface, &camera);}
    


// Pan right
                if (joydata.lX - joystick_a.center_x > FUDGE_FACTOR_PAN) {


                    //Possibly diagonal
                    // Down right
                    if (joydata.lY - joystick_a.center_y > FUDGE_FACTOR_TILT) {
                        joystick_a.tilt = abs(joydata.lY-joystick_a.center_y)/TILT_FACTOR;
                        VISCA_set_pantilt_downright(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan right/down");
                    }

                    // Up right
                    else if (joydata.lY - joystick_a.center_y < -FUDGE_FACTOR_TILT) {
                        joystick_a.tilt = abs(joystick_a.center_y-joydata.lY)/TILT_FACTOR;
                        VISCA_set_pantilt_upright(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan right/up");
                    }

                    // Right only
                    else {
                        joystick_a.pan = abs(joystick_a.center_x - joydata.lX)/PAN_FACTOR;
                        VISCA_set_pantilt_right(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan right");
                    }
                }

// Pan left
                else if (joydata.lX - joystick_a.center_x < -FUDGE_FACTOR_PAN) {

                    //Possibly diagonal
                    //Down left
                    if (joydata.lY - joystick_a.center_y > FUDGE_FACTOR_TILT) {
                        joystick_a.tilt = abs(joydata.lY-joystick_a.center_y)/TILT_FACTOR;
                        VISCA_set_pantilt_downleft(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan left/down");                    
                    }

                    //Up left
                    else if (joydata.lY - joystick_a.center_y < -FUDGE_FACTOR_TILT) {
                        joystick_a.tilt = abs(joystick_a.center_y-joydata.lY)/TILT_FACTOR;
                        VISCA_set_pantilt_upleft(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan left/up");
                    }

                    //Left only
                    else {
                        joystick_a.pan = (joystick_a.center_x-joydata.lX)/PAN_FACTOR;
                        VISCA_set_pantilt_left(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan left");
                    }
                }




/*                else {
                    VISCA_set_pantilt_stop(&iface, &camera, 0, joystick_a.tilt);
                    joystick_a.pan=0;
                    sprintf(joystick_a.action,"Stop");
                }
*/                
                

// Tilt down
                if (joydata.lY - joystick_a.center_y > FUDGE_FACTOR_TILT) {

                    //Possibly diagonal
                    //Down left
                    if (joydata.lX - joystick_a.center_x < -FUDGE_FACTOR_PAN) {
                        joystick_a.pan = (joystick_a.center_x-joydata.lX)/PAN_FACTOR;
                        VISCA_set_pantilt_downleft(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan down/left");
                    }

                    //Down right
                    else if (joydata.lX - joystick_a.center_x > FUDGE_FACTOR_PAN) {
                        joystick_a.pan = abs(joystick_a.center_x - joydata.lX)/PAN_FACTOR;
                        VISCA_set_pantilt_downright(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan down/right");
                    }


                    //Down only
                    else {
                        joystick_a.tilt = abs(joydata.lY-joystick_a.center_y)/TILT_FACTOR;
                        VISCA_set_pantilt_down(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Tilt down");
                    }
                    
                }

// Tilt up
                else if (joydata.lY - joystick_a.center_y < -FUDGE_FACTOR_TILT) {

                    //Possibly diagonal
                    //Up left
                    if (joydata.lX - joystick_a.center_x < -FUDGE_FACTOR_PAN) {
                        joystick_a.pan = (joystick_a.center_x-joydata.lX)/PAN_FACTOR;
                        VISCA_set_pantilt_upleft(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan up/left");
                    }

                    //Up right
                    else if (joydata.lX - joystick_a.center_x > FUDGE_FACTOR_PAN) {
                        joystick_a.pan = abs(joystick_a.center_x - joydata.lX)/PAN_FACTOR;
                        VISCA_set_pantilt_upright(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Pan up/right");
                    }

                    // Up only
                    else {
                        joystick_a.tilt = abs(joystick_a.center_y-joydata.lY)/TILT_FACTOR;
                        VISCA_set_pantilt_up(&iface, &camera, joystick_a.pan, joystick_a.tilt);
                        sprintf(joystick_a.action,"Tilt up");
                    }
                }

// Stop moving
                if ((abs(joystick_a.center_y - joydata.lY) < FUDGE_FACTOR_TILT) && (abs(joystick_a.center_x - joydata.lX) < FUDGE_FACTOR_PAN) && strcmp (joystick_a.action, "Stop"))
                {
                    VISCA_set_pantilt_stop(&iface, &camera, 0, joystick_a.tilt);
                    joystick_a.tilt=0;
                    joystick_a.pan=0;
                    VISCA_clear(&iface, &camera);                   // Clear the interface
                    sprintf(joystick_a.action,"Stop");
                }

// Set camera address
                joystick_a.button_direction=joydata.rgdwPOV[0];
                
                if (joydata.rgdwPOV[0] != -1) {

                    switch (joydata.rgdwPOV[0]) {
                           case 0: joystick_a.address=1; break;
                           case 4500: joystick_a.address=1; break;
                           case 9000: joystick_a.address=2; break;
                           case 13500: joystick_a.address=3; break;
                           case 18000: joystick_a.address=4; break;
                           case 22500: joystick_a.address=5; break;
                           case 27000: joystick_a.address=6; break;
                           case 31500: joystick_a.address=7; break;
                    }

                    camera.address=joystick_a.address;
                    VISCA_clear(&iface, &camera);
                    VISCA_get_camera_info(&iface, &camera);

                }


                if (joydata.rgbButtons[1] == 128) {
                    if (VISCA_set_backlight_comp(&iface,&camera,2) == VISCA_SUCCESS) {
//                        VISCA_get_backlight_comp(&iface,&camera,&value);
//                        sprintf(program_action, "Backlight + %d",value);
                        sprintf(program_action, "Backlight +");
                    }
                }
                  
                if (joydata.rgbButtons[2] == 128) {
                    if (VISCA_set_backlight_comp(&iface,&camera,3) == VISCA_SUCCESS) {
//                        VISCA_get_backlight_comp(&iface,&camera,&value);
//                        sprintf(program_action, "Backlight - %d",value);
                        sprintf(program_action, "Backlight -");
                    }
                }

                if (joydata.rgbButtons[3] == 128 && strcmp (joystick_a.action, "Focus +")) {
                    VISCA_set_focus_auto(&iface,&camera,0);
                    VISCA_get_focus_value(&iface,&camera,&value16);
                    value16+=1000;
                    VISCA_set_focus_value(&iface,&camera,value16);
                    Sleep(100);
                    sprintf(program_action, "Focus + %d", value16);
//                    VISCA_set_focus_near(&iface, &camera);
                }

                if (joydata.rgbButtons[4] == 128 && strcmp (joystick_a.action, "Focus -")) {
                    VISCA_set_focus_auto(&iface,&camera,0);
                    VISCA_get_focus_value(&iface,&camera,&value16);
                    value16-=1000;
                    VISCA_set_focus_value(&iface,&camera,value16);
                    Sleep(100);
                    sprintf(program_action, "Focus - %d", value16);
                }          

/*                if (!strncmp(joystick_a.action,"Focus",strlen("Focus")) && !joydata.rgbButtons[3] && !joydata.rgbButtons[4]) {
                    VISCA_set_focus_stop(&iface, &camera);
                }
*/
                if (joydata.rgbButtons[0] == 128) {
//                    joystick_a.autofocus = !joystick_a.autofocus;  //Toggle
//                    sprintf(joystick_a.action, "Autofocus to %d",joystick_a.autofocus);
                      sprintf(program_action,"Autofocus");
                    VISCA_set_focus_auto(&iface, &camera, 1);
                }

            }
            
            else {
                MessageBox( NULL, "Joystick was not found. Program will now exit", "Program Error", MB_OK );
                // Close everything down
                //tell Windows to close the app
                                        
                PostQuitMessage(0);
                quit = 1;
                break;
            }

            Sleep (30);

        }  // End of loop
        
        joystick_thread_dead=1;

        return err;
        }


//-------------------
//Window Proc
//-------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
//        FARPROC     proc;
//        extra_data  *edata_ptr;
        char        buff[128];
        int i, pan_temp, tilt_temp;
        uint16_t zoom_temp;
        

        switch (uMsg)
        {
                case WM_DESTROY:
                    stop_threads=1;
                    while (!joystick_thread_dead) {Sleep (1000);}

                       //tell Windows to close the app
 
                        PostQuitMessage(0);
                        quit = 1;
                        break;

                case WM_PAINT:
                        {
                        PAINTSTRUCT ps;
                        BeginPaint(hwnd, &ps);
//                        show_dinput_status(ps.hdc);
                        show_data(ps.hdc);
                        EndPaint(hwnd, &ps);
                        }
                        break;
                        
                case WM_COMMAND:
                    switch( LOWORD( wParam ) ) {

                               case MENU_ABOUT:
/*                                    proc = MakeProcInstance( (FARPROC)AboutDlgProc, hInstance );
                                    DialogBox( hInstance,"AboutBox", hwnd, (DLGPROC)proc );
                                    FreeProcInstance( proc );
                                    */
                                    sprintf (buff, "VISCAJoy %s\n(c)2012 NFA Technologies",version);
                                                MessageBox( NULL, buff, "VISCAJoy", MB_OK );

                                    break;
            
                                case MENU_EXIT:
                                    //tell Windows to close the app
                                    PostQuitMessage(0);
                                    quit = 1;
                                    break;

/*                                       edata_ptr = (extra_data *) GetWindowLong( hwnd,
                                                            EXTRA_DATA_OFFSET );
                        #ifdef __NT__
                                    sprintf( buff, "Command string was \"%s\"", edata_ptr->cmdline );
                        #else
                                    sprintf( buff, "Command string was \"%Fs\"", edata_ptr->cmdline );
                        #endif
                                    MessageBox( NULL, buff, "Program Information", MB_OK );
*/                                    
                                    
                    }
                    break;
                    
                case WM_TIMER:
                        InvalidateRect(hwnd, 0, TRUE);
                        break;

                //this message gets sent when you press a key
                case WM_KEYDOWN:
                        switch (wParam)
                        {
                                //pressed Escape key?
                                case VK_ESCAPE:
                                        //tell Windows to close the app
                                        PostQuitMessage(0);
                                        quit = 1;
                                        break;

                                case VK_SPACE:
                                        //Recenter the joystick
                                        center_joystick();
                                        break;

                                case 0x30:
                                        // Home
                                            sprintf(program_action,"Recall home");                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                        
                                            VISCA_set_zoom_value(&iface,&camera,0);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,0,0);

                                        break;
                                        

                                case 0x31:
                                        // Preset 1
                                        i=1;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x32:
                                        // Preset 2
                                        i=2;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x33:
                                        // Preset 3
                                        i=3;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x34:
                                        // Preset 4
                                        i=4;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x35:
                                        // Preset 5
                                        i=5;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x36:
                                        // Preset 6
                                        i=6;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x37:
                                        // Preset 7
                                        i=7;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x38:
                                        // Preset 8
                                        i=8;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                                case 0x39:
                                        // Preset 9
                                        i=9;
                                        if ((GetKeyState(VK_SHIFT) & 0x8000) || preset[camera_num][i].set == 0) {
                                            VISCA_get_zoom_value(&iface, &camera, &preset[camera_num][i].zoom);
                                            VISCA_get_pantilt_position(&iface,&camera,&preset[camera_num][i].pan,&preset[camera_num][i].tilt);
                                            preset[camera_num][i].set=1;
                                            sprintf(program_action,"Set preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);

                                        }
                                        else {
                                            sprintf(program_action,"Recall preset %d",i);
                                            sprintf(program_action,"%s %d %d %d", program_action, preset[camera_num][i].pan,preset[camera_num][i].tilt,preset[camera_num][i].zoom);                                            
                                            VISCA_clear(&iface, &camera);                   // Clear the interface                                            
                                            VISCA_set_zoom_value(&iface,&camera,preset[camera_num][i].zoom);
                                            Sleep(100);
                                            VISCA_set_pantilt_absolute_position(&iface,&camera,24,20,preset[camera_num][i].pan/15,preset[camera_num][i].tilt/15);
                                        }
                                        break;

                        }
                        break;

                //any message you don't process gets passed to this function inside Windows.
                default:
                        return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        //any message you do process, return 0 (unless the message documentation says otherwise)
return 0;
}

//---------------------------------------------------------------------------------------
// WinMain
//---------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
        WNDCLASSEX clas;
        MSG msg;
        int style;
        RECT rect;
        unsigned ThreadID;
        HRESULT err;
        char title[256];
        char com_port[10];
        char buff [255];

        //Here we create the Class we named above
        clas.cbSize = sizeof(WNDCLASSEX);
        clas.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        clas.lpfnWndProc = WindowProc;//<- tell it where the WindowProc is
        clas.cbClsExtra = 0;
        clas.cbWndExtra = 0;
        clas.hInstance = hInstance;
        clas.hIcon = NULL;
        clas.hCursor = NULL;
//        clas.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);//<- background colour of window
    clas.hIcon = LoadIcon( hInstance, "GenericIcon" );
    clas.hCursor = LoadCursor( NULL, IDC_ARROW );
    clas.hbrBackground = GetStockObject( WHITE_BRUSH );
    clas.lpszMenuName = "VISCAJoyMenu";
    clas.lpszClassName = VISCAJoyClass;
//        clas.lpszClassName = game_class;//<- the class name
        clas.hIconSm = 0;
        //do it!
        RegisterClassEx(&clas);

        //style of the window - what boxes do we need (close minimised etc)
        style = WS_CAPTION|WS_SYSMENU|WS_MAXIMIZEBOX|WS_MINIMIZEBOX;
        //create the window
        sprintf (title,"VISCAJoy %s",version);
        VISCAJoy_Window = CreateWindowEx(0, VISCAJoyClass, title, style, CW_USEDEFAULT, CW_USEDEFAULT, 1,1, NULL, NULL, hInstance, 0);

        //adjust the window size so that a SCREEN_X x SCREEN_Y window will fit inside its frame
        rect.left = rect.top = 0;
        rect.right = SCREEN_X;
        rect.bottom = SCREEN_Y;
        AdjustWindowRectEx(&rect, style , FALSE, 0);
        SetWindowPos(VISCAJoy_Window, NULL, 0,0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE|SWP_NOZORDER);

        //show the window on the desktop
        ShowWindow(VISCAJoy_Window, nCmdShow);

        if (init_dinput(hInstance) == 0)
        {
                MessageBox(NULL, "A problem occurred creating DirectInput.\nCheck your DirectX version.\n"
                                                        "This program requires DirectX9.0 or better.", "DirectX Initialization Error", MB_ICONWARNING|MB_OK);
                shutdown_dinput();
                return 0;
        }

        // Set the COM port
        if (strlen(lpCmdLine) == 0) { sprintf (com_port,COM_PORT);}
        else {strcpy(com_port,lpCmdLine);}

        if (init_visca(com_port) == 0) {
            sprintf(buff,"A problem occurred initializing VISCA on %s.\nCheck your camera connection.\nThis program requires a Sony EVI-D30 or compatible.\n",com_port);
                MessageBox(NULL, buff, "VISCA Initialization Error", MB_ICONWARNING|MB_OK);
                shutdown_dinput();
                return 0;
        }

        // Create a timer to refresh the window
        SetTimer(VISCAJoy_Window, 1, 150, NULL);

        // Do a test read on the joystick to make sure it can work
        if (joystick)
        {
            err = IDirectInputDevice8_Poll(joystick);
            err = IDirectInputDevice8_GetDeviceState(joystick, sizeof joydata, &joydata);
        }

        if (err)  {// Fail and bail
            MessageBox( NULL, "No joystick was found. Program will now exit", "Program Error", MB_OK );
            // Close everything down
                                        
            PostQuitMessage(0);            
            quit = 1;
            KillTimer(VISCAJoy_Window, 1);
            shutdown_dinput();
            return 0;
        }


        _beginthreadex(NULL, 0, read_joystick, (LPVOID)NULL, 0, &ThreadID);    

        //message processing loop
        //all Windows programs have one of these.  It receives the messages Windows sends to the program and
        //passes them to the WindowProc in the Class we registered for the window.
        quit = 0;
        do
        {
                //Are there any messages waiting?
                while (GetMessage(&msg, VISCAJoy_Window, 0,0) > 0 && !quit)
                {

                        //pass the message to WindowProc
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                }

        } while (!quit);

        // Close everything down
        KillTimer(VISCAJoy_Window, 1);

        shutdown_dinput();

        stop_threads=1;
        while (!joystick_thread_dead) {Sleep (1000);}

        return 0;
}


//---------------------------------------------------------------------------------------
// Create the DirectInput interface and access joystick
//---------------------------------------------------------------------------------------
int init_dinput(HINSTANCE hInst)
{
        HRESULT err;

        // Create the DirectInput interface
        err = DirectInput8Create(hInst, DIRECTINPUT_VERSION, &IID_IDirectInput8, &dinput, NULL);
        if (err != DI_OK)
                return 0;

        // Check for a joystick device, and create it if one exists
        err = IDirectInput8_EnumDevices(dinput, DI8DEVCLASS_GAMECTRL, dinput_enum_callback, 0, DIEDFL_ATTACHEDONLY);
        if (err != DI_OK) {
                return 0;
        }
        
        return 1;        
}


//-------------------------------------------------
// Show stuff on the screen
//-------------------------------------------------

void show_data (HDC hdc) {

        int line=0;
        char text[256];
    
    sprintf(text,"Joystick A -> VISCA");
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    sprintf(text,"Press spacebar to re-center joystick.");
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);

    sprintf(text,"Camera #: %d", joystick_a.address);
    TextOut(hdc, 0,(line++*16), text, strlen(text));

//  dump the camera info
          
    sprintf(text,"vendor: 0x%04x",camera.vendor);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
        
    sprintf(text,"model: 0x%04x",camera.model);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
          
    sprintf(text,"ROM version: 0x%04x",camera.rom_version);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
          
    sprintf(text,"socket number: 0x%02x",camera.socket_num);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);
    
    sprintf(text,"Camera Action: %s",joystick_a.action);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    sprintf(text,"Program Action: %s",program_action);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);

    sprintf(text,"X: %d  Center: %d",joystick_a.x,joystick_a.center_x);
    TextOut(hdc, 0,(line++*16), text, strlen(text));    
    sprintf(text,"Pan speed: %d",joystick_a.pan);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);

    sprintf(text,"Y: %d  Center: %d",joystick_a.y,joystick_a.center_y);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    sprintf(text,"Tilt speed: %d",joystick_a.tilt);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);

    sprintf(text,"Z: %d  Center: %d",joystick_a.z, joystick_a.center_z);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    sprintf(text,"Zoom: %d",joystick_a.zoom);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);

//    sprintf(text,"Speed %d",joystick_a.speed);
    sprintf(text,"Autofocus is %d",joystick_a.autofocus);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);
/*
    sprintf(text,"Slider %d",joystick_a.slider);
    TextOut(hdc, 0,(line++*16), text, strlen(text));

    sprintf(text,"Button_direction %d",joystick_a.button_direction);
    TextOut(hdc, 0,(line++*16), text, strlen(text));
    TextOut(hdc, 0,(line++*16), "\0", 0);
*/

    return;
    
}

int init_visca (char *serial_port) {

    int camera_num;

    if (VISCA_open_serial(&iface, serial_port)!=VISCA_SUCCESS)
    {
      //fprintf(stderr,"%s: unable to open serial device %s\n",argv[0],argv[1]);
        return 0;
    }

    else {  // Initialize the camera at address 1
        iface.broadcast=0;
        VISCA_set_address(&iface, &camera_num);
        joystick_a.address=1;
        camera.address=joystick_a.address;
        VISCA_clear(&iface, &camera);
        VISCA_get_camera_info(&iface, &camera);

        return 1;
    }
 
}



//---------------------------------------------------------------------------------------
// Callback to look at DirectInput devices to see if we're interested in them
//---------------------------------------------------------------------------------------
BOOL CALLBACK dinput_enum_callback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
        HRESULT err;

        err = IDirectInput8_CreateDevice(dinput, &lpddi->guidInstance, &joystick, NULL);
        if (err != DI_OK)
                return DIENUM_STOP;
        err = IDirectInputDevice8_SetDataFormat(joystick, &c_dfDIJoystick2);
        if (err != DI_OK)
        {
                IDirectInputDevice8_Release(joystick);
                joystick = NULL;
                return DIENUM_STOP;
        }
//        err = IDirectInputDevice8_SetCooperativeLevel(joystick, VISCAJoy_Window, DISCL_NONEXCLUSIVE|DISCL_FOREGROUND);
        err = IDirectInputDevice8_SetCooperativeLevel(joystick, VISCAJoy_Window, DISCL_NONEXCLUSIVE);
        err = IDirectInputDevice8_Acquire(joystick);

        return DIENUM_STOP;
}


//---------------------------------------------------------------------------------------
// Read data back from the joystick controller(s)
//---------------------------------------------------------------------------------------
HRESULT read_dinput_status(void)
{
        HRESULT err;

/*        // Read the keyboard
        err = IDirectInputDevice8_Poll(keyboard);
        err = IDirectInputDevice8_GetDeviceState(keyboard, 256, keymap);

        // Read the mouse
        err = IDirectInputDevice8_Poll(mouse);
        err = IDirectInputDevice8_GetDeviceState(mouse, sizeof mousedata, &mousedata);
*/
        // Read the joystick
        if (joystick)
        {
                err = IDirectInputDevice8_Poll(joystick);
                err = IDirectInputDevice8_GetDeviceState(joystick, sizeof joydata, &joydata);
        }

        if (err)  {// Assume we need to reacquire
              IDirectInputDevice8_Acquire(joystick);
        }

        if (joystick) {

                err = IDirectInputDevice8_Poll(joystick);
                err = IDirectInputDevice8_GetDeviceState(joystick, sizeof joydata, &joydata);
        }

        else {

        KillTimer(VISCAJoy_Window, 1);
            MessageBox( NULL, "Joystick was not found. Program will now exit", "Program Error", MB_OK );
        // Close everything down
                                        //tell Windows to close the app
                                        PostQuitMessage(0);
                                        quit = 1;

        }


        

        return err;
}

int center_joystick () {
    int read_result;

    Sleep (1000);

    read_result = read_dinput_status();

    joystick_a.center_x=joydata.lX;
    joystick_a.center_y=joydata.lY;
    joystick_a.center_slider=joydata.rglSlider[0];
    joystick_a.center_z=joydata.lRz;

    return read_result;   
}





/*
 * AboutDlgProc - processes messages for the about dialog.
 */
BOOL _EXPORT FAR PASCAL AboutDlgProc( HWND hwnd, unsigned msg,
                                UINT wparam, LONG lparam )
{
    lparam = lparam;                    /* turn off warning */

    switch( msg ) {
    case WM_INITDIALOG:
        return( TRUE );

    case WM_COMMAND:
        if( LOWORD( wparam ) == IDOK ) {
            EndDialog( hwnd, TRUE );
            return( TRUE );
        }
        break;
    }
    return( FALSE );

} /* AboutDlgProc */


//---------------------------------------------------------------------------------------
// Shutdown DirectInput
//---------------------------------------------------------------------------------------
void shutdown_dinput(void)
{
        if (joystick)
        {
                IDirectInputDevice8_Unacquire(joystick);
                IDirectInputDevice8_Release(joystick);
        }
        if (mouse)
        {
                IDirectInputDevice8_Unacquire(mouse);
                IDirectInputDevice8_Release(mouse);
        }
        if (keyboard)
        {
                IDirectInputDevice8_Unacquire(keyboard);
                IDirectInputDevice8_Release(keyboard);
        }
        if (dinput) IDirectInput8_Release(dinput);
}


