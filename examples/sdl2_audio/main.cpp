#include <SDL2/SDL.h>
#include <adlcpp.h>
#include <signal.h>

/* prototype for our audio callback */
/* see the implementation for more information */
void my_audio_callback(void *midi_player, Uint8 *stream, int len);

/* variable declarations */
static Uint32 is_playing = 0; /* remaining length of the sample we have to play */
static short buffer[4096]; /* Audio buffer */


void hitCtrlC(int)
{
    is_playing = 0;
    signal(SIGINT, SIG_DFL);
}


int main(int argc, char *argv[])
{
    /* local variables */
    static SDL_AudioSpec            spec; /* the specs of our piece of music */
    static const char               *music_path = NULL; /* Path to music file */

    if (argc < 2)
    {
        fprintf(stderr, "\n"
                        "\n"
                        "No given files to play!\n"
                        "\n"
                        "Syntax: %s <path-to-MIDI-file>\n"
                        "\n", argv[0]);
        return 2;
    }

    signal(SIGINT, &hitCtrlC);

    music_path = argv[1];

    /* Initialize SDL.*/
    if(SDL_Init(SDL_INIT_AUDIO) < 0)
        return 1;

    spec.freq = 48000;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 2048;

    AdlSimpleMidiPlay midiPlay;

    /* set the callback function */
    spec.callback = my_audio_callback;
    /* set ADLMIDI's descriptor as userdata to use it for sound generation */
    spec.userdata = &midiPlay;

    /* Open the audio device */
    if (SDL_OpenAudio(&spec, NULL) < 0)
    {
        fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
        return 1;
    }

    midiPlay.SetLoopEnabled(false);
    midiPlay.SetSampleRate(spec.freq);
    midiPlay.SetCardsNum(4);
    midiPlay.SetBankNo(59);

    /* Open the MIDI (or MUS, IMF or CMF) file to play */
    if(!midiPlay.LoadMidi(music_path))
    {
        fprintf(stderr, "Couldn't open music file!\n");
        SDL_CloseAudio();
        return 1;
    }

    is_playing = 1;
    /* Start playing */
    SDL_PauseAudio(0);

    printf("Playing... Hit Ctrl+C to quit!\n");

    /* wait until we're don't playing */
    while (is_playing)
    {
        SDL_Delay(100);
    }

    /* shut everything down */
    SDL_CloseAudio();

    return 0;
}

/*
 audio callback function
 here you have to copy the data of your audio buffer into the
 requesting audio buffer (stream)
 you should only copy as much as the requested length (len)
*/
void my_audio_callback(void *midi_player, Uint8 *stream, int len)
{
    AdlSimpleMidiPlay* p = (struct AdlSimpleMidiPlay*)midi_player;

    /* Take some samples from the ADLMIDI */
    int samples_count = p->Play((short*)buffer, len / 4);

    if(samples_count <= 0)
    {
        is_playing = 0;
        SDL_memset(stream, 0, len);
        return;
    }

    /* Send buffer to the audio device */
    SDL_memcpy(stream, (Uint8*)buffer, len);
}

