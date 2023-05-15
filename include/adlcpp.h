#pragma once

#ifndef ADLCPP_H
#define ADLCPP_H

#include <string>

#ifdef ADLMIDI_BUILD
#   ifndef ADLMIDI_DECLSPEC
#       if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#           define ADLMIDI_DECLSPEC __declspec(dllexport)
#       else
#           define ADLMIDI_DECLSPEC
#       endif
#   endif
#else
#   define ADLMIDI_DECLSPEC
#endif

#ifndef __DJGPP__

struct AdlSimpleMidiPlay_private;

/**
 * @brief The simple MIDI Playing class
 */
class ADLMIDI_DECLSPEC AdlSimpleMidiPlay
{
    AdlSimpleMidiPlay_private *p;

public:
    AdlSimpleMidiPlay();
    ~AdlSimpleMidiPlay();
    AdlSimpleMidiPlay(const AdlSimpleMidiPlay &) = delete;

    /**
     * @brief Set the output sample rate. CAN BE USED BEFORE OPENING OF THE MIDI FILE.
     * @param rate Sample rate in Hz
     */
    void SetSampleRate(int rate);

    /**
     * @brief Change bank number. CAN BE USED BEFORE OPENING OF THE MIDI FILE.
     * @param bankNo Number of bank
     */
    void SetBankNo(int bankNo);

    /**
     * @brief Retreive the current bank number
     * @return Bank number value
     */
    int GetBankNo() const;

    /**
     * @brief Get the total counf of embedded banks
     * @return Total count of banks
     */
    static int MaxBankNo();

    /**
     * @brief Get a full list of banks.
     * @return Array of strings
     */
    static const char* const* GetBankNames();

    /**
     * @brief Change the number of emulated cards. CAN BE USED BEFORE OPENING OF THE MIDI FILE.
     * @param Number of OPL3 cards
     */
    void SetCardsNum(int cardsNum);

    /**
     * @brief Get the current number of emulated cards.
     * @return
     */
    int GetCardsNum() const;

    /**
     * @brief Change the number of 4-operator voices allocation. CAN BE USED BEFORE OPENING OF THE MIDI FILE.
     * @param Number of 4-op voices
     *
     * You can change the amout of 4-operator voices between 0 and 6 multipled by current number of cards.
     */
    void Set4OpNum(int fourOpNum);

    /**
     * @brief Get the current count of available 4-operator voices
     * @return Number of available 4-operator voices.
     */
    int Get4OpNum() const;

    /**
     * @brief Load MIDI file.
     * @param path Full path to MIDI file.
     * @return true if MIDI file was loaded successfully
     */
    bool LoadMidi(const std::string &path);

    /**
     * @brief Enable the song looping
     * @param en Enable loop
     */
    void SetLoopEnabled(bool en);

    /**
     * @brief Play the music and fill the output buffer of specified size
     * @param output Output buffer
     * @param samples Count of stereo frames (one frame contains two samples)
     * @return A count of actually written frames. Returns 0 at end of song with disabled loop.
     */
    long Play(short *output, long frames);
};

#endif


#endif // ADLCPP_H
