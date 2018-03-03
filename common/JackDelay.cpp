#include <algorithm>
#include <cstring>
#include <new>

#include "JackDelay.h"
#include "JackError.h"


#if JACK_DELAY_MAX

JackDelay::JackDelay():
    fBuffer(nullptr), fBufferSize(0), fBufferPos(0)
{
}

JackDelay::JackDelay(jack_nframes_t n):
    fBuffer(nullptr), fBufferSize(0), fBufferPos(0)
{
    resize(n);
}

void JackDelay::resize(jack_nframes_t bufferSizeNew)
{
    if(fBuffer == nullptr) {
        uintptr_t bufferMemoryAddress = reinterpret_cast<uintptr_t>(fBufferMemory);
        uintptr_t bufferAddress = (bufferMemoryAddress + 7) & ~static_cast<uintptr_t>(7);  // round up to 8-byte-aligned address
        fBuffer = reinterpret_cast<jack_default_audio_sample_t *>(bufferAddress);
    }

    if(bufferSizeNew == fBufferSize)
        return;

    if(bufferSizeNew > JACK_DELAY_MAX) {
        jack_error("maximum delay exceeded (%d > %d)", bufferSizeNew, JACK_DELAY_MAX);
        return;
    }

    jack_default_audio_sample_t *bufferEnd = fBuffer + fBufferSize;

    if(bufferSizeNew > fBufferSize) {
        // enlarge delay buffer -> insert zeros at beginning:
        jack_nframes_t diff = bufferSizeNew - fBufferSize;
        std::copy_backward(fBuffer, bufferEnd, bufferEnd + diff);
        std::fill(fBuffer, fBuffer + diff, 0);
    }
    else {
        // shrink delay buffer -> skip data at beginning:
        jack_nframes_t diff = fBufferSize - bufferSizeNew;
        std::copy(fBuffer + diff, bufferEnd, fBuffer);
    }

    fBufferSize = bufferSizeNew;
    fBufferPos = 0;
}

void JackDelay::Process(const jack_default_audio_sample_t *in, jack_default_audio_sample_t *out, jack_nframes_t n)
{
    if(empty())
    {
        // no delay -> just copy input to output:
        std::copy(in, in + n, out);
        return;
    }

    /*
     * If the number of frames to be copied is larger than the buffer size,
     * a part of the input data can be directly copied to the output. This
     * region is processed first since some other quantities below depend on
     * its size.
     */
    int length1 = std::max(static_cast<int>(n - fBufferSize), 0);

    if(length1 > 0)
        std::copy(in, in + length1, out + fBufferSize);

    /*
     * In the next step, copy data from the delay buffer starting at the
     * current position to the output buffer.
     */
    const jack_default_audio_sample_t *in2 = in + length1;
    jack_default_audio_sample_t *buffer2 = fBuffer + fBufferPos;
    jack_nframes_t length2 = std::min(fBufferSize - fBufferPos, n);
    std::copy(buffer2, buffer2 + length2, out);
    std::copy(in2, in2 + length2, buffer2);

    /*
     * Finally, in case of delay buffer wrap around, the remaining data from
     * the start of the delay buffer are copied.
     */
    jack_nframes_t length3 = n - (length1 + length2);

    if(length3 > 0)
    {
        const jack_default_audio_sample_t *in3 = in2 + length2;
        std::copy(fBuffer, fBuffer + length3, out + length2);
        std::copy(in3, in3 + length3, fBuffer);
    }

    // advance buffer position:
    fBufferPos = (fBufferPos + n - length1) % fBufferSize;
}

void JackDelay::Process(jack_default_audio_sample_t *inout, jack_nframes_t n)
{
    if(empty())
        return;  // no delay -> nothing to be done

    jack_nframes_t blockSize = std::min(n, fBufferSize);
    jack_nframes_t length1 = n - blockSize;
    jack_nframes_t length2 = std::min(blockSize, fBufferSize - fBufferPos);
    jack_default_audio_sample_t *inout2 = inout + length2;
    jack_nframes_t length3 = blockSize - length2;

    std::rotate(inout, inout + length1, inout + n);
    std::swap_ranges(inout, inout2, fBuffer + fBufferPos);
    std::swap_ranges(inout2, inout2 + length3, fBuffer);

    fBufferPos = (fBufferPos + n - length1) % fBufferSize;
}

JackDelay::~JackDelay()
{
    clear();
}

#endif
