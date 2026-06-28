/*
Copyright (C) 2007-2008 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

//
// snd_sdl.c
//

#include "shared/shared.h"
#include "common/zone.h"
#include "client/sound/dma.h"
#include <SDL3/SDL.h>

static void Filler(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    int size = dma.samples << 1;
    int pos = dma.samplepos << 1;
    int wrapped = pos + additional_amount - size;

    if (wrapped < 0) {
        SDL_PutAudioStreamData(stream, dma.buffer + pos, additional_amount);
        dma.samplepos += additional_amount >> 1;
    } else {
        int remaining = size - pos;
        SDL_PutAudioStreamData(stream, dma.buffer + pos, remaining);
        SDL_PutAudioStreamData(stream, dma.buffer, wrapped);
        dma.samplepos = wrapped >> 1;
    }
}

static SDL_AudioStream *audio_stream = NULL;

static void Shutdown(void)
{
    Com_Printf("Shutting down SDL audio.\n");

    SDL_DestroyAudioStream(audio_stream);
    audio_stream = NULL;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    Z_Freep(&dma.buffer);
}

static sndinitstat_t Init(void)
{
    SDL_AudioSpec desired, obtained;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        Com_EPrintf("Couldn't initialize SDL audio: %s\n", SDL_GetError());
        return SIS_FAILURE;
    }

    memset(&desired, 0, sizeof(desired));
    switch (s_khz->integer) {
    case 48:
        desired.freq = 48000;
        break;
    case 44:
        desired.freq = 44100;
        break;
    case 22:
        desired.freq = 22050;
        break;
    default:
        desired.freq = 11025;
        break;
    }

    desired.format = SDL_AUDIO_S16LE;
    desired.channels = 2;
    audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, Filler, NULL);
    if (audio_stream == 0) {
        Com_EPrintf("Couldn't open SDL audio: %s\n", SDL_GetError());
        goto fail1;
    }
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio_stream), &obtained, NULL);

    if (obtained.format != SDL_AUDIO_S16LE) {
        Com_EPrintf("SDL audio format %d unsupported.\n", obtained.format);
        goto fail2;
    }

    if (obtained.channels != 1 && obtained.channels != 2) {
        Com_EPrintf("SDL audio channels %d unsupported.\n", obtained.channels);
        goto fail2;
    }

    dma.speed = obtained.freq;
    dma.channels = obtained.channels;
    dma.samples = 0x8000 * obtained.channels;
    dma.submission_chunk = 1;
    dma.samplebits = 16;
    dma.buffer = Z_Mallocz(dma.samples * 2);
    dma.samplepos = 0;

    Com_Printf("Using SDL audio driver: %s\n", SDL_GetCurrentAudioDriver());

    SDL_ResumeAudioStreamDevice(audio_stream);

    return SIS_SUCCESS;

fail2:
    SDL_DestroyAudioStream(audio_stream);
    audio_stream = NULL;
fail1:
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return SIS_FAILURE;
}

static void BeginPainting(void)
{
    SDL_LockAudioStream(audio_stream);
}

static void Submit(void)
{
    SDL_UnlockAudioStream(audio_stream);
}

static void Activate(bool active)
{
    if (active) {
        SDL_ResumeAudioStreamDevice(audio_stream);
    } else {
        SDL_PauseAudioStreamDevice(audio_stream);
    }
}

const snddma_driver_t snddma_sdl = {
    .name = "sdl",
    .init = Init,
    .shutdown = Shutdown,
    .begin_painting = BeginPainting,
    .submit = Submit,
    .activate = Activate,
};
