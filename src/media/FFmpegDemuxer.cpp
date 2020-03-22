#include "FFmpegDemuxer.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif
#include "FFmpegUtils.h"

static AVRational gContextBaseTime = { 1, AV_TIME_BASE };

FFmpegDemuxer::FFmpegDemuxer()
{

}

bool FFmpegDemuxer::open(const char* pFile)
{
    close();

    if (avformat_open_input(&m_pFormatContext, pFile, NULL, NULL) != 0)
    {
        avformat_close_input(&m_pFormatContext);
        m_pFormatContext = NULL;
        return false;
    }
    if (avformat_find_stream_info(m_pFormatContext, NULL) < 0)
    {
    }

    for (int i = 0; i < m_pFormatContext->nb_streams; i++)
    {
        if (m_pVideoStream && m_pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            openVideoStream(i);
        }
        else if (m_pAudioStream && m_pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            openAudioStream(i);
        }
    }

    if (m_pFormatContext->duration != AV_NOPTS_VALUE)
    {
		m_mediaInfo.iFileTotalTime = QmBaseTimeToMSTime(m_pFormatContext->duration, gContextBaseTime);
    }
}

void FFmpegDemuxer::close()
{
    if (m_pFormatContext)
    {
        avformat_close_input(&m_pFormatContext);
        m_pFormatContext = NULL;
    }
}

int FFmpegDemuxer::readPacket(AVPacketPtr& ptr)
{
	int ret = av_read_frame(m_pFormatContext, ptr.get());
	return ret;
}

int FFmpegDemuxer::seek(double fPos)
{
	int64_t seek_target = QmMSTimeToBaseTime(m_mediaInfo.iFileTotalTime * fPos, gContextBaseTime);
	int iRet = av_seek_frame(m_pFormatContext, -1, seek_target, AVSEEK_FLAG_BACKWARD);
	if (iRet < 0)
	{
		iRet = av_seek_frame(m_pFormatContext, -1, seek_target, AVSEEK_FLAG_FRAME);
	}
	return iRet;
}

void FFmpegDemuxer::openVideoStream(int i)
{
    m_pVideoStream = m_pFormatContext->streams[i];
    AVCodecParameters* p_codec_par = m_pFormatContext->streams[i]->codecpar;

    if (AV_NOPTS_VALUE != m_pVideoStream->duration)
        m_mediaInfo.videoTotalTime = QmBaseTimeToMSTime(m_pVideoStream->duration, m_pVideoStream->time_base);

    m_mediaInfo.frameRate = av_q2d(av_stream_get_r_frame_rate(m_pVideoStream));
    m_mediaInfo.videoWidth = p_codec_par->width;
    m_mediaInfo.videoHeight = p_codec_par->height;
    m_mediaInfo.videoFormat = p_codec_par->format;
}

void FFmpegDemuxer::openAudioStream(int i)
{
    m_pAudioStream = m_pFormatContext->streams[i];
    AVCodecParameters* p_codec_par = m_pFormatContext->streams[i]->codecpar;
    if (AV_NOPTS_VALUE != m_pAudioStream->duration)
        m_mediaInfo.audioTotalTime = QmBaseTimeToMSTime(m_pAudioStream->duration, m_pAudioStream->time_base);

    m_mediaInfo.sampleRate = p_codec_par->sample_rate;
    m_mediaInfo.nChannels = p_codec_par->channels;
    m_mediaInfo.audioFormat = p_codec_par->format;
}
