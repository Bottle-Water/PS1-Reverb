//
//  Reverb.cpp
//  Ps1Reverb
//
//  Created by Zachary Burkett on 12/12/25.
//


#include "Reverb.h"
#include <algorithm>
#include <cmath>

Reverb::Reverb()
{
    loadPreset("Room");
}

void Reverb::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    const int size = std::max(1, regs.bufferSize);
    leftBuffer.assign(size, 0);
    rightBuffer.assign(size, 0);

    reset();
}

void Reverb::reset()
{
    std::fill(leftBuffer.begin(), leftBuffer.end(), 0);
    std::fill(rightBuffer.begin(), rightBuffer.end(), 0);
    bufferPos = 0;

    halfRatePhase = false;
    lastWetL = 0.0f;
    lastWetR = 0.0f;
}

// Helpers for PS1 math
// --------------------
float Reverb::ps1VolumeToFloat(int16_t ps1Vol)
{
    return juce::jmap(static_cast<float>(ps1Vol),
                      static_cast<float>(-0x8000),
                      static_cast<float>(0x7FFF),
                      -1.0f,
                      1.0f);
}

int32_t Reverb::clamp16(int32_t x) noexcept
{
    return juce::jlimit<int32_t>(-32768, 32767, x);
}

// signed 16 is present in PS1 audio mixing path
int16_t Reverb::floatToS16(float x) noexcept
{
    x = juce::jlimit(-1.0f, 1.0f, x);
    const int32_t v = (int32_t)std::lrintf(x * 32767.0f);
    return (int16_t)clamp16(v);
}

float Reverb::s16ToFloat(int16_t x) noexcept
{
    return (float)x / 32768.0f;
}

int16_t Reverb::mulQ15(int16_t a, int16_t b) noexcept
{
    // Q15 * Q15 -> Q30, then back to Q15 with rounding. Technique: https://sestevenson.wordpress.com/fixed-point-multiplication/
    int32_t p = (int32_t)a * (int32_t)b;
    p += 0x4000;
    p >>= 15;
    return (int16_t)clamp16(p);
}

int16_t Reverb::iirQ15(int16_t input, int16_t feedback, int16_t vIIR) noexcept
{
    // ((input - feedback) * vIIR) + feedback with saturation in between to avoid exploding feedback
    int32_t diff = (int32_t)input - (int32_t)feedback;
    diff = clamp16(diff);
    const int16_t scaled = mulQ15((int16_t)diff, vIIR);
    const int32_t out = (int32_t)scaled + (int32_t)feedback;
    return (int16_t)clamp16(out);
}

int16_t Reverb::readBufferS16(const std::vector<int16_t>& buffer, int offset) const noexcept
{
    const int size = (int)buffer.size();
    int idx = (bufferPos + offset) % size;
    if (idx < 0) idx += size;
    return buffer[idx];
}

void Reverb::writeBufferS16(std::vector<int16_t>& buffer, int offset, int16_t value) noexcept
{
    const int size = (int)buffer.size();
    int idx = (bufferPos + offset) % size;
    if (idx < 0) idx += size;
    buffer[idx] = value;
}

void Reverb::updateBufferSize()
{
    regs.bufferSize = std::max({
        regs.mLSAME1, regs.mRSAME1, regs.mLSAME2, regs.mRSAME2,
        regs.mLDIFF1, regs.mRDIFF1, regs.mLDIFF2, regs.mRDIFF2,
        regs.mLCOMB1, regs.mRCOMB1, regs.mLCOMB2, regs.mRCOMB2,
        regs.mLCOMB3, regs.mRCOMB3, regs.mLCOMB4, regs.mRCOMB4,
        regs.mLAPF1, regs.mRAPF1, regs.mLAPF2, regs.mRAPF2,
        regs.dAPF1, regs.dAPF2
    }) + 2048;

    // this should never happen but just in case the regs are all negative or something
    if (regs.bufferSize < 1024)
        regs.bufferSize = 1024;
}


