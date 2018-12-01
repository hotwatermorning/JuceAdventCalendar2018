/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include <mutex>

#include "../JuceLibraryCode/JuceHeader.h"

class ScanDialog
:   public ThreadWithProgressWindow
{
    KnownPluginList known_plugin_list_;
    AudioPluginFormatManager mgr_;
    
    std::unique_ptr<juce::PluginDirectoryScanner> scanner_;
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent
:   public AudioAppComponent
,   public FileDragAndDropTarget
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent();

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...

    bool isInterestedInFileDrag (const StringArray &files) override;
    void filesDropped (const StringArray &files, int x, int y) override;
    
    std::mutex mutable mtx_;
    std::unique_lock<std::mutex> make_lock() const {
        return std::unique_lock<std::mutex>(mtx_);
    }
    
    std::unique_ptr<AudioProcessor> proc_;
    std::unique_ptr<AudioProcessorEditor> editor_;
    AudioPluginFormatManager mgr_;
    int sample_pos_;
    int last_pitch_;
    MidiBuffer mb_;
    
    void LoadPlugin(PluginDescription const &desc);
    void UnloadPlugin();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
