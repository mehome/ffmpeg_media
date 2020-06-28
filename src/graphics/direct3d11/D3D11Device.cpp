#include "D3D11Device.h"
#include <dxgi.h>
#include <d3d11.h>
#include "ShaderResource.h"
#include "VertexBuffers.h"
#include "D3D11Texture.h"


static CComPtr<IDXGIAdapter1> chooseAdapterByLUID(uint64_t luid)
{
	CComPtr<IDXGIFactory1> dxgiFactory;
	CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

	CComPtr<IDXGIAdapter1> adapter;
	for (int i = 0; dxgiFactory->EnumAdapters1(i, &adapter) == S_OK; ++i)
	{
		DXGI_ADAPTER_DESC desc;
		HRESULT hr = adapter->GetDesc(&desc);
		if (memcmp(&luid, &desc.AdapterLuid, sizeof(desc.AdapterLuid)) == 0)
			return adapter;

		adapter = nullptr;
	}
	return nullptr;
}

static HRESULT createDevice(uint64_t adapterLUID, ID3D11Device** ppDevice, ID3D11DeviceContext** ppDeviceContext)
{
	CComPtr<IDXGIAdapter1> adapter = chooseAdapterByLUID(adapterLUID);
	HRESULT hr = S_OK;

	hr = D3D11CreateDevice(adapter,
		adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
		nullptr,    // Module
#ifdef _DEBUG
		D3D11_CREATE_DEVICE_DEBUG |
#endif // _DEBUG
		D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		nullptr, 0, // Highest available feature level
		D3D11_SDK_VERSION,
		ppDevice,
		nullptr,    // Actual feature level
		ppDeviceContext);  // Device context
	return hr;
}

D3D11Device::D3D11Device()
{

}

D3D11Device::~D3D11Device()
{

}

bool D3D11Device::create(uint64_t adapterLUID)
{
	if (m_d3d11Device)
		return true;

	HRESULT hr = createDevice(adapterLUID, &m_d3d11Device, &m_d3d11DeviceContext);
	if (FAILED(hr))
		return false;

	if (!initShaderResource())
		return false;

	if (!initVertexBuffer())
		return false;

	return true;
}

bool D3D11Device::createSwapChain(HWND hWnd, int w, int h)
{
	if (!m_d3d11Device)
		return false;

	m_swapChain = nullptr;

	DXGI_SWAP_CHAIN_DESC sc_desc;
	ZeroMemory(&sc_desc, sizeof(sc_desc));
	sc_desc.BufferCount = 2;
	sc_desc.BufferDesc.Width = w;
	sc_desc.BufferDesc.Height = h;
	sc_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sc_desc.SampleDesc.Count = 1;
	sc_desc.OutputWindow = hWnd;
	sc_desc.Windowed = TRUE;

	CComPtr<IDXGIFactory1> dxgiFactory;
	CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	HRESULT hr = dxgiFactory->CreateSwapChain(m_d3d11Device, &sc_desc, &m_swapChain);
	if (FAILED(hr))
	{
		return false;
	}

	{
		m_samplerLinear = nullptr;
		D3D11_SAMPLER_DESC desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
		hr = m_d3d11Device->CreateSamplerState(
			&desc,
			&m_samplerLinear
		);
		if (FAILED(hr))
		{
			return false;
		}
		ID3D11SamplerState* pSampler = m_samplerLinear;
		m_d3d11DeviceContext->PSSetSamplers(0, 1, &pSampler);
	}

	m_width = w;
	m_height = h;
	makeRenderTargetView();
	setViewPort(m_width, m_height);
	return hr == S_OK;
}

void D3D11Device::resize(int w, int h)
{
	_resize(w, h);
}

bool D3D11Device::initShaderResource()
{
	if (m_shaderResource)
		return true;

	m_shaderResource = std::make_shared<ShaderResource>();
	if (!m_shaderResource->init(m_d3d11Device))
	{
		m_shaderResource = nullptr;
		return false;
	}

	m_d3d11DeviceContext->IASetInputLayout(m_shaderResource->inputLayout());
	return true;
}

bool D3D11Device::initVertexBuffer()
{
	if (m_vertexBuffers)
		return true;

	m_vertexBuffers = std::make_shared<VertexBuffers>(2);
	return true;
}

void D3D11Device::begin()
{
	FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	m_d3d11DeviceContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

	ID3D11RenderTargetView* targetView = m_renderTargetView;
	m_d3d11DeviceContext->OMSetRenderTargets(1, &targetView, nullptr);
}