// Preset loading
//
// 0: dAPF1
// 1: dAPF2
// 2: vIIR
// 3: vCOMB1
// 4: vCOMB2
// 5: vCOMB3
// 6: vCOMB4
// 7: vWALL
//
// 8: vAPF1
// 9: vAPF2
// 10: mLSAME1
// 11: mRSAME1
// 12: mLCOMB1
// 13: mRCOMB1
// 14: mLCOMB2
// 15: mRCOMB2
//
// 16: mLSAME2
// 17: mRSAME2
// 18: mLDIFF1
// 19: mRDIFF1
// 20: mLCOMB3
// 21: mRCOMB3
// 22: mLCOMB4
// 23: mRCOMB4
//
// 24: mLDIFF2
// 25: mRDIFF2
// 26: mLAPF1
// 27: mRAPF1
// 28: mLAPF2
// 29: mRAPF2
// 30: vLIN
// 31: vRIN
//
void Reverb::loaderSpuRegs(const uint16_t preset[32])
{

     // bad hack to make half echo sound good
    int addrScale = 4;
    if (preset[0] == 23)
    {
        addrScale = 2;
    }

    

    regs.dAPF1 = (int)preset[0] * addrScale;
    regs.dAPF2 = (int)preset[1] * addrScale;

    regs.vIIR   = (int16_t)preset[2];
    regs.vCOMB1 = (int16_t)preset[3];
    regs.vCOMB2 = (int16_t)preset[4];
    regs.vCOMB3 = (int16_t)preset[5];
    regs.vCOMB4 = (int16_t)preset[6];
    regs.vWALL  = (int16_t)preset[7];

    regs.vAPF1 = (int16_t)preset[8];
    regs.vAPF2 = (int16_t)preset[9];

    regs.mLSAME1 = (int)preset[10] * addrScale;
    regs.mRSAME1 = (int)preset[11] * addrScale;

    regs.mLCOMB1 = (int)preset[12] * addrScale;
    regs.mRCOMB1 = (int)preset[13] * addrScale;
    regs.mLCOMB2 = (int)preset[14] * addrScale;
    regs.mRCOMB2 = (int)preset[15] * addrScale;

    regs.mLSAME2 = (int)preset[16] * addrScale;
    regs.mRSAME2 = (int)preset[17] * addrScale;

    regs.mLDIFF1 = (int)preset[18] * addrScale;
    regs.mRDIFF1 = (int)preset[19] * addrScale;

    regs.mLCOMB3 = (int)preset[20] * addrScale;
    regs.mRCOMB3 = (int)preset[21] * addrScale;
    regs.mLCOMB4 = (int)preset[22] * addrScale;
    regs.mRCOMB4 = (int)preset[23] * addrScale;

    regs.mLDIFF2 = (int)preset[24] * addrScale;
    regs.mRDIFF2 = (int)preset[25] * addrScale;

    regs.mLAPF1 = (int)preset[26] * addrScale;
    regs.mRAPF1 = (int)preset[27] * addrScale;
    regs.mLAPF2 = (int)preset[28] * addrScale;
    regs.mRAPF2 = (int)preset[29] * addrScale;

    regs.vLIN = (int16_t)preset[30];
    regs.vRIN = (int16_t)preset[31];

    regs.vLOUT = floatToS16(0.5f);
    regs.vROUT = floatToS16(0.5f);

    // Float mirrors (maybe useful for parameters later)
    paramsFloat.vIIR   = ps1VolumeToFloat(regs.vIIR);
    paramsFloat.vWALL  = ps1VolumeToFloat(regs.vWALL);
    paramsFloat.vCOMB1 = ps1VolumeToFloat(regs.vCOMB1);
    paramsFloat.vCOMB2 = ps1VolumeToFloat(regs.vCOMB2);
    paramsFloat.vCOMB3 = ps1VolumeToFloat(regs.vCOMB3);
    paramsFloat.vCOMB4 = ps1VolumeToFloat(regs.vCOMB4);
    paramsFloat.vAPF1  = ps1VolumeToFloat(regs.vAPF1);
    paramsFloat.vAPF2  = ps1VolumeToFloat(regs.vAPF2);
    paramsFloat.vLIN   = ps1VolumeToFloat(regs.vLIN);
    paramsFloat.vRIN   = ps1VolumeToFloat(regs.vRIN);
    paramsFloat.vLOUT  = ps1VolumeToFloat(regs.vLOUT);
    paramsFloat.vROUT  = ps1VolumeToFloat(regs.vROUT);

    updateBufferSize();
}

// Processing

