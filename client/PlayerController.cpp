#include "PlayerController.h"
#include "common.h"
#include <string.h>
#include <stdio.h>

#ifndef WIN32
#include <dlfcn.h>
static PlayerController *load_bot_posix(const char *name)
{
    char path[64];

    /* Ensure filename is sensible */
    if (strchr(name, '/') || strchr(name, '.') || strlen(name) > 40)
    {
        return NULL;
    }

    sprintf(path, "bots/%s.so", name);
    void *dlh = dlopen(path, RTLD_NOW);
    if (dlh == NULL)
    {
        info("dlopen failed -- %s", dlerror());
        return NULL;
    }

    void *func = dlsym(dlh, "create_bot");
    if (func == NULL)
    {
        info("dlsym failed -- %s", dlerror());
        dlclose(dlh);
        return NULL;
    }

    PlayerController *pc = ((PlayerController*(*)())func)();
    if (pc == NULL) info("%s: create_bot() returned NULL", path);
    return pc;
}
#endif /* ndef WIN32 */

#ifdef WIN32
static PlayerController *load_bot_win32(const char *name)
{
    char path[64];

    /* Ensure filename is sensible */
    if (strchr(name, '/') || strchr(name, '.') || strchr(name, '\\') || strlen(name) > 40)
    {
        return NULL;
    }

    sprintf(path, "bots\\%s.dll", name);
    HMODULE hModule = LoadLibrary(path);
    if (hModule == NULL)
    {
        info("Couldn't load library %s", path);
        return NULL;
    }

    void *func = (void*)GetProcAddress(hModule, "create_bot");
    if (func == NULL)
    {
        info("Couldn't find function create_bot() in %s", path);
        FreeLibrary(hModule);
        return NULL;
    }

    PlayerController *pc = ((PlayerController*(*)())func)();
    if (pc == NULL) info("%s: create_bot() returned NULL", path);
    return pc;
}
#endif /* def WIN32 */

PlayerController *PlayerController::load_bot(const char *name)
{
#ifdef WIN32
    return load_bot_win32(name);
#else
    return load_bot_posix(name);
#endif
}
