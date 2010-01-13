#include "Audio.h"
#include <mikmod.h>
#include <stdio.h>

static bool init_mikmod()
{
    /* libmikmod initialization boiler plate: */
    MikMod_RegisterAllDrivers();
    MikMod_RegisterAllLoaders();
    md_mode |= DMODE_SOFT_MUSIC;
    if (MikMod_Init((char*)"") != 0)
    {
        fprintf(stderr, "Could not initialize sound, reason: %s\n",
                        MikMod_strerror(MikMod_errno));
        return false;
    }
    if (MikMod_SetNumVoices(64, 16) != 0)
    {
        fprintf(stderr, "Could not reserve voices (64 music, 16 sound); "
                        "reason: %s\n", MikMod_strerror(MikMod_errno));
        return false;
    }
    return true;
}

Audio *Audio::instance()
{
    static Audio *audio = NULL;
    if (audio) return audio;
    if (init_mikmod()) audio = new Audio();
    return audio;
}

Audio::Audio() : module(NULL)
{
}

Audio::~Audio()
{
    unload_music();
    MikMod_Exit();
}

bool Audio::play_music(const char *path)
{
    MODULE *new_module = Player_Load((char*)path, 64, 0);
    if (!new_module)
    {
        fprintf(stderr, "Could not load module from %s, reason: %s\n",
                        path, MikMod_strerror(MikMod_errno));
        return false;
    }
    else
    {
        unload_music();
        Player_Start(module = new_module);
        return true;
    }
}

void Audio::unload_music()
{
    if (module)
    {
        Player_Stop();
        Player_Free(module);
        module = NULL;
    }
}

void Audio::update()
{
    MikMod_Update();
}

void Audio::play_sample(int index)
{
    if (!module) return;
    if (index < 0 || index >= module->numsmp) return;
    Sample_Play(&module->samples[index], 0, 0);
}