void Reverb::processStereo(float& leftIn, float& rightIn, float& leftOut, float& rightOut)
{
    // Run the reverb core every other sample (PS1 reverb runs at 22050Hz when SPU runs at 44100Hz)
    halfRatePhase = !halfRatePhase;

    float wetL = lastWetL;
    float wetR = lastWetR;

    if (halfRatePhase)
    {
        
        const int16_t inL = floatToS16(leftIn);
        const int16_t inR = floatToS16(rightIn);
        const int16_t Lin = mulQ15(inL, regs.vLIN);
        const int16_t Rin = mulQ15(inR, regs.vRIN);

        // same side reflections
        {
            const int16_t l1 = readBufferS16(leftBuffer, regs.mLSAME2);
            const int16_t l2 = readBufferS16(leftBuffer, regs.mLSAME1 - 2);

            const int16_t inSameL = (int16_t)clamp16((int32_t)Lin + (int32_t)mulQ15(l1, regs.vWALL));
            const int16_t outSameL = iirQ15(inSameL, l2, regs.vIIR);
            writeBufferS16(leftBuffer, regs.mLSAME1, outSameL);

            const int16_t r1 = readBufferS16(rightBuffer, regs.mRSAME2);
            const int16_t r2 = readBufferS16(rightBuffer, regs.mRSAME1 - 2);

            const int16_t inSameR = (int16_t)clamp16((int32_t)Rin + (int32_t)mulQ15(r1, regs.vWALL));
            const int16_t outSameR = iirQ15(inSameR, r2, regs.vIIR);
            writeBufferS16(rightBuffer, regs.mRSAME1, outSameR);
        }

        // diff side reflections
        {
            const int16_t r1 = readBufferS16(rightBuffer, regs.mRDIFF2);
            const int16_t l2 = readBufferS16(leftBuffer, regs.mLDIFF1 - 2);

            const int16_t inDiffL = (int16_t)clamp16((int32_t)Lin + (int32_t)mulQ15(r1, regs.vWALL));
            const int16_t outDiffL = iirQ15(inDiffL, l2, regs.vIIR);
            writeBufferS16(leftBuffer, regs.mLDIFF1, outDiffL);

            const int16_t l1 = readBufferS16(leftBuffer, regs.mLDIFF2);
            const int16_t r2 = readBufferS16(rightBuffer, regs.mRDIFF1 - 2);

            const int16_t inDiffR = (int16_t)clamp16((int32_t)Rin + (int32_t)mulQ15(l1, regs.vWALL));
            const int16_t outDiffR = iirQ15(inDiffR, r2, regs.vIIR);
            writeBufferS16(rightBuffer, regs.mRDIFF1, outDiffR);
        }

        // COMB sum
        int32_t combL =
            (int32_t)mulQ15(readBufferS16(leftBuffer, regs.mLCOMB1), regs.vCOMB1) +
            (int32_t)mulQ15(readBufferS16(leftBuffer, regs.mLCOMB2), regs.vCOMB2) +
            (int32_t)mulQ15(readBufferS16(leftBuffer, regs.mLCOMB3), regs.vCOMB3) +
            (int32_t)mulQ15(readBufferS16(leftBuffer, regs.mLCOMB4), regs.vCOMB4);

        int32_t combR =
            (int32_t)mulQ15(readBufferS16(rightBuffer, regs.mRCOMB1), regs.vCOMB1) +
            (int32_t)mulQ15(readBufferS16(rightBuffer, regs.mRCOMB2), regs.vCOMB2) +
            (int32_t)mulQ15(readBufferS16(rightBuffer, regs.mRCOMB3), regs.vCOMB3) +
            (int32_t)mulQ15(readBufferS16(rightBuffer, regs.mRCOMB4), regs.vCOMB4);

        int16_t Lout = (int16_t)clamp16(combL);
        int16_t Rout = (int16_t)clamp16(combR);

        // APF1
        {
            const int16_t apf1ReadL = readBufferS16(leftBuffer, regs.mLAPF1 - regs.dAPF1);
            const int16_t apf1InL = (int16_t)clamp16((int32_t)Lout - (int32_t)mulQ15(apf1ReadL, regs.vAPF1));
            writeBufferS16(leftBuffer, regs.mLAPF1, apf1InL);
            Lout = (int16_t)clamp16((int32_t)mulQ15(apf1InL, regs.vAPF1) + (int32_t)apf1ReadL);

            const int16_t apf1ReadR = readBufferS16(rightBuffer, regs.mRAPF1 - regs.dAPF1);
            const int16_t apf1InR = (int16_t)clamp16((int32_t)Rout - (int32_t)mulQ15(apf1ReadR, regs.vAPF1));
            writeBufferS16(rightBuffer, regs.mRAPF1, apf1InR);
            Rout = (int16_t)clamp16((int32_t)mulQ15(apf1InR, regs.vAPF1) + (int32_t)apf1ReadR);
        }

        // APF2
        {
            const int16_t apf2ReadL = readBufferS16(leftBuffer, regs.mLAPF2 - regs.dAPF2);
            const int16_t apf2InL = (int16_t)clamp16((int32_t)Lout - (int32_t)mulQ15(apf2ReadL, regs.vAPF2));
            writeBufferS16(leftBuffer, regs.mLAPF2, apf2InL);
            Lout = (int16_t)clamp16((int32_t)mulQ15(apf2InL, regs.vAPF2) + (int32_t)apf2ReadL);

            const int16_t apf2ReadR = readBufferS16(rightBuffer, regs.mRAPF2 - regs.dAPF2);
            const int16_t apf2InR = (int16_t)clamp16((int32_t)Rout - (int32_t)mulQ15(apf2ReadR, regs.vAPF2));
            writeBufferS16(rightBuffer, regs.mRAPF2, apf2InR);
            Rout = (int16_t)clamp16((int32_t)mulQ15(apf2InR, regs.vAPF2) + (int32_t)apf2ReadR);
        }

        // Output gain
        const int16_t wetOutL = mulQ15(Lout, regs.vLOUT);
        const int16_t wetOutR = mulQ15(Rout, regs.vROUT);

        wetL = s16ToFloat(wetOutL);
        wetR = s16ToFloat(wetOutR);

        lastWetL = wetL;
        lastWetR = wetR;

        bufferPos = (bufferPos + 1) % (int)leftBuffer.size();
    }

    leftOut = wetL;
    rightOut = wetR;
}


