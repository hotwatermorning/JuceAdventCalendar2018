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
    // 実際は個別に`addFormat()`を呼び出さなくても、`mgr_.addDefaultFormats()`を呼び出すだけで十分。
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // 再生処理用の変数の初期化
    sample_pos_ = 0;
    last_pitch_ = -1;
    mb_.ensureSize(10);
    
    if(proc_) {
        proc_->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
    // フレーム処理中は、プラグインがアンロードされないようにする。
    auto lock = make_lock();
    
    if(proc_) {
        // シンセの動作検証用に、仮のMIDIデータを作ってプラグインに渡す
        {
            // 指定したサンプル位置でのノートを返す。
            auto get_note_at = [](auto sample_rate, auto sample_pos) {
                static std::vector<int> const pitches = { 60, 62, 64, 65, 67, 69, 71, 72 };
                return pitches[((int)((sample_pos / sample_rate) * 2)) % pitches.size()];
            };
            
            auto const pitch = get_note_at(deviceManager.getAudioDeviceSetup().sampleRate,
                                           sample_pos_);

            if(pitch != last_pitch_) {
                if(last_pitch_ != -1) { mb_.addEvent(MidiMessage::noteOff(1, last_pitch_, 0.5f), 0); }
                mb_.addEvent(MidiMessage::noteOn(1, pitch, 0.5f), 0);
                last_pitch_ = pitch;
            }
        }
    
        // プラグインのフレーム処理を実行
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
    
    // エディターGUIは、自身が表示するのに好ましいサイズの情報を持っているので、
    // MainComponentの方をそのサイズに合わせる。
    setBounds(editor_->getLocalBounds());
}

bool MainComponent::isInterestedInFileDrag (const StringArray &files)
{
    // 拡張子がこれらのファイルのみドロップ対象にする。
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
    // PluginDescriptionは、JUCEがスキャンしたプラグインの情報を保持する構造体
    OwnedArray<PluginDescription> found;
    
    // ドロップしたファイルをスキャンして、PluginDescriptionを生成。
    // プラグインフォーマットによっては、一つのファイルに複数のプラグインが含まれていることがあるので、
    // PluginDescriptionの配列を参照として渡しておいて、その配列に見つかったプラグインの情報を追加していく仕組みになっている。
    KnownPluginList().scanAndAddDragAndDroppedFiles(mgr_, files[0], found);
    
    if(found.isEmpty()) { return; }
    
    LoadPlugin(*found[0]);
}

void MainComponent::LoadPlugin(PluginDescription const &desc)
{
    // 事前にロードされているプラグインがあれば、アンロードしておく。
    UnloadPlugin();
    
    auto const sample_rate = deviceManager.getAudioDeviceSetup().sampleRate;
    auto const buffer_size = deviceManager.getAudioDeviceSetup().bufferSize;
    
    String error;
    
    // AudioPluginFormatManagerを利用して、プラグインを構築する
    // プラグインフォーマットによっては、構築のタイミングでサンプリングレートとバッファサイズを必要とするものがあるので、
    // 初期値として、ここでそれらを渡しておく。
    AudioProcessor* p = mgr_.createPluginInstance(desc, sample_rate, buffer_size, error);

    if(error.isEmpty() == false) {
        std::cout << "Cannot load a plugin: " << error << std::endl;
        return;
    }
    
    assert(p != nullptr);
    
    // 初期値としてサンプリングレートとバッファサイズを渡していても、再生可能にはなっていないので、
    // 改めてサンプリングレートとバッファサイズを渡して再生可能な状態にする。
    p->prepareToPlay(sample_rate, buffer_size);
    
    {
        // デバイスのフレーム処理と競合しないように、proc_変数に新しいAudioProcessorを設定
        auto lock = make_lock();
        proc_.reset(p);
    }
    
    // プラグインがエディターGUIを持っている場合は、それも構築して、子ウィンドウに追加して表示できるようにしておく。
    if(proc_->hasEditor()) {
        editor_.reset(proc_->createEditor());
        addAndMakeVisible(*editor_);
        resized();
    }
}

void MainComponent::UnloadPlugin()
{
    // エディターGUIがプラグインから取得されてる場合は、先にそれを削除する。
    // エディターの仕組みは、プラグインがフレーム処理を行う仕組みとは分離されているので、
    // この部分は排他制御を行う必要はない。
    if(editor_) {
        removeChildComponent(editor_.get());
        editor_.reset();
    }
    
    // フレーム処理と競合しないように、proc_変数の中身を移し替える。
    auto lock = make_lock();
    auto old_proc = std::move(proc_);
    lock.unlock();
    
    if(old_proc) {
        old_proc->releaseResources();
        old_proc.reset();
    }
}
