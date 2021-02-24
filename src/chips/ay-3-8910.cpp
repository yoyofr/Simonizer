/*
MIT License

Copyright (c) 2020-2021 Aleksei Dynda

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "chips/ay-3-8910.h"

#include <stdint.h>
#include <stdlib.h>

#define AY38910_DEBUG 1

#if AY38910_DEBUG && !defined(VGM_DECODER_LOGGER)
#define VGM_DECODER_LOGGER 1
#endif
#include "../vgm_logger.h"


#define YM2149_PIN26_LOW   (0x10)

/*

Normalized voltage

 0      0.0000
 1      0.0137
 2      0.0205
 3      0.0291
 4      0.0423
 5      0.0618
 6      0.0847
 7      0.1369
 8      0.1691
 9      0.2647
10      0.3527
11      0.4499
12      0.5704
13      0.6873
14      0.8482
15      1.0000

 */

static uint16_t ay8910LevelTable[16] =
{
0,	183,	408,	683,	1020,	1433,	1939,	2558,	3317,	4246,	5385,	6779,	8487,	10579,	13142,	16281,
/*    2345,  3017,  3945,  4820,  5985,  7190,  8590,  10600,
    11370, 12890, 13620, 14275, 14760, 15090, 15350, 15950,*/
};

static uint16_t ym2149LevelTable[32] =
{
0,	81,	171,	270,	380,	501,	635,	783,
947,	1128,	1327,	1548,	1793,	2062,	2361,	2690,
3055,	3457,	3902,	4394,	4937,	5538,	6201,	6935,
7746,	8642,	9632,	10726,	11936,	13272,	14749,	16382,
};

enum
{
/*
 The frequency of each square wave generated by the three tone
 generators (channels A, B, C) is obtained by combining the contents
 of the Coarse and Fine Tune registers.
 FTR = fine tune register (provides the lower 8 bits)
 CTR = coarse tune register (provides the higher 4 bits)
*/

    FTR_A = 0,
    CTR_A = 1,
    FTR_B = 2,
    CTR_B = 3,
    FTR_C = 4,
    CTR_C = 5,
/*
 Noise period register (lower 5 bits, upper bits are not used)
 */
    NPR = 6,

    R_MIXER = 7,

/*
 The amplitudes of the signals generated by each of the three D/A
 converters (channels A, B, C) is determined by the contents of the
 lower 5 bits of the amplitude registers:
 b4 = amplitude mode (modulate envelope or period)
 b3 - b0 = fixed amplitude level
*/
    R_AMP_A = 8,
    R_AMP_B = 9,
    R_AMP_C = 10,
/*
 The frequency of the envelope is the 16 bit envelope period value.
 FTC = fine tune control (provides the lower 8 bits)
 CTC = coarse tune control (provides the higher 8 bits)
*/
    R_FPC_E = 11,
    R_CPC_E = 12,
    R_ENVELOPE = 13,
    R_RS232_A = 14,
    R_RS232_B = 15,
};


enum
{
    CHANNEL_A = 0,
    CHANNEL_B = 1,
    CHANNEL_C = 2,
};


AY38910::AY38910(uint8_t chipType, uint8_t flags)
{
    setType( chipType, flags );
}

void AY38910::setType( uint8_t chipType, uint8_t flags )
{
    m_chipType = chipType;
    m_flags = flags;
    reset();
}

void AY38910::setFrequency( uint32_t frequency )
{
    m_frequency = frequency * 2;
    reset();
}

void AY38910::setSampleFrequency( uint32_t sampleFrequency )
{
    m_sampleFrequency = sampleFrequency;
    reset();
}

void AY38910::setVolume(uint16_t volume)
{
    m_userVolume = volume;
    calcVolumeTables();
}

