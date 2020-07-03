#include "VideoRenderWindow.h"
#include "libgraphics/D3D11Device.h"
#include "libgraphics/D3D11Texture.h"
#include "QmMacro.h"
#include "win/MsgWnd.h"
#include "libmedia/FFmpegVideoTransformat.h"
#include "libmedia/FFmpegUtils.h"
#include "utils/LogTimeElapsed.h"
#include <d3d11.h>


#define DxRender 1


IMPLEMENT_DYNAMIC(VideoRenderWindow, CWnd)
BEGIN_MESSAGE_MAP(VideoRenderWindow, CWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

VideoRenderWindow::VideoRenderWindow()
{
#if DxRender
	m_d3d11Device = std::make_shared<D3D11Device>();
	m_d3d11Device->create(0);
	m_videoTex = std::make_shared<D3D11Texture>(m_d3d11Device->device());
#else
	m_memorySurface = std::make_shared<QcDIBSection>();
	m_transFormat = std::make_unique<FFmpegVideoTransformat>();
#endif
}

VideoRenderWindow::~VideoRenderWindow()
{

}

void VideoRenderWindow::init(HWND hWnd)
{
	SubclassWindow(hWnd);

	RECT rc;
	::GetClientRect(hWnd, &rc);
	if (m_d3d11Device)
		m_d3d11Device->createSwapChain(hWnd, rc.right - rc.left, rc.bottom - rc.top);
}


ID3D11Device* VideoRenderWindow::device() const
{
	return m_d3d11Device->device();
}

void VideoRenderWindow::OnPaint()
{
	CWnd::OnPaint();

	onRender();
}
void VideoRenderWindow::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	if (m_memorySurface)
		m_memorySurface->resize(cx, cy);
	if (m_d3d11Device)
		m_d3d11Device->resize(cx, cy);
}

void VideoRenderWindow::onRender()
{
	RECT rc;
	::GetClientRect(m_hWnd, &rc);
	int w = rc.right - rc.left;
	int h = rc.bottom - rc.top;

	if (m_memorySurface)
	{
		m_memorySurface->resize(w, h);
		AVFrameRef frame = AVFrameRef::fromHWFrame(frame);
		{
			QmStdMutexLocker(m_lastFrameMutex);
			frame = m_lastFrame;
		}
		m_transFormat->transformat(m_lastFrame.width(), frame.height(), frame.format(), frame.data(), frame.linesize()
			, w, h, FFmpegUtils::fourccToFFmpegFormat(m_memorySurface->format()), m_memorySurface->datas(), m_memorySurface->lineSizes());

		HDC hDC = ::GetDC(m_hWnd);
		::BitBlt(hDC, 0, 0, w, h, m_memorySurface->dcHandle(), 0, 0, SRCCOPY);
		::ReleaseDC(m_hWnd, hDC);
	}
	if (m_d3d11Device)
	{
		AVFrameRef frame;
		{
			QmStdMutexLocker(m_lastFrameMutex);
			frame = m_lastFrame;
		}
		m_d3d11Device->begin();

		//LogTimeElapsed logRender(L"renderFrame");
		do 
		{
			if (frame.isHWFormat())
			{
				ID3D11Texture2D* tex2D = (ID3D11Texture2D*)frame.data(0);
				int subResouce = (int)frame.data(1);

				//TODO do not need copy to m_videoTex
				if (m_videoTex->updateFromTexArray(tex2D, subResouce))
				{
					m_d3d11Device->drawTexture(m_videoTex.get(), { 0, 0, w, h });
				}
				else
				{
					frame = AVFrameRef::fromHWFrame(frame);
				}
			}

			//update load to gpu
			int iVideoFormat = FFmpegUtils::ffmpegFormatToFourcc(frame.format());
			switch (iVideoFormat)
			{
			case FOURCC_I420:
			{
				m_videoTex->updateYUV(frame.data(), frame.linesize(), frame.width(), frame.height());
				break;
			}
			case FOURCC_NV12:
			{
				m_videoTex->updateNV12(frame.data(), frame.linesize(), frame.width(), frame.height());
				break;
			}
			default:
			{
				//TODO
				break;
			}
			}
			m_d3d11Device->drawTexture(m_videoTex.get(), { 0, 0, w, h });
		} while (0);

		m_d3d11Device->present();
	}
}

bool VideoRenderWindow::OnVideoFrame(const AVFrameRef& frame)
{
	{
		QmStdMutexLocker(m_lastFrameMutex);
		m_lastFrame = frame;
	}
	if (!m_bUpdating)
	{
		m_bUpdating = true;
		std::weak_ptr<VideoRenderWindow> weakThis = shared_from_this();
#if 0
		auto pThis = weakThis.lock();
			if (pThis)
			{
				pThis->m_bUpdating = false;
				pThis->onRender();
			}
#else
		MsgWnd::mainMsgWnd()->post([weakThis]() {
			auto pThis = weakThis.lock();
			if (pThis)
			{
				pThis->m_bUpdating = false;
				pThis->onRender();
			}
		});
#endif
	}
	return true;
}