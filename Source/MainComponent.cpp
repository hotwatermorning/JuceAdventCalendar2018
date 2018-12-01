/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (400, 300);

    // specify the number of input and output channels that we want to open
    setAudioChannels (2, 2);
    
    // AudioPluginFormatManagerに、ホストとして対応したいPluginFormatを設定する。
    mgr_.addFormat(new VST3PluginFormat);
    mgr_.addFormat(new AudioUnitPluginFormat);
    
    // ↑ただし、既定で有効にするPluginFormatはProjucer側で事前に設定してあるので、
    // 個別に`addFormat()`を呼び出さなくても、`mgr_.addDefaultFormats()`を呼び出すだけで十分。
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    sample_pos_ = 0;
    last_pitch_ = -1;
    mb_.ensureSize(10);
    
    if(proc_) {
        proc_->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    //bufferToFill.clearActiveBufferRegion();
    
    auto lock = make_lock();
    
    if(proc_) {
        auto sample_pos_to_note = [](auto sample_rate, auto sample_pos) {
            static std::vector<int> const pitches = { 60, 62, 64, 65, 67, 69, 71, 72 };
            return pitches[((int)((sample_pos / sample_rate) * 2)) % pitches.size()];
        };
        
        auto const pitch = sample_pos_to_note(deviceManager.getAudioDeviceSetup().sampleRate,
                                              sample_pos_);

        if(pitch != last_pitch_) {
            if(last_pitch_ != -1) { mb_.addEvent(MidiMessage::noteOff(1, last_pitch_, 0.5f), 0); }
            mb_.addEvent(MidiMessage::noteOn(1, pitch, 0.5f), 0);
            last_pitch_ = pitch;
        }
    
        proc_->processBlock(*bufferToFill.buffer, mb_);
        
        mb_.clear();
    } else {
        bufferToFill.clearActiveBufferRegion();
    }
    
    sample_pos_ += bufferToFill.numSamples;
}

void MainComponent::releaseResources()
{
    if(proc_) {
        proc_->releaseResources();
    }
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.drawText("Drag'n'Drop A Plugin Here.", getLocalBounds(), Justification::centred);
}

void MainComponent::resized()
{
    if(!editor_) { return; }
    
    setBounds(editor_->getLocalBounds());
}

bool MainComponent::isInterestedInFileDrag (const StringArray &files)
{
    std::vector<String> exts = { ".vst3", ".component" };
    for(auto name: files) {
        if(std::any_of(exts.begin(), exts.end(),
                       [name](auto ext) { return name.endsWith(ext); }
                       ))
        {
            return true;
        }
    }
    
    return false;
}

void MainComponent::filesDropped (const StringArray &files, int x, int y)
{
    OwnedArray<PluginDescription> found;
    KnownPluginList().scanAndAddDragAndDroppedFiles(mgr_, files[0], found);
    
    if(found.isEmpty()) { return; }
    
    LoadPlugin(*found[0]);
}

void MainComponent::LoadPlugin(PluginDescription const &desc)
{
    UnloadPlugin();
    
    auto const sample_rate = deviceManager.getAudioDeviceSetup().sampleRate;
    auto const buffer_size = deviceManager.getAudioDeviceSetup().bufferSize;
    
    String error;
    
    AudioProcessor* p = mgr_.createPluginInstance(desc, sample_rate, buffer_size, error);

    if(error.isEmpty() == false) {
        std::cout << "Cannot load a plugin: " << error << std::endl;
        return;
    }
    
    assert(p != nullptr);
    
    p->prepareToPlay(sample_rate, buffer_size);
    
    {
        auto lock = make_lock();
        proc_.reset(p);
    }
    
    if(proc_->hasEditor()) {
        editor_.reset(proc_->createEditor());
        addAndMakeVisible(*editor_);
        resized();
    }
}

void MainComponent::UnloadPlugin()
{
    if(editor_) {
        removeChildComponent(editor_.get());
        editor_.reset();
    }
    
    auto lock = make_lock();
    auto old_proc = std::move(proc_);
    lock.unlock();
    
    if(old_proc) {
        old_proc->releaseResources();
        old_proc.reset();
    }
}
