#ifndef DELAY_HPP
#define DELAY_HPP


#include <cstdlib>

#include <jack/types.h>


PRE_PACKED_STRUCTURE
class JackDelay
{
private:
    static const size_t SAMPLE_SIZE = sizeof(jack_default_audio_sample_t);

    jack_nframes_t fBufferSize;
    jack_nframes_t fBufferPos;
    jack_default_audio_sample_t *fBuffer;

public:
    JackDelay();
    JackDelay(jack_nframes_t n);
    ~JackDelay();

    void clear();
    bool empty() const;
    jack_nframes_t size() const;
    void resize(jack_nframes_t n);

    void Process(const jack_default_audio_sample_t *in, jack_default_audio_sample_t *out, jack_nframes_t n);
} POST_PACKED_STRUCTURE;

inline jack_nframes_t JackDelay::size() const
{
    return fBufferSize;
}

inline bool JackDelay::empty() const
{
    return size() == 0;
}

inline void JackDelay::clear()
{
    resize(0);
}


#endif
