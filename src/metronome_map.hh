/*
 * klick - an advanced metronome for jack
 *
 * Copyright (C) 2007-2008  Dominic Sacré  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _METRONOME_MAP_HH
#define _METRONOME_MAP_HH

#include "metronome.hh"
#include "tempomap.hh"
#include "position.hh"

#include <string>

/*
 * plays a click track using a predefined tempomap
 */
class MetronomeMap
  : public Metronome,
    public AudioInterface::TimebaseCallback
{
  public:
    MetronomeMap(
        AudioInterface & audio,
        TempoMapConstPtr tempomap,
        float tempo_multiplier,
        bool transport,
        bool master,
        int preroll,
        std::string const & start_label
    );
    virtual ~MetronomeMap();

    bool running() const;

  protected:
    static double const TICKS_PER_BEAT = 1920.0;

    virtual void process_callback(sample_t *, nframes_t);
    virtual void timebase_callback(jack_position_t *);

    // transport position
    nframes_t _current;

    // position in tempomap
    Position _pos;

    bool _transport_enabled;
    bool _transport_master;
};


#endif // _METRONOME_MAP_HH