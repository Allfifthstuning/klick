/*
 * klick - an advanced metronome for jack
 *
 * Copyright (C) 2007-2009  Dominic Sacré  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "audio_interface.hh"
#include "audio_chunk.hh"

#include "util/debug.hh"


AudioInterface::AudioInterface()
  : _next_chunk(0)
  , _volume(1.0f)
{
}


void AudioInterface::set_process_callback(ProcessCallback cb)
{
    _process_cb = cb;
}


void AudioInterface::play(AudioChunkConstPtr chunk, nframes_t offset, float volume)
{
    ASSERT(chunk->samplerate() == samplerate());

    _chunks[_next_chunk].chunk  = chunk;
    _chunks[_next_chunk].offset = offset;
    _chunks[_next_chunk].pos    = 0;
    _chunks[_next_chunk].volume = volume;

    _next_chunk = (_next_chunk + 1) % _chunks.size();
}


void AudioInterface::process_mix(sample_t *buffer, nframes_t nframes)
{
    for (ChunkArray::iterator i = _chunks.begin(); i != _chunks.end(); ++i)
    {
        if (i->chunk) {
            process_mix_samples(buffer + i->offset,
                                i->chunk->samples() + i->pos,
                                std::min(nframes - i->offset, i->chunk->length() - i->pos),
                                i->volume * _volume);

            i->pos += nframes - i->offset;
            i->offset = 0;

            if (i->pos >= i->chunk->length()) {
                i->chunk.reset();
            }
        }
    }
}


void AudioInterface::process_mix_samples(sample_t *dest, sample_t const * src, nframes_t length, float volume)
{
    for (sample_t *end = dest + length; dest < end; ++dest, ++src) {
        *dest += *src * volume;
    }
}
