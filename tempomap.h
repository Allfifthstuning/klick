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

#ifndef _TEMPOMAP_H
#define _TEMPOMAP_H

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

#include <regex.h>


class TempoMap
{
  public:
    enum BeatType {
        BEAT_EMPHASIS = 1,
        BEAT_NORMAL,
        BEAT_SILENT
    };

    struct Entry {
        std::string label;
        uint bars;
        float tempo;                    // zero if tempo is given for each beat
        float tempo2;                   // zero if tempo is constant
        std::vector<float> tempi;       // empty unless tempo == 0.0
        uint beats;
        uint denom;
        std::vector<BeatType> accents;  // empty if default
        float volume;
    };

    typedef std::vector<Entry> Entries;

    // helper class for regex parsing
    class Regex
    {
      public:
        Regex(const std::string &regex, int flags) {
            regcomp(&_re, regex.c_str(), flags);
        }
        ~Regex() {
            regfree(&_re);
        }
        bool match(const std::string &str, int nmatch, regmatch_t pmatch[], int flags) const {
            return (regexec(&_re, str.c_str(), nmatch, pmatch, flags) == 0);
        }
      protected:
        regex_t _re;
    };

    // get all entries
    const Entries & entries() const { return _entries; }
    // get n'th entry
    const Entry & entry(uint n) const { return _entries[n]; }
    const Entry & operator[](uint n) const { return _entries[n]; }
    // get number of entries
    uint size() const { return _entries.size(); }

    // get entry with label l, NULL if no such entry exists
    const Entry * entry(const std::string & l) const {
        if (l.empty()) return NULL;
        for (Entries::const_iterator i = _entries.begin(); i != _entries.end(); ++i) {
            if (i->label == l) return &*i;
        }
        return NULL;
    }

    void add(const Entry & e) {
        _entries.push_back(e);
    }

    std::string dump() const;

    static boost::shared_ptr<TempoMap> join(boost::shared_ptr<const TempoMap>,
                                            boost::shared_ptr<const TempoMap>);

    static boost::shared_ptr<TempoMap> new_from_file(const std::string & filename);
    static boost::shared_ptr<TempoMap> new_from_cmdline(const std::string & line);

    static boost::shared_ptr<TempoMap> new_simple(uint bars, float tempo, uint beats, uint denom,
                                                  const std::vector<BeatType> & acc = std::vector<BeatType>(),
                                                  float volume = 1.0f);

  protected:
    // determines whether the submatch m is not empty
    static bool is_specified(const std::string &s, const regmatch_t &m) {
        return ((m.rm_eo - m.rm_so) != 0);
    }
    // get submatch m from the line s as string
    static std::string extract_string(const std::string &s, const regmatch_t &m) {
        uint len = m.rm_eo - m.rm_so;
        return len ? std::string(s.c_str() + m.rm_so, len) : "";
    }
    // get submatch m from the line s as int
    static uint extract_int(const std::string &s, const regmatch_t &m) {
        uint len = m.rm_eo - m.rm_so;
        return len ? atoi(std::string(s.c_str() + m.rm_so, len).c_str()) : 0;
    }
    // get submatch m from the line s as float
    static float extract_float(const std::string &s, const regmatch_t &m) {
        uint len = m.rm_eo - m.rm_so;
        return len ? atof(std::string(s.c_str() + m.rm_so, len).c_str()) : 0.0f;
    }

    // builds a vector of beat types, based on the string description
    static std::vector<BeatType> parse_accents(const std::string &s, uint nbeats);

    static std::vector<float> parse_tempi(const std::string &s, float tempo1, uint nbeats_total);

    Entries _entries;
};


#endif // _TEMPOMAP_H