void AY38910::calcVolumeTables()
{
    if ( m_chipType & 0xF0 )
    {
        for (int i = 0; i < 32; i++)
        {
            uint32_t vol = static_cast<uint32_t>( ym2149LevelTable[i] ) * m_userVolume * 60 / (100 * 32);
            if ( vol > 65535 ) vol = 65535;
            m_levelTable[i] = vol;
        }
    }
    else
    {
        for (int i = 0; i < 16; i++)
        {
            uint32_t vol = static_cast<uint32_t>( ay8910LevelTable[i] ) * m_userVolume * 60 / (100 * 32);
            if ( vol > 65535 ) vol = 65535;
            m_levelTable[i] = vol;
        }
    }
}

void AY38910::reset()
{
    m_envStepMask = ( m_chipType & 0xF0 ) ? 0x1F: 0x0F;
    m_toneFrequencyScale = (m_frequency / m_sampleFrequency); // * 16
    m_envFrequencyScale = (m_frequency / m_sampleFrequency);  // * 256
    if ( !( m_chipType & 0xF0 ) )
    {
        m_envFrequencyScale /= 2;
    }

    if ( ( m_chipType & 0xF0 ) && ( m_flags & YM2149_PIN26_LOW ) )
    {
        m_envFrequencyScale /= 2;
    }

    for (int i=0; i<=R_ENVELOPE; i++) write(i, 0);
}

uint16_t AY38910::read(uint8_t reg)
{
    switch (reg)
    {
    case FTR_A:
        return (m_period[CHANNEL_A] >> 4) & 0xFF;
    case CTR_A:
        return (m_period[CHANNEL_A] >> 12) & 0x0F;
    case FTR_B:
        return (m_period[CHANNEL_B] >> 4) & 0xFF;
    case CTR_B:
        return (m_period[CHANNEL_B] >> 12) & 0x0F;
    case FTR_C:
        return (m_period[CHANNEL_C] >> 4) & 0xFF;
    case CTR_C:
        return (m_period[CHANNEL_C] >> 12) & 0x0F;
    case NPR:
        return (m_periodNoise >> 4);
    case R_MIXER:
        return m_mixer;
    case R_AMP_A:
        return m_ampR[CHANNEL_A];
    case R_AMP_B:
        return m_ampR[CHANNEL_B];
    case R_AMP_C:
        return m_ampR[CHANNEL_C];
    case R_FPC_E:
        return (m_periodE >> 8) & 0xff;
    case R_CPC_E:
        return (m_periodE >> 16) & 0xff00;
    case R_ENVELOPE:
        return m_envelopeReg;
    case R_RS232_A:
    case R_RS232_B:
        return 0xff;
    default:
        return 0;
    }
}

void AY38910::write(uint8_t reg, uint16_t value)
{
    LOGI( ">> reg: %d = 0x%02X\n", reg, value);
    switch (reg)
    {
    case FTR_A:
        m_period[CHANNEL_A] = ((m_period[CHANNEL_A] & 0xf000) | (value << 4));
        break;
    case CTR_A:
        m_period[CHANNEL_A] = (((value & 0x0f) << 12) | (m_period[CHANNEL_A] & 0xff0));
        break;
    case FTR_B:
        m_period[CHANNEL_B] = ((m_period[CHANNEL_B] & 0xf000) | (value << 4));
        break;
    case CTR_B:
        m_period[CHANNEL_B] = (((value & 0x0f) << 12) | (m_period[CHANNEL_B] & 0xff0));
        break;
    case FTR_C:
        m_period[CHANNEL_C] = ((m_period[CHANNEL_C] & 0xf000) | (value << 4));
        break;
    case CTR_C:
        m_period[CHANNEL_C] = (((value & 0x0f) << 12) | (m_period[CHANNEL_C] & 0xff0));
        break;
    case NPR:
        m_periodNoise = (value & 0x1f) << 4;
        break;
    case R_MIXER:
        m_mixer = value;
        break;

    case R_AMP_A:
    case R_AMP_B:
    case R_AMP_C:
    {
        uint8_t channel = reg - R_AMP_A;
        m_ampR[channel] = value;
        m_amplitude[channel] = (value & 0x0f);
        // For YM2149 type chips there 32 levels instead of 16
        if ( m_chipType & 0xF0 ) m_amplitude[channel] <<= 1;
        m_useEnvelope[channel] = ((value & 0x10) != 0);
        break;
    }
    case R_FPC_E:
        m_periodE = ((m_periodE & 0xff0000) | (value << 8));
        break;
    case R_CPC_E:
        m_periodE = ((value << 16) | (m_periodE & 0x00ff00));
        break;
    case R_ENVELOPE:
        m_envelopeReg = value;
        m_holding = false;
        m_hold = !!(value & 0x01); // true means one cycle only
        m_alternate = !!(value & 0x02); // true means change direction after each cycle
        // When both the Hold bit and the Alternate bit are ones, the envelope counter is reset to its initial count before holding.
        m_attack = !!(value & 0x04); // if true from 0000 to 1111, otherwise from 1111 to 0000
        m_continue = !!(value & 0x08); // if true hold bit, otherwise hold is enabled
        if ( !m_continue ) m_hold = true;
        m_envVolume = m_attack ? 0 : m_envStepMask;

        m_counterEnv = 0;
        break;
    case R_RS232_A:
    case R_RS232_B:
        break;
    default:
        LOGE( "Unknown register %02X\n", reg );
        break;
    }
}

