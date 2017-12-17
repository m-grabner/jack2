#include <algorithm>
#include <cstring>
#include <new>

#include "JackDelay.h"


JackDelay::JackDelay():
    fBufferSize(0), fBufferPos(0), fBuffer(NULL)
{
}

JackDelay::JackDelay(jack_nframes_t n):
    fBufferSize(0), fBufferPos(0), fBuffer(NULL)
{
    resize(n);
}

void JackDelay::resize(jack_nframes_t bufferSizeNew)
{
    if(bufferSizeNew == fBufferSize)
        return;

    if(bufferSizeNew == 0)
    {
        if(fBuffer != NULL)
            free(fBuffer);

        fBufferSize = 0;
        fBufferPos = 0;
        fBuffer = NULL;
        return;
    }

    jack_default_audio_sample_t *bufferNew = (jack_default_audio_sample_t *)malloc(bufferSizeNew * SAMPLE_SIZE);

    if(bufferNew == NULL)
        throw std::bad_alloc();

    if(fBuffer == NULL)
    {
        // clear new buffer:
        memset(bufferNew, 0, SAMPLE_SIZE * bufferSizeNew);
    }
    else
    {
        int diff = bufferSizeNew - fBufferSize;

        if(diff < 0)
        {
            // shrink buffer: copy content of old buffer to new buffer,
            // omitting frames at the current buffer position:
            jack_nframes_t pos1 = (fBufferPos - diff) % fBufferSize;
            jack_nframes_t pos2 = std::min(pos1 + bufferSizeNew, fBufferSize);
            jack_nframes_t length = pos2 - pos1;
            memcpy(bufferNew, fBuffer + pos1, SAMPLE_SIZE * length);

            if(length < bufferSizeNew)
                memcpy(bufferNew + length, fBuffer, SAMPLE_SIZE * (bufferSizeNew - length));
        }
        else
        {
            // enlarge buffer: insert zeros at current buffer position
            // and copy content of old buffer:
            memset(bufferNew, 0, SAMPLE_SIZE * diff);
            jack_nframes_t length = fBufferSize - fBufferPos;
            memcpy(bufferNew + diff, fBuffer + fBufferPos, SAMPLE_SIZE * length);

            if(length < fBufferSize)
                memcpy(bufferNew + diff + length, fBuffer, SAMPLE_SIZE * (fBufferSize - length));
        }

        free(fBuffer);
    }

    fBufferSize = bufferSizeNew;
    fBufferPos = 0;
    fBuffer = bufferNew;
}

void JackDelay::Process(const jack_default_audio_sample_t *in, jack_default_audio_sample_t *out, jack_nframes_t n)
{
    if(fBufferSize == 0)
    {
        // no delay -> just copy input to output:
        memcpy(out, in, SAMPLE_SIZE * n);
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
    {
        const jack_default_audio_sample_t *in1 = in;
        jack_default_audio_sample_t *out1 = out + fBufferSize;
        size_t bytes1 = SAMPLE_SIZE * length1;
        memcpy(out1, in1, bytes1);
    }

    /*
     * In the next step, copy data from the delay buffer starting at the
     * current position to the output buffer.
     */
    const jack_default_audio_sample_t *in2 = in + length1;
    jack_default_audio_sample_t *buffer2 = fBuffer + fBufferPos;
    jack_default_audio_sample_t *out2 = out;
    jack_nframes_t length2 = std::min(fBufferSize - fBufferPos, n);
    size_t bytes2 = SAMPLE_SIZE * length2;
    memcpy(out2, buffer2, bytes2);
    memcpy(buffer2, in2, bytes2);

    /*
     * Finally, in case of delay buffer wrap around, the remaining data from
     * the start of the delay buffer are copied.
     */
    jack_nframes_t length3 = n - (length1 + length2);

    if(length3 > 0)
    {
        size_t bytes3 = SAMPLE_SIZE * length3;
        const jack_default_audio_sample_t *in3 = in2 + length2;
        jack_default_audio_sample_t *buffer3 = fBuffer;
        jack_default_audio_sample_t *out3 = out + length2;
        memcpy(out3, buffer3, bytes3);
        memcpy(buffer3, in3, bytes3);
    }

    // advance buffer position:
    fBufferPos = (fBufferPos + n - length1) % fBufferSize;
}

JackDelay::~JackDelay()
{
    clear();
}
