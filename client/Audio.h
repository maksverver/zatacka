#ifndef AUDIO_H_INCLUDED
#define AUDIO_H_INCLUDED

class MODULE;

class Audio
{
public:
    static Audio *instance();
    bool play_music(const char *path);
    void play_sample(int index);
    void update();

private:
    void unload_music();

    Audio();
    ~Audio();
    Audio(const Audio &);
    Audio &operator=(const Audio &);

private:
    MODULE *module;
};

#endif /* ndef AUDIO_H_INCLUDED */
