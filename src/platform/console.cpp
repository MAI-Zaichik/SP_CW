#include "platform/console.h"

#ifdef _WIN32
#include <windows.h>
#endif

// windows.h
void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}