// Presets
// From: https://psx-spx.consoledev.net/soundprocessingunitspu/#spu-reverb-registers


void Reverb::loadPreset(const std::string& presetName)
{
    const uint16_t roomPreset[32] = {
        0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80,
        0x5800, 0x5300, 0x04D6, 0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF,
        0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000
    };

    const uint16_t studioSmallPreset[32] = {
        0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00,
        0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266,
        0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7,
        0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000
    };

    const uint16_t studioMediumPreset[32] = {
        0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0,
        0x5280, 0x4EC0, 0x0904, 0x076B, 0x0824, 0x065F, 0x07A2, 0x0616,
        0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305, 0x0462, 0x02B7,
        0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, 0x8000, 0x8000
    };

    const uint16_t studioLargePreset[32] = {
        0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680,
        0x5680, 0x52C0, 0x0DFB, 0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973,
        0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0, 0x06EF, 0x03D2,
        0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000
    };

    const uint16_t hallPreset[32] = {
        0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000,
        0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1,
        0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD,
        0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000
    };

    const uint16_t halfEchoPreset[32] = {
        0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500,
        0x5F80, 0x54C0, 0x0371, 0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7,
        0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1, 0x011F, 0x0059,
        0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000
    };

    const uint16_t spaceEchoPreset[32] = {
        0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000,
        0x6000, 0x5400, 0x1ED6, 0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2,
        0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D, 0x11F6, 0x0C5D,
        0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, 0x8000, 0x8000
    };

    const uint16_t chaosEchoPreset[32] = {
        0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100,
        0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000
    };

    const uint16_t delayPreset[32] = {
        0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000
    };

    const uint16_t offPreset[32] = {
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000
    };

    const uint16_t* preset = roomPreset;

    if (presetName == "Room") preset = roomPreset;
    else if (presetName == "Studio Small") preset = studioSmallPreset;
    else if (presetName == "Studio Medium") preset = studioMediumPreset;
    else if (presetName == "Studio Large") preset = studioLargePreset;
    else if (presetName == "Hall") preset = hallPreset;
    else if (presetName == "Half Echo") preset = halfEchoPreset;
    else if (presetName == "Space Echo") preset = spaceEchoPreset;
    else if (presetName == "Chaos Echo") preset = chaosEchoPreset;
    else if (presetName == "Delay") preset = delayPreset;
    else if (presetName == "Off") preset = offPreset;

    loaderSpuRegs(preset);

    // If already prepared, reallocate buffers to match new preset sizing
    if (!leftBuffer.empty() && !rightBuffer.empty())
    {
        leftBuffer.assign(regs.bufferSize, 0);
        rightBuffer.assign(regs.bufferSize, 0);
        reset();
    }
}
