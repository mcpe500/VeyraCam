#include "mf_decoder.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <wmcodecdsp.h>
#include <codecapi.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace veyra {

MFDecoder::MFDecoder() = default;

MFDecoder::~MFDecoder() {
    Shutdown();
}

bool MFDecoder::Initialize(uint32_t width, uint32_t height, bool enableHardwareAcceleration) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    width_ = width;
    height_ = height;
    isHardwareAccelerated_ = enableHardwareAcceleration;

#ifdef _WIN32
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::cerr << "[MFDecoder] MFStartup failed: " << std::hex << hr << std::endl;
        return false;
    }

    // 1. Create H.264 Decoder MFT
    hr = CoCreateInstance(
        CLSID_CMSH264DecoderMFT,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&decoderMft_)
    );
    if (FAILED(hr)) {
        std::cerr << "[MFDecoder] Failed to create CLSID_CMSH264DecoderMFT" << std::endl;
        MFShutdown();
        return false;
    }

    // 2. Hardware acceleration setup with D3D11 if requested
    if (enableHardwareAcceleration) {
        UINT createDeviceFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL featureLevel;

        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &d3d11Device_,
            &featureLevel,
            &d3d11Context_
        );

        if (SUCCEEDED(hr)) {
            UINT resetToken = 0;
            hr = MFCreateDXGIDeviceManager(&resetToken, &dxgiDeviceManager_);
            if (SUCCEEDED(hr)) {
                dxgiDeviceManager_->ResetDevice(d3d11Device_.Get(), resetToken);
                decoderMft_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(dxgiDeviceManager_.Get()));
                isHardwareAccelerated_ = true;
            }
        } else {
            isHardwareAccelerated_ = false;
        }
    }

    // 3. Set Input Media Type (H.264)
    ComPtr<IMFMediaType> inputType;
    MFCreateMediaType(&inputType);
    inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width_, height_);

    hr = decoderMft_->SetInputType(0, inputType.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "[MFDecoder] Failed to set input media type" << std::endl;
        Shutdown();
        return false;
    }

    // 4. Set Output Media Type (NV12)
    ComPtr<IMFMediaType> outputType;
    MFCreateMediaType(&outputType);
    outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width_, height_);

    hr = decoderMft_->SetOutputType(0, outputType.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "[MFDecoder] Failed to set output media type" << std::endl;
        Shutdown();
        return false;
    }

    decoderMft_->GetOutputStreamInfo(0, &outputStreamInfo_);
    decoderMft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    decoderMft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoderMft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
#endif

    initialized_ = true;
    return true;
}

bool MFDecoder::DecodeFrame(const uint8_t* h264Data, size_t dataSize, uint64_t timestampUs, bool isKeyframe) {
    if (!initialized_ || !h264Data || dataSize == 0) return false;

    std::lock_guard<std::mutex> lock(mutex_);

#ifdef _WIN32
    // 1. Create Media Foundation Sample from raw NAL bytes
    ComPtr<IMFMediaBuffer> inputMediaBuffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(dataSize), &inputMediaBuffer);
    if (FAILED(hr)) return false;

    BYTE* destBuffer = nullptr;
    inputMediaBuffer->Lock(&destBuffer, nullptr, nullptr);
    memcpy(destBuffer, h264Data, dataSize);
    inputMediaBuffer->Unlock();
    inputMediaBuffer->SetCurrentLength(static_cast<DWORD>(dataSize));

    ComPtr<IMFSample> inputSample;
    MFCreateSample(&inputSample);
    inputSample->AddBuffer(inputMediaBuffer.Get());
    inputSample->SetSampleTime(static_cast<LONGLONG>(timestampUs * 10)); // 100ns units
    if (isKeyframe) {
        inputSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    }

    // 2. Feed into Decoder MFT
    hr = decoderMft_->ProcessInput(0, inputSample.Get(), 0);
    if (FAILED(hr)) {
        return false;
    }

    // 3. Extract decoded NV12 output sample
    MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
    ComPtr<IMFSample> outputSample;
    ComPtr<IMFMediaBuffer> outputMediaBuffer;

    if (!(outputStreamInfo_.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        hr = MFCreateSample(&outputSample);
        if (FAILED(hr)) return false;

        DWORD bufferSize = outputStreamInfo_.cbSize ? outputStreamInfo_.cbSize : (width_ * height_ * 3 / 2);
        hr = MFCreateMemoryBuffer(bufferSize, &outputMediaBuffer);
        if (FAILED(hr)) return false;

        outputSample->AddBuffer(outputMediaBuffer.Get());
        outputDataBuffer.pSample = outputSample.Get();
    }

    DWORD processOutputStatus = 0;
    hr = decoderMft_->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);
    if (SUCCEEDED(hr) && outputDataBuffer.pSample) {
        ComPtr<IMFMediaBuffer> outBuffer;
        outputDataBuffer.pSample->ConvertToContiguousBuffer(&outBuffer);
        if (outBuffer) {
            BYTE* nv12Ptr = nullptr;
            DWORD maxLen = 0, currentLen = 0;
            outBuffer->Lock(&nv12Ptr, &maxLen, &currentLen);

            DecodedFrameNV12 frame;
            frame.width = width_;
            frame.height = height_;
            frame.stride = width_;
            frame.timestampUs = timestampUs;

            size_t ySize = width_ * height_;
            size_t uvSize = width_ * height_ / 2;

            if (currentLen >= ySize + uvSize) {
                frame.yPlane.assign(nv12Ptr, nv12Ptr + ySize);
                frame.uvPlane.assign(nv12Ptr + ySize, nv12Ptr + ySize + uvSize);
            }

            outBuffer->Unlock();

            if (callback_) {
                callback_(frame);
            }
        }
    }
#else
    // Non-Windows stub / simulation fallback for testing
    if (callback_) {
        DecodedFrameNV12 frame;
        frame.width = width_;
        frame.height = height_;
        frame.stride = width_;
        frame.timestampUs = timestampUs;
        frame.yPlane.resize(width_ * height_, 128);
        frame.uvPlane.resize(width_ * height_ / 2, 128);
        callback_(frame);
    }
#endif

    return true;
}

void MFDecoder::Flush() {
    std::lock_guard<std::mutex> lock(mutex_);
#ifdef _WIN32
    if (decoderMft_) {
        decoderMft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    }
#endif
}

void MFDecoder::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) return;

#ifdef _WIN32
    if (decoderMft_) {
        decoderMft_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        decoderMft_.Reset();
    }
    dxgiDeviceManager_.Reset();
    d3d11Context_.Reset();
    d3d11Device_.Reset();
    MFShutdown();
#endif

    initialized_ = false;
}

} // namespace veyra