uint32_t AY38910::getSample()
{
    for (int i=0; i<3; i++)
    {
        m_counter[i] += m_toneFrequencyScale;
        if (m_counter[i] >= m_period[i])
        {
            m_channelOutput[i] = !m_channelOutput[i];
            m_counter[i] = 0;
        }
    }

    m_counterNoise += m_toneFrequencyScale;
    if (m_counterNoise >= m_periodNoise)
    {
        m_counterNoise = 0;
        m_noiseRecalc = !m_noiseRecalc;
        if ( m_noiseRecalc )
        {
            // The Random Number Generator of the 8910 is a 17-bit shift
            // register. The input to the shift register is bit0 XOR bit3
            // (bit0 is the output).
            m_rng ^= (((m_rng & 1) ^ ((m_rng >> 3) & 1)) << 17);
            m_rng >>= 1;
            m_noiseHigh = !!(m_rng & 1);
        }
    }

    if ( !m_holding )
    {
        if (m_periodE > 0)
        {
            m_counterEnv += m_envFrequencyScale;
            // The envelope counter runs at half the speed of the
            // tone counter, so double the envelope period
            if (m_counterEnv >= m_periodE)
            {
                m_counterEnv = 0;
                m_envVolume += m_attack ? 1: -1;
                if ( m_envVolume > m_envStepMask ) // if overflow happened, we reached the boundary: low or high
                {
                    m_holding = m_hold;
                    // step back
                    m_envVolume -= m_attack ? 1: -1;
                    if ( !m_continue ) m_envVolume = 0;
                    else if ( m_alternate && m_hold ) m_envVolume ^= m_envStepMask;
                    else if ( !m_hold && !m_alternate ) m_envVolume ^= m_envStepMask;
                    else if ( !m_hold && m_alternate ) m_attack = !m_attack;
                }
            }
        }
    }

    uint32_t left = 0;
    uint32_t right = 0;

    for(int chan=0; chan<3; chan++)
    {
// Two variant for calculating enable field. Both work
//        bool enabled = ( ((m_mixer >> chan) & 1)  || m_channelOutput[chan] ) &&
//                       ( ((m_mixer >> (3 + chan)) & 1) || m_noiseHigh );
        bool enabled = ( ((m_mixer >> chan) & 1) == 0 && m_channelOutput[chan] ) ||
                       ( ((m_mixer >> (3 + chan)) & 1) == 0 && m_noiseHigh );
        if (m_useEnvelope[chan])
        {
            // TODO: Evelope must have it's own table
            left += m_levelTable[enabled ? m_envVolume: 0];
            right += m_levelTable[enabled ? m_envVolume: 0];
        }
        else
        {
            left += m_levelTable[enabled ? m_amplitude[chan]: 0];
            right += m_levelTable[enabled ? m_amplitude[chan]: 0];
        }
    }
/*    left /= 3;*/ if ( left > 65535 ) left = 65535;
/*    right /= 3;*/ if ( right > 65535 ) right = 65535;
    return (left<<16) | right;
}
