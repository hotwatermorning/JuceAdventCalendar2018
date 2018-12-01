/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include <mutex>

#include "../JuceLibraryCode/JuceHeader.h"

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
    
    //! proc_変数の排他制御用
    std::mutex mutable mtx_;
    std::unique_lock<std::mutex> make_lock() const {
        return std::unique_lock<std::mutex>(mtx_);
    }

    //! ロードしたプラグイン
    std::unique_ptr<AudioProcessor> proc_;
    
    //! プラグインから取得したエディターGUI
    std::unique_ptr<AudioProcessorEditor> editor_;
    
    AudioPluginFormatManager mgr_;
    
    //! @name 再生処理に使用する変数
    //@{
    int sample_pos_; //!< 現在の再生サンプル位置
    int last_pitch_; //!< 最後にプラグインに送信したMIDIデータ
    MidiBuffer mb_;  //!< プラグインに送信するMIDIデータ
    //@}
    
    //! プラグインをロードして、proc_変数にセットする。
    void LoadPlugin(PluginDescription const &desc);
    
    //! プラグインをアンロードする。 (ロードされていないときは何もしない)
    void UnloadPlugin();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
