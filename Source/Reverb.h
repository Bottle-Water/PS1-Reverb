//
//  Reverb.h
//  Ps1Reverb
//
//  Created by Zachary Burkett on 12/12/25.
//

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cstdint>

class Reverb
{
public:
    Reverb();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    void processStereo(float& leftIn, float& rightIn, float& leftOut, float& rightOut);

    void loadPreset(const std::string& presetName);

private:
    // True SPU-style reverb register model (interpreted as sample offsets in this implementation).
    struct ReverbRegs
    {
        // Q15 volumes (signed)
        int16_t vLOUT = 0;
        int16_t vROUT = 0;
        int16_t vLIN = 0;
        int16_t vRIN = 0;

        int16_t vWALL = 0;
        int16_t vIIR = 0;

        int16_t vCOMB1 = 0;
        int16_t vCOMB2 = 0;
        int16_t vCOMB3 = 0;
        int16_t vCOMB4 = 0;

        int16_t vAPF1 = 0;
        int16_t vAPF2 = 0;

        // Offsets (interpreted as sample offsets into ring buffers)
        int mLSAME1 = 0;
        int mRSAME1 = 0;
        int mLSAME2 = 0;
        int mRSAME2 = 0;

        int mLDIFF1 = 0;
        int mRDIFF1 = 0;
        int mLDIFF2 = 0;
        int mRDIFF2 = 0;

        int mLCOMB1 = 0;
        int mRCOMB1 = 0;
        int mLCOMB2 = 0;
        int mRCOMB2 = 0;
        int mLCOMB3 = 0;
        int mRCOMB3 = 0;
        int mLCOMB4 = 0;
        int mRCOMB4 = 0;

        int mLAPF1 = 0;
        int mRAPF1 = 0;
        int mLAPF2 = 0;
        int mRAPF2 = 0;

        int dAPF1 = 0;
        int dAPF2 = 0;

        int bufferSize = 0;
    };

    // Optional float mirrors (nice later when you add UI parameters)
    struct ParamsFloat
    {
        float vLOUT = 0.5f;
        float vROUT = 0.5f;
        float vLIN = 0.0f;
        float vRIN = 0.0f;

        float vWALL = 0.0f;
        float vIIR = 0.0f;

        float vCOMB1 = 0.0f;
        float vCOMB2 = 0.0f;
        float vCOMB3 = 0.0f;
        float vCOMB4 = 0.0f;

        float vAPF1 = 0.0f;
        float vAPF2 = 0.0f;
    };

    ReverbRegs regs {};
    ParamsFloat paramsFloat {};

    // delay buffers (16-bit)
    std::vector<int16_t> leftBuffer;
    std::vector<int16_t> rightBuffer;
    int bufferPos = 0;

    // Processing mode
    double sampleRate = 44100.0;
    bool halfRatePhase = false;   // run reverb core every other sample
    float lastWetL = 0.0f;
    float lastWetR = 0.0f;

    // Preset loader (maps consoledev tables into SPU regs)
    void loaderSpuRegs(const uint16_t preset[32]);
    void updateBufferSize();

    // Q15 / int16 helpers
    static float ps1VolumeToFloat(int16_t ps1Vol);

    static int32_t clamp16(int32_t x) noexcept;
    static int16_t floatToS16(float x) noexcept;
    static float s16ToFloat(int16_t x) noexcept;
    static int16_t mulQ15(int16_t a, int16_t b) noexcept;
    static int16_t iirQ15(int16_t input, int16_t feedback, int16_t vIIR) noexcept;

    int16_t readBufferS16(const std::vector<int16_t>& buffer, int offset) const noexcept;
    void writeBufferS16(std::vector<int16_t>& buffer, int offset, int16_t value) noexcept;
};
