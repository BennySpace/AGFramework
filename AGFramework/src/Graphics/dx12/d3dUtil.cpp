#include "d3dUtil.h"
#include <comdef.h>
#include <fstream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
bool FileExists(const std::wstring &filename)
{
	if (filename.empty())
	{
		return false;
	}

	const DWORD attributes = GetFileAttributesW(filename.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string WideToUtf8(const std::wstring &value)
{
	if (value.empty())
	{
		return std::string();
	}

	const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(sizeRequired > 0 ? sizeRequired - 1 : 0, '\0');
	if (sizeRequired > 1)
	{
		WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], sizeRequired - 1, nullptr, nullptr);
	}

	return result;
}
} // namespace

DxException::DxException(HRESULT hr, const std::wstring &functionName, const std::wstring &filename, int lineNumber)
    : ErrorCode(hr), FunctionName(functionName), Filename(filename), LineNumber(lineNumber)
{
}

ComPtr<ID3DBlob> d3dUtil::LoadBinary(const std::wstring &filename)
{
	if (!FileExists(filename))
	{
		throw std::runtime_error("Required binary asset not found: " + WideToUtf8(filename));
	}

	std::ifstream fin(filename, std::ios::binary);
	if (!fin)
	{
		throw std::runtime_error("Failed to open binary asset: " + WideToUtf8(filename));
	}

	fin.seekg(0, std::ios_base::end);
	std::ifstream::pos_type size = (int)fin.tellg();
	fin.seekg(0, std::ios_base::beg);

	ComPtr<ID3DBlob> blob;
	ThrowIfFailed(D3DCreateBlob(size, blob.GetAddressOf()));

	fin.read((char *)blob->GetBufferPointer(), size);
	fin.close();

	return blob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList,
                                                                    const void *initData, UINT64 byteSize,
                                                                    Microsoft::WRL::ComPtr<ID3D12Resource> &uploadBuffer)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	// Create the actual default buffer resource.
	const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
	                                              D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap.
	ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
	                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	const CD3DX12_RESOURCE_BARRIER toCopyDestBarrier =
	    CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &toCopyDestBarrier);
	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	const CD3DX12_RESOURCE_BARRIER toGenericReadBarrier =
	    CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	cmdList->ResourceBarrier(1, &toGenericReadBarrier);

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.

	return defaultBuffer;
}

ComPtr<ID3DBlob> d3dUtil::CompileShader(const std::wstring &filename, const D3D_SHADER_MACRO *defines, const std::string &entrypoint,
                                        const std::string &target)
{
	if (!FileExists(filename))
	{
		throw std::runtime_error("Required shader asset not found: " + WideToUtf8(filename));
	}

	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(), target.c_str(), compileFlags,
	                        0, &byteCode, &errors);

	if (errors != nullptr)
		OutputDebugStringA((char *)errors->GetBufferPointer());

	if (FAILED(hr) && errors != nullptr)
	{
		const char *shaderErrors = static_cast<const char *>(errors->GetBufferPointer());
		throw std::runtime_error("Failed to compile shader " + WideToUtf8(filename) + ": " + shaderErrors);
	}

	ThrowIfFailed(hr);

	return byteCode;
}

std::wstring DxException::ToString() const
{
	// Get the string description of the error code.
	_com_error err(ErrorCode);
	std::wstring msg = err.ErrorMessage();

	return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}
