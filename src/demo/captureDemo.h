#pragma once

#include <memory>

class QcAudioTransformat;
class WASAPICapture;
class MsgWnd;
class CoreRunloop;
class CaptureDemo
{
public:
    CaptureDemo();

    void run();
protected:
    std::unique_ptr<CoreRunloop> m_runloop;
    std::unique_ptr<WASAPICapture> m_audioCapture;
    std::unique_ptr<QcAudioTransformat> m_audioTransForPlayer;
};