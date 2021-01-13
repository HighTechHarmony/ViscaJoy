typedef struct {
char far *cmdline;
} extra_data;

#define EXTRA_DATA_OFFSET       0

#define MENU_ABOUT              5
#define MENU_EXIT               6

#ifdef __WINDOWS_386__
#define _EXPORT
#else
#define _EXPORT __export
#endif
