/*
 * klick - an advanced metronome for jack
 *
 * Copyright (C) 2007  Dominic Sacré  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "position.h"
#include "options.h"
#include "audio_interface.h"
#include "tempomap.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <boost/lambda/lambda.hpp>
#include <cmath>

using namespace std;
using namespace boost::lambda;


Position::Position(TempoMapConstPtr tempomap, float multiplier)
  : _tempomap(tempomap),
    _multiplier(multiplier)
{
    float_frames_t f = 0.0;
    int b = 0;

    // calculate first frame of each tempomap entry
    for (TempoMap::Entries::const_iterator i = tempomap->entries().begin(); i != tempomap->entries().end(); ++i) {
        _start_frames.push_back(f);
        _start_bars.push_back(b);
        if (i->bars != -1) {
            f += frame_dist(*i, 0, i->bars * i->beats);
            b += i->bars;
        } else {
            // play entry ad infinitum
            f = numeric_limits<float_frames_t>::max();
            b = numeric_limits<int>::max();
        }
    }

    // add end of tempomap
    _start_frames.push_back(f);
    _start_bars.push_back(b);

    reset();
}


void Position::reset()
{
    _frame = 0.0;
    _entry = _bar = _beat = 0;
    _bar_total = 0;
    _init = true;
    _end = false;
}


void Position::set_start_label(const string & start_label)
{
    TempoMapPtr t(new TempoMap());

    // remove everything before the start label
    TempoMap::Entries::const_iterator i = _tempomap->entries().begin();
    while (i->label != start_label) ++i;
    for ( ; i != _tempomap->entries().end(); ++i) t->add(*i);
    _tempomap = t;
}


void Position::add_preroll(int nbars)
{
    const TempoMap::Entry & e = (*_tempomap)[0];

    TempoMapPtr preroll;

    // create a new tempomap for preroll
    if (nbars == Options::PREROLL_2_BEATS) {
        vector<TempoMap::BeatType> pattern;
        for (int n = 0; n < e.denom; n++) {
            pattern.push_back(TempoMap::BEAT_NORMAL);
        }
        preroll = TempoMap::new_simple(1, e.tempo, 2, e.denom, pattern, 0.66f);
    } else {
        preroll = TempoMap::new_simple(nbars, e.tempo, e.beats, e.denom, e.pattern, 0.66f);
    }

    // join preroll and our actual tempomap
    _tempomap = TempoMap::join(preroll, _tempomap);
}


void Position::locate(nframes_t f)
{
    reset();

    if (f == 0) {
        // nothing else to do
        return;
    }

    // find the tempomap entry f is in
    _entry = distance(_start_frames.begin(),
                      upper_bound(_start_frames.begin(), _start_frames.end(), f) - 1);

    if (_entry == (int)_tempomap->size()) {
        // end of tempomap
        _end = true;
        return;
    }

    const TempoMap::Entry & e = (*_tempomap)[_entry];

    // difference between start of entry and desired position
    float_frames_t diff = f - _start_frames[_entry];
    double secs = diff / (float_frames_t)Audio->samplerate() * _multiplier;

    // constant tempo
    if (e.tempo && !e.tempo2) {
        int nbeats = (int)(secs / 240.0 * e.tempo * e.denom);

        _bar  = nbeats / e.beats;
        _beat = nbeats % e.beats;

        _frame = _start_frames[_entry] + frame_dist(e, 0, _bar * e.beats + _beat);
        _bar_total = _start_bars[_entry] + _bar;
    }
    // gradual tempo change
    else if (e.tempo && e.tempo2) {
        int nbeats = 0;

        // do a binary search for the beat at (or before) f.
        // this is not the most efficient implementation possible,
        // but good enough for now...
        int low = 0;
        int high = e.bars * e.beats;
        float_frames_t v = f - _start_frames[_entry];

        while(low <= high) {
            int mid = (low + high) / 2;
            float_frames_t d = frame_dist(e, 0, mid);
            if (d > v) {
                high = mid - 1;
            } else if (d <= v) {
                // unless there's a beat exactly at f, what we're really looking for
                // is the last beat before f.
                // check if mid+1 would be after f, in which case we're already done
                float_frames_t dd = frame_dist(e, 0, mid + 1);
                if (dd < v) {
                    low = mid + 1;
                } else {
                    // found it
                    nbeats = mid;
                    break;
                }
            }
        }

        _bar  = nbeats / e.beats;
        _beat = nbeats % e.beats;

        _frame = _start_frames[_entry] + frame_dist(e, 0, _bar * e.beats + _beat);
        _bar_total = _start_bars[_entry] + _bar;
    }
    // tempo per beat
    else if (!e.tempo) {
        _bar = _beat = 0;
        _frame = _start_frames[_entry];
        _bar_total = _start_bars[_entry];

        // terribly inefficient, but who uses tempo per beat anyway...?
        while (_frame + dist_to_next() <= f && !_end) {
            advance();
        }
    }

    // make sure we don't miss the first beat if it starts at f
    _init = (_frame == f);
}


void Position::advance()
{
    if (_init) {
        _init = false;
        return;
    }

    _frame += dist_to_next();

    const TempoMap::Entry & e = (*_tempomap)[_entry];

    // move to next beat
    if (++_beat >= e.beats) {
        _beat = 0;
        // move to next bar
        if (++_bar >= e.bars && e.bars != -1) {
            _bar = 0;
            // move to next entry
            if (++_entry >= (int)_tempomap->size()) {
                _entry--;       // no such entry
                _end = true;
            }
        }
        _bar_total++;
    }
}


Position::float_frames_t Position::dist_to_next() const
{
    // no valid next tick
    if (_init) return 0.0;
    if (_end) return numeric_limits<float_frames_t>::max();

    const TempoMap::Entry & e = (*_tempomap)[_entry];

    return frame_dist(e, _bar * e.beats + _beat,
                         _bar * e.beats + _beat + 1);
}


Position::float_frames_t Position::frame_dist(const TempoMap::Entry & e, int start, int end) const
{
    if (start == end) {
        return 0.0;
    }

    ASSERT(start < end);

    int nbeats = end - start;
    double secs = 0.0;

    // constant tempo
    if ((e.tempo && !e.tempo2) || (e.tempo && e.tempo == e.tempo2)) {
        secs = nbeats * 240.0 / (e.tempo * e.denom);
    }
    // gradual tempo change
    else if (e.tempo && e.tempo2) {
        double tdiff = e.tempo2 - e.tempo;

        double t1 = (double)e.tempo + tdiff * ((double)start / (e.bars * e.beats));
        double t2 = (double)e.tempo + tdiff * ((double)end   / (e.bars * e.beats));

        double avg_tempo = (t1 - t2) / (log(t1) - log(t2));
        secs = (nbeats * 240.0) / (avg_tempo * e.denom);
    }
    // different tempo for each beat
    else if (!e.tempo) {
        secs = accumulate(e.tempi.begin() + start,
                          e.tempi.begin() + end,
                          0.0, _1 + 240.0 / (_2 * e.denom));
    }

    return secs * (float_frames_t)Audio->samplerate() / _multiplier;
}


const Position::Tick Position::tick() const
{
    if (_end) {
        // end of tempomap, return "nothing"
        return (Tick) { (nframes_t)_frame, TempoMap::BEAT_SILENT, 0 };
    }

    const TempoMap::Entry & e = (*_tempomap)[_entry];

    TempoMap::BeatType t;
    if (e.pattern.empty()) {
        // use default pattern
        t = (_beat == 0) ? TempoMap::BEAT_EMPHASIS : TempoMap::BEAT_NORMAL;
    } else {
        // use pattern as specified in the tempomap
        t = e.pattern[_beat];
    }
    return (Tick) { (nframes_t)_frame, t, e.volume };
}