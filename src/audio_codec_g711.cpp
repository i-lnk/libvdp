#include "audio_codec_g711.h"

static unsigned char alaw_encode(short pcm16)
{
    int p = pcm16;
    unsigned a;  // A-law value we are forming
    if(p<0)
    {
        // -ve value
        // Note, ones compliment is used here as this keeps encoding symetrical
        // and equal spaced around zero cross-over, (it also matches the standard).
        p = ~p;
        a = 0x00; // sign = 0
    }
    else
    {
        // +ve value
        a = 0x80; // sign = 1
    }

    // Calculate segment and interval numbers
    p >>= 4;
    if(p>=0x20)
    {
        if(p>=0x100)
        {
            p >>= 4;
            a += 0x40;
        }
        if(p>=0x40)
        {
            p >>= 2;
            a += 0x20;
        }
        if(p>=0x20)
        {
            p >>= 1;
            a += 0x10;
        }
    }
    // a&0x70 now holds segment value and 'p' the interval number

    a += p;  // a now equal to encoded A-law value

    return a^0x55;  // A-law has alternate bits inverted for transmission
}

static int alaw_decode(unsigned char alaw)
{
    alaw ^= 0x55;  // A-law has alternate bits inverted for transmission

    unsigned sign = alaw&0x80;

    int linear = alaw&0x1f;
    linear <<= 4;
    linear += 8;  // Add a 'half' bit (0x08) to place PCM value in middle of range

    alaw &= 0x7f;
    if(alaw>=0x20)
    {
        linear |= 0x100;  // Put in MSB
        unsigned shift = (alaw>>4)-1;
        linear <<= shift;
    }

    if(!sign)
        return -linear;
    else
        return linear;
}

unsigned int audio_alaw_enc(unsigned char *dst, short *src, unsigned int srcSize)
{
    srcSize >>= 1;
    unsigned char* end = dst+srcSize;
    while(dst<end)
        *dst++ = alaw_encode(*src++);

    return srcSize;
}

unsigned int audio_alaw_dec(short *dst, const unsigned char *src, unsigned int srcSize)
{
    short* end = dst+srcSize;
    while(dst<end)
        *dst++ = alaw_decode(*src++);
    return srcSize<<1;
}

