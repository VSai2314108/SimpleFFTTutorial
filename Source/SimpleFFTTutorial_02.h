/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   this permission notice appear in all copies.
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   //r
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             SimpleFFTTutorial
 version:          2.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Displays an FFT spectrogram.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_dsp, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        SpectrogramComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once
#include <vector>
#include <string>

//==============================================================================
class SpectrogramComponent : public juce::AudioAppComponent,
    private juce::Timer
{
public:
    SpectrogramComponent()
        : forwardFFT(fftOrder),
        spectrogramImage(juce::Image::RGB, 512, 512, true)
    {
        setOpaque(true);
        setAudioChannels(2, 0);  // we want a couple of input channels but no outputs
        startTimerHz(60);
        setSize(700, 500);

        //Label for Alert
        addAndMakeVisible(titleLabel);
        titleLabel.setFont(juce::Font(32.0f, juce::Font::bold));
        titleLabel.setText("Determining BaseLine", juce::dontSendNotification);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);

        //Label for Button
        addAndMakeVisible(setBaseButton);
        setBaseButton.setButtonText("Set Base");
        setBaseButton.onClick = [this] { setBase(); };   // [8]
        setBaseButton.setBounds(getWidth() - 160, getHeight() - 60, 150, 50);

        addAndMakeVisible(ignoreAlarmButton);
        ignoreAlarmButton.setButtonText("Ignore Alarm");
        ignoreAlarmButton.onClick = [this] { ignoreAlarm(); };
        ignoreAlarmButton.setBounds(10, getHeight() - 60, 150, 50);

        //Adding testing Feature
        addAndMakeVisible(inputText);
        inputText.setEditable(true);
        inputText.setColour(juce::Label::backgroundColourId, juce::Colours::darkblue);
        inputText.setBounds(10, 10, 50, 20);
        inputText.onTextChange = [this] { baseindex = 0; alerted = 0; base.clear(); basef = 0; };
    }

    ~SpectrogramComponent() override
    {
        shutdownAudio();
    }

    //==============================================================================
    void prepareToPlay(int, double) override {}
    void releaseResources() override {}

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (bufferToFill.buffer->getNumChannels() > 0)
        {
            auto* channelData = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);

            for (auto i = 0; i < bufferToFill.numSamples; ++i)
                pushNextSampleIntoFifo(channelData[i]);
        }
    }

    //==============================================================================
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        g.setOpacity(1.0f);
        g.drawImage(spectrogramImage, getLocalBounds().toFloat());
    }

    void timerCallback() override
    {
        if (nextFFTBlockReady)
        {
            drawNextLineOfSpectrogram();
            nextFFTBlockReady = false;
            repaint();
        }
    }

    void pushNextSampleIntoFifo(float sample) noexcept
    {
        // if the fifo contains enough data, set a flag to say
        // that the next line should now be rendered..
        if (fifoIndex == fftSize)       // [8]
        {
            if (!nextFFTBlockReady)    // [9]
            {
                std::fill(fftData.begin(), fftData.end(), 0.0f);
                std::copy(fifo.begin(), fifo.end(), fftData.begin());
                nextFFTBlockReady = true;
            }

            fifoIndex = 0;
        }

        fifo[(size_t)fifoIndex++] = sample; // [9]
    }

    void drawNextLineOfSpectrogram()
    {
        auto rightHandEdge = spectrogramImage.getWidth() - 1;
        auto imageHeight = spectrogramImage.getHeight();

        // first, shuffle our image leftwards by 1 pixel..
        spectrogramImage.moveImageSection(0, 0, 1, 0, rightHandEdge, imageHeight);         // [1]

        // then render our FFT data..
        forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());                   // [2]

        // find the range of values produced, so we can scale our rendering to
        // show up the detail clearly
        auto maxLevel = juce::FloatVectorOperations::findMinAndMax(fftData.data(), fftSize / 2); // [3]

        //method variables
        //1
        int alert = 0;
        //2
        std::vector<float> uses;
        //3
        float use = 0;

        //initialize the comparison object at one hundred

        if (baseindex >= 100 && inputText.getText() == "2")
        {
            for (int i = 0; i < base.size(); i++)
            {
                uses.push_back(0.0);
            }
        }


        //FFT level creation
        for (auto y = 1; y < imageHeight; ++y)                                              // [4]
        {
            auto
                skewedProportionY = 1.0f - std::exp(std::log((float)y / (float)imageHeight) * 0.2f);
            //recreate a seperate loop for data calculations using all of the data between 0 and fftSize/2 in order to properely represent data
            //remove line 188 ie. line below creating the fftdata index
            auto fftDataIndex = (size_t)juce::jlimit(0, fftSize / 2, (int)(skewedProportionY * fftSize / 2));
            auto level = juce::jmap(fftData[fftDataIndex], 0.0f, juce::jmax(maxLevel.getEnd(), 1e-5f), 0.0f, 1.0f);
            /*
            if (inputText.getText() == "1")
            {
                if (baseindex > 99)
                {
                    if ((level > 0 && level > 2 * base[y - 1]) || (level < 0 && level < 2 * base[y - 1]))
                    {
                        alert++;
                    }
                }
                else if (baseindex == 99)
                {
                    base[y - 1] = base[y - 1] / 100.0f;
                    std::cout << base[y - 1] << std::endl;
                }
                else if (baseindex == 0)
                {
                    base.push_back(level);
                }
                else
                {
                    base[y - 1] = base[y - 1] + level;
                }
            }
            else if (inputText.getText() == "2")
            {
                float val = imageHeight / 25;
                if (baseindex > 99)
                {
                    uses[(y - 1) / 25] += level;
                }
                else if (baseindex == 99)
                {
                    if (y>= 25 * (imageHeight / 25) && y+1 == imageHeight)
                    {
                        base[(y - 1) / 25] = base[(y - 1) / 25] / ((float)(imageHeight - 25 * (imageHeight / 25)));
                    }
                    else if ((y - 1) % 25 == 0)
                    {
                        base[(y - 1)/25] = base[(y - 1)/25] / val;
                    }
                }
                else if (baseindex == 0)
                {
                    if ((y - 1) % 25 == 0)
                    {
                        base.push_back(0);
                    }
                    base[(y - 1) / 25] = base[(y - 1) / 25] + level;
                }
                else
                {
                    base[(y - 1) / 25] = base[(y-1)/25] + level;
                }
            }
    */
            spectrogramImage.setPixelAt(rightHandEdge, y, juce::Colour::fromHSV(level, 1.0f, level, 1.0f)); // [5]
        }

        //data set collection
        for (auto i = 0; i <= fftSize / 2; i++)
        {
            auto level = juce::jmap(fftData[i], 0.0f, juce::jmax(maxLevel.getEnd(), 1e-5f), 0.0f, 1.0f);
            //looks for x amounts of frequency to change by y percent from baseline
            if (inputText.getText() == "1")
            {
                if (baseindex > 99)
                {
                    if ((level > 0 && level > 2 * base[i]) || (level < 0 && level < 2 * base[i]))
                    {
                        alert++;
                    }
                }
                else if (baseindex == 99)
                {
                    base[i] = base[i] / 100.0f;
                    std::cout << base[i] << std::endl;
                }
                else if (baseindex == 0)
                {
                    base.push_back(level);
                }
                else
                {
                    base[i] = base[i] + level;
                }

            }
            //looks for the max frequience group of const (25) groups to shift over by x groups from baseline
            else if (inputText.getText() == "2")
            {
                int val = (fftSize / 2) / 25;
                if (baseindex > 99)
                {
                    uses[(i) / val] += level;
                }
                else if (baseindex == 99)
                {
                    if (i >= val * ((fftSize / 2) / val) && i + 1 > (fftSize / 2))
                    {
                        base[(i) / val] = (base[(i) / val] + level) / ((float)((fftSize / 2) - val * ((fftSize / 2) / val)));
                    }
                    else if ((i) % val == 0)
                    {
                        base[(i) / val] = base[(i) / val] / 25;
                    }
                }
                else if (baseindex == 0)
                {
                    if ((i) % val == 0)
                    {
                        base.push_back(0);
                    }
                    base[(i) / val] = base[(i) / val] + level;
                }
                else
                {
                    base[(i) / val] = base[(i) / val] + level;
                }
            }
            //looks for the total amplitude (sum of individual amplitudes) to deviate by x percent 
            else if (inputText.getText() == "3")
            {
                if (baseindex > 99)
                {
                    use += level;
                }
                else if (baseindex == 99)
                {
                    basef += level;
                    if (i == (fftSize / 2))
                    {
                        basef = basef / 100.0f;
                    }
                }
                else
                {
                    basef += level;
                }

            }
        }

        //global alert triggred
        if (baseindex > 99 && inputText.getText() == "1")
        {
            if (alert >= 250)
                alerted = 1;
        }
        else if (inputText.getText() == "2" && baseindex > 99)
        {
            int mbase = 0;
            int muse = 0;
            for (int i = 0; i < uses.size(); i++)
            {
                if (base[i] > base[mbase])
                    mbase = i;
                if (uses[i] > uses[muse])
                    muse = i;
            }
            if (abs(mbase - muse) > 1)
                alerted = 1;
        }
        else if (inputText.getText() == "3" && baseindex > 99)
        {
            //change the value after the > sign ... the value - 1 * 100 is the percent change threshold
            if (use / basef > 2.0f)
                alerted = 1;
        }

        //Draw Label
        if (alert < 250 && baseindex > 99 && alerted == 0)
        {
            titleLabel.setText("Normal Reading", juce::dontSendNotification);
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        else if (baseindex <= 99)
        {
            titleLabel.setText("Determining Baseline", juce::dontSendNotification);
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        else
        {
            titleLabel.setText("Alert Triggered", juce::dontSendNotification);
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        }

        //increment base
        baseindex++;

    }

    void resized() override
    {
        titleLabel.setBounds(10, 10, getWidth() - 20, 50);

        setBaseButton.setBounds(getWidth() - 260, getHeight() - 90, 250, 80);

        ignoreAlarmButton.setBounds(10, getHeight() - 90, 250, 80);
    }

    void setBase()
    {
        baseindex = 0;
        basef = 0;
        for (int i = 0; i < base.size(); i++)
        {
            base[i] = 0;
        }
    }

    void ignoreAlarm()
    {
        alerted = 0;
    }

    //Change this value to reduce bins, default is 10
    static constexpr auto fftOrder = 10;                // [1]
    static constexpr auto fftSize = 1 << fftOrder;     // [2]

private:
    juce::dsp::FFT forwardFFT;                          // [3]
    juce::Image spectrogramImage;

    std::array<float, fftSize> fifo;                    // [4]
    std::array<float, fftSize * 2> fftData;             // [5]
    int fifoIndex = 0;                                  // [6]
    bool nextFFTBlockReady = false;                     // [7]

    std::vector<float> base;
    float basef = 0;
    int baseindex = 0;
    int alerted = 0;

    juce::Label titleLabel;

    juce::TextButton setBaseButton;
    juce::TextButton ignoreAlarmButton;

    juce::Label inputText;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramComponent)
};