void D3D11Device::drawTexture(D3D11Texture* pTexture, const RECT& dstRect)
{
	switch (pTexture->format())
	{
	case DXGI_FORMAT_420_OPAQUE:
	{
		ID3D11ShaderResourceView* resView[] = {
			pTexture->resourceView(0),
			pTexture->resourceView(1),
			pTexture->resourceView(2),
		};
		CComPtr<ID3D11PixelShader> yuvPS = m_shaderResource->pixelShader(ShaderResource::kPS_YUV);
		_draw(yuvPS, resView, 3, dstRect);
		break;
	}
	case DXGI_FORMAT_NV12:
	{
		ID3D11ShaderResourceView* resView[] = {
		pTexture->resourceView(0),
		pTexture->resourceView(1)
		};
		CComPtr<ID3D11PixelShader> nvPS = m_shaderResource->pixelShader(ShaderResource::kPS_NV12);
		_draw(nvPS, resView, 2, dstRect);
		break;
	}
	default:
		break;
	}
}

void D3D11Device::present()
{
	m_swapChain->Present(0, 0);
}

ID3D11Device* D3D11Device::device() const
{
	return m_d3d11Device;
}

CComPtr<ID3D11DeviceContext> D3D11Device::deviceContext() const
{
	return m_d3d11DeviceContext;
}

void D3D11Device::_resize(int Width, int Height)
{
	if (!m_swapChain || (m_width == Width && m_height == Height) )
		return;

	m_d3d11DeviceContext->OMSetRenderTargets(0,0,0);
	m_renderTargetView = nullptr;

	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	m_swapChain->GetDesc(&SwapChainDesc);
	HRESULT hr = m_swapChain->ResizeBuffers(SwapChainDesc.BufferCount, Width, Height, SwapChainDesc.BufferDesc.Format, SwapChainDesc.Flags);
	if (FAILED(hr))
	{
		return;
	}

	makeRenderTargetView();
	setViewPort(Width, Height);
	m_width = Width;
	m_height = Height;
}

//
// Reset render target view
//
void D3D11Device::makeRenderTargetView()
{
	// Get backbuffer
	ID3D11Texture2D* BackBuffer = nullptr;
	HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&BackBuffer));
	if (FAILED(hr))
	{
		return;
	}

	// Create a render target view
	m_renderTargetView = nullptr;
	hr = m_d3d11Device->CreateRenderTargetView(BackBuffer, nullptr, &m_renderTargetView);
	BackBuffer->Release();
	if (FAILED(hr))
	{
		return;
	}

	// Set new render target
	ID3D11RenderTargetView* pRenderTarget = m_renderTargetView;
	m_d3d11DeviceContext->OMSetRenderTargets(1, &pRenderTarget, nullptr);

	return;
}

void D3D11Device::setViewPort(UINT Width, UINT Height)
{
	D3D11_VIEWPORT VP;
	VP.Width = static_cast<FLOAT>(Width);
	VP.Height = static_cast<FLOAT>(Height);
	VP.MinDepth = 0.0f;
	VP.MaxDepth = 1.0f;
	VP.TopLeftX = 0;
	VP.TopLeftY = 0;
	m_d3d11DeviceContext->RSSetViewports(1, &VP);
}

void D3D11Device::_draw(ID3D11PixelShader* psShader, ID3D11ShaderResourceView** textureViews, int nViewCount, const RECT& dstRect)
{
	m_d3d11DeviceContext->PSSetShaderResources(0, nViewCount, textureViews);
	m_d3d11DeviceContext->VSSetShader(m_shaderResource->vertexShader(), nullptr, 0);
	m_d3d11DeviceContext->PSSetShader(psShader, nullptr, 0);


	auto PointToNdc = [](int x, int y, float z, int targetWidth, int targetHeight) -> XMFLOAT3
	{
		float X = 2.0f * (float)x / targetWidth - 1.0f;
		float Y = 1.0f - 2.0f * (float)y / targetHeight;
		float Z = z;
		return XMFLOAT3(X, Y, Z);
	};
	VertexIn vertrices[] =
	{
		{ PointToNdc(dstRect.left, dstRect.top, 0, m_width, m_height), XMFLOAT2(0, 0), 0xffffffff },
		{ PointToNdc(dstRect.right, dstRect.top, 0, m_width, m_height), XMFLOAT2(1, 0), 0xffffffff },
		{ PointToNdc(dstRect.left, dstRect.bottom, 0, m_width, m_height), XMFLOAT2(0, 1), 0xffffffff },
		{ PointToNdc(dstRect.right, dstRect.bottom, 0, m_width, m_height), XMFLOAT2(1, 1), 0xffffffff },
	};
	UINT stride = sizeof(VertexIn);
	UINT offset = 0;
	CComPtr<ID3D11Buffer> vb = m_vertexBuffers->updateVertex(m_d3d11Device, (const uint8_t*)vertrices, sizeof(vertrices));
	ID3D11Buffer* pVBBuffer = vb;
	m_d3d11DeviceContext->IASetVertexBuffers(0, 1, &pVBBuffer, &stride, &offset);
	m_d3d11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_d3d11DeviceContext->Draw(4, 0);
}
