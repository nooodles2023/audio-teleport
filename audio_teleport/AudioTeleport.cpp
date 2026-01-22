#include <string>
#include <thread>
#include <audioclient.h>
#include <fstream>
#include "Mmdeviceapi.h"
#include "PolicyConfig.h"
#include "Propidl.h"
#include "Functiondiscoverykeys_devpkey.h"
#include "AudioTeleport.h"
#include "debug.h"


#ifdef _WIN64
#include "../libs/x64/libopus/include/opus.h"
#include "../libs/x64/libsamplerate-0.2.2/include/samplerate.h"
#include "../libs/x64/libsndfile-1.2.2/include/sndfile.h"
#include "../libs/x64/portaudio/include/portaudio.h"

#pragma comment(lib, "../libs/x64/libopus/lib/opus.lib")
#pragma comment(lib, "../libs/x64/portaudio/lib/portaudio_x64.lib")
#pragma comment(lib, "../libs/x64/libsndfile-1.2.2/lib/sndfile.lib")
#pragma comment(lib, "../libs/x64/libsamplerate-0.2.2/lib/samplerate.lib")
#else
#include "../libs/x86/libopus/include/opus.h"
#include "../libs/x86/libsamplerate-0.2.2/include/samplerate.h"
#include "../libs/x86/libsndfile-1.2.2/include/sndfile.h"
#include "../libs/x86/portaudio/include/portaudio.h"

#pragma comment(lib, "../libs/x86/libopus/lib/opus.lib")
#pragma comment(lib, "../libs/x86/portaudio/lib/portaudio_x86.lib")
#pragma comment(lib, "../libs/x86/libsndfile-1.2.2/lib/sndfile.lib")
#pragma comment(lib, "../libs/x86/libsamplerate-0.2.2/lib/samplerate.lib")
#endif  // _WIN64


#define LOOPBACK_BUFFER_SIZE 1024 * 1024 * 100
#define OPUS_SAMPLES_COUNT_PER_CHANNEL 480
#define OPUS_SAMPLES_COUNT_PER_CHANNEL_IN_BYTES OPUS_SAMPLES_COUNT_PER_CHANNEL * sizeof(float)

static int PlayCallback(const void* inputBuffer, void* outputBuffer,
                        unsigned long framesPerBuffer,
                        const PaStreamCallbackTimeInfo* timeInfo,
                        PaStreamCallbackFlags statusFlags, void* userData) {
  WavData* data = (WavData*)userData;
  float* out = (float*)outputBuffer;
  unsigned long i;

  for (i = 0; i < framesPerBuffer; i++) {
    if (data->frameIndex < data->maxFrameIndex) {
      *out++ = data->samples[data->frameIndex++];  // left
      *out++ = data->samples[data->frameIndex++];  // right
    } else {
      *out++ = 0;
      *out++ = 0;
    }
  }

  return (data->frameIndex < data->maxFrameIndex) ? paContinue : paComplete;
}

AudioTeleport::AudioTeleport() {
  HRESULT hr = CoInitialize(NULL);
  AT_ASSERT(hr == ERROR_SUCCESS);

  PaError err = Pa_Initialize();
  AT_ASSERT(err == paNoError);

  loopback_buffer_.clear();
  loopback_buffer_.reserve(LOOPBACK_BUFFER_SIZE);

  int error;
  opus_encoder_ = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
  if (error != OPUS_OK) {
    AT_PANIC("failed to create opus encoder error=%s", opus_strerror(error));
  }
}

AudioTeleport::~AudioTeleport() {
  opus_encoder_destroy(opus_encoder_);
  StopLoopbackRecord();
  Pa_Terminate();
  CoUninitialize();
}

void AudioTeleport::StartLoopbackRecord() {
  AT_TAG();

  if (loopback_thread_.joinable()) {
    AT_ERROR("current loopback thread is still running");
    return;
  }

  this->loopback_recording_ = true;
  loopback_thread_ = std::thread(&AudioTeleport::AudioLoopBack, this);
}

void AudioTeleport::StopLoopbackRecord() {
  AT_TAG();

  this->loopback_recording_ = false;
  if (loopback_thread_.joinable()) {
    loopback_thread_.join();
  }
}

void AudioTeleport::ClearLoopbackCache() {
  while (!this->loopback_opus_packet_queue_.empty()) {
    this->loopback_opus_packet_queue_.pop();
  }
  this->loopback_buffer_.clear();
}

void AudioTeleport::RegisterLoopbackListener(LOOPBACKDATACALLBACK callback) {
  this->loopback_listener_ = callback;
}

void AudioTeleport::UnRegisterLoopbackListener() {
  this->loopback_listener_ = nullptr;
}

void AudioTeleport::GetOneLoopbackOpusPacket(char* data, size_t size, size_t* out_size) {
  *out_size = 0;
  this->mutex_.lock();

  // pcm data's format in loopback_buffer_ is f32le
  // opus encoder with 48000hz needs 120 samples for one frame at least
  // Why frame_byts is different from https://gitlab.weike.fm/sfcloud/sweet-server/-/blob/main/server.cc?
  auto remain = this->loopback_buffer_.size();
  auto ptr = (uint8_t*)this->loopback_buffer_.data();
  while (remain >= OPUS_SAMPLES_COUNT_PER_CHANNEL_IN_BYTES) {
    std::string packet(1000, '\0');
    auto ret = opus_encode_float(opus_encoder_, (const float*)ptr, OPUS_SAMPLES_COUNT_PER_CHANNEL, (unsigned char*)packet.data(), (opus_int32)packet.size());
    if (ret > 0) {
      packet.resize(ret);
      this->loopback_opus_packet_queue_.emplace(std::move(packet));
    }
    ptr += OPUS_SAMPLES_COUNT_PER_CHANNEL_IN_BYTES;
    remain -= OPUS_SAMPLES_COUNT_PER_CHANNEL_IN_BYTES;
  }

  if (remain > 0) {
    this->loopback_buffer_.resize(remain);
  } else {
    this->loopback_buffer_.clear();
  }

  if (!this->loopback_opus_packet_queue_.empty()) {
    auto& packet = this->loopback_opus_packet_queue_.front();
    AT_ASSERT(size > packet.size());
    memcpy(data, packet.data(), packet.size());
    *out_size = packet.size();
    this->loopback_opus_packet_queue_.pop();
  }

  this->mutex_.unlock();
}

void AudioTeleport::GetLoopbackBuffer(char* data, size_t size, size_t* out_size) {
  *out_size = 0;
  this->mutex_.lock();

  if (!this->loopback_buffer_.empty()) {
    if (this->loopback_buffer_.size() > size) {
      memcpy(data, (void*)this->loopback_buffer_.data(), size);
      *out_size = size;
      this->loopback_buffer_.erase(0, size);
    } else {
      memcpy(data, (void*)this->loopback_buffer_.data(), this->loopback_buffer_.size());
      *out_size = this->loopback_buffer_.size();
      this->loopback_buffer_.clear();
    }
  }

  this->mutex_.unlock();
}

void AudioTeleport::AudioLoopBack() {
  AT_LOG("loopback start.");

  HRESULT hr = CoInitialize(NULL);
  AT_ASSERT(ERROR_SUCCESS == hr);

  IMMDeviceEnumerator* pEnum = nullptr;
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
  AT_ASSERT(ERROR_SUCCESS == hr);

  IMMDevice* pDefDevice;
  hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDefDevice);
  AT_ASSERT(ERROR_SUCCESS == hr);
  pEnum->Release();

  IAudioClient* pAudioClient = nullptr;
  hr = pDefDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
  AT_ASSERT(ERROR_SUCCESS == hr);

  WAVEFORMATEX* pwfx = nullptr;
  hr = pAudioClient->GetMixFormat(&pwfx);
  AT_ASSERT(ERROR_SUCCESS == hr);

  // Check if the format is WAVE_FORMAT_EXTENSIBLE and IEEE Float
  bool isFloat = false;
  if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE* pWaveFormatExtensible = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
    if (pWaveFormatExtensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
      isFloat = true;
    }
  } else if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    isFloat = true;
  }
  AT_ASSERT(isFloat);

  hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, nullptr);
  AT_ASSERT(ERROR_SUCCESS == hr);

  IAudioCaptureClient* pCaptureClient = nullptr;
  hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
  AT_ASSERT(ERROR_SUCCESS == hr);

  pAudioClient->Start();

  BYTE* pData;
  DWORD flags;
  UINT32 numFramesAvailable;
  UINT32 packetLength = 0;
  while (this->loopback_recording_) {
    pCaptureClient->GetNextPacketSize(&packetLength);

    while (packetLength != 0) {
      pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
      size_t data_size = (size_t)numFramesAvailable * pwfx->nBlockAlign;
      if (this->loopback_listener_) {
        this->loopback_listener_(pData, data_size);
      } else {
        this->mutex_.lock();
        if (this->loopback_buffer_.size() + data_size > this->loopback_buffer_.capacity()) {
          AT_LOG("loopback buffer overflow!");
          this->loopback_buffer_.clear();
        }
        this->loopback_buffer_.append((char*)pData, data_size);
        this->mutex_.unlock();
      }
      pCaptureClient->ReleaseBuffer(numFramesAvailable);
      pCaptureClient->GetNextPacketSize(&packetLength);
    }
  }

  pAudioClient->Stop();

  CoTaskMemFree(pwfx);
  pCaptureClient->Release();
  pAudioClient->Release();
  pDefDevice->Release();
  CoUninitialize(); 

  AT_LOG("loopback done.");
}

void AudioTeleport::dumploopbuffer() {
  std::ofstream file("tmp.pcm", std::ios::binary);
  file.write(this->loopback_buffer_.data(), this->loopback_buffer_.size());
  file.close();
}

bool AudioTeleport::GetDefaultCaptureDeviceIDFromSys() {
  bool bSuccess = false;

  IMMDeviceEnumerator* pEnum = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
  if (!SUCCEEDED(hr)) {
    goto clean;
  }

  IMMDevice* pDefDevice;
  hr = pEnum->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDefDevice);
  if (!SUCCEEDED(hr)) {
    goto clean;
  }

  LPWSTR devID;
  hr = pDefDevice->GetId(&devID);
  if (SUCCEEDED(hr)) {
    this->default_capture_device_id_ = devID;
    AT_LOG("current default microphone id=%ws\n", this->default_capture_device_id_.c_str());
    bSuccess = true;
  }
  pDefDevice->Release();

clean:
  if (pEnum != nullptr) {
    pEnum->Release();
  }

  return bSuccess;
}

bool AudioTeleport::GetDeviceIdByDeviceName(bool capture, std::wstring& device_name, std::wstring& device_id) {
  bool bFind = false;
  IMMDeviceEnumerator* pEnum = nullptr;
  IMMDeviceCollection* pDevices = nullptr;

  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
  if (!SUCCEEDED(hr)) {
    AT_ERROR("CoCreateInstance MMDeviceEnumerator failed=%d", GetLastError());
    goto clean;
  }

  hr = pEnum->EnumAudioEndpoints(capture ? eCapture : eRender, DEVICE_STATE_ACTIVE, &pDevices);
  if (!SUCCEEDED(hr)) {
    AT_ERROR("EnumAudioEndpoints failed=%d", GetLastError());
    goto clean;
  }

  UINT count;
  pDevices->GetCount(&count);
  if (!SUCCEEDED(hr)) {
    AT_ERROR("pDevices GetCount failed=%d", GetLastError());
    goto clean;
  }

  for (UINT i = 0; i < count; i++) {
    IMMDevice* pDevice;
    hr = pDevices->Item(i, &pDevice);
    if (SUCCEEDED(hr)) {
      LPWSTR devID;
      hr = pDevice->GetId(&devID);
      if (SUCCEEDED(hr)) {
        IPropertyStore* pStore;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pStore);
        if (SUCCEEDED(hr)) {
          PROPVARIANT friendlyName;
          PropVariantInit(&friendlyName);
          hr = pStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
          if (SUCCEEDED(hr)) {
            std::wstring strTmp = friendlyName.pwszVal;
            AT_LOG("find device name=%ws\n", strTmp.c_str());

            if (strTmp == device_name) {
              bFind = true;
              device_id = devID;
            }
            PropVariantClear(&friendlyName);
          }
          pStore->Release();
        }
      }
      pDevice->Release();
    }

    if (bFind) {
      break;
    }
  }

clean:
  if (pDevices != nullptr) {
    pDevices->Release();
  }

  if (pEnum != nullptr) {
    pEnum->Release();
  }

  return bFind;
}

bool AudioTeleport::SetDefaultDevice(std::wstring& devID) {
  IPolicyConfigVista* pPolicyConfig;
  HRESULT hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID*)&pPolicyConfig);
  if (!SUCCEEDED(hr)) {
    AT_ERROR("CoCreateInstance CPolicyConfigVistaClient failed=%d", hr);
    return false;
  }

  hr = pPolicyConfig->SetDefaultEndpoint(devID.c_str(), eConsole);
  pPolicyConfig->Release();

  if (!SUCCEEDED(hr)) {
    AT_ERROR("SetDefaultEndpoint failed=%d", hr);
    return false;
  }

  return true;
}

int AudioTeleport::GetPaDeviceIdByDeviceName(const std::string& device_name) {
  int numDevices = Pa_GetDeviceCount();
  if (numDevices < 0) {
    AT_ERROR("ERROR: Pa_CountDevices returned %d", numDevices);
    return paNoDevice;
  }

  int selectedDeviceIndex = paNoDevice;
  for (int i = 0; i < numDevices; ++i) {
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
    if (deviceInfo && deviceInfo->maxOutputChannels > 0) {
      std::string deviceName(deviceInfo->name);
      if (deviceName.find(device_name) != std::string::npos) {
        selectedDeviceIndex = i;
        break;
      }
    }
  }

  return selectedDeviceIndex;
}

void AudioTeleport::PlayPCMToVirtualRender(const char* pcm_data, int samplerate, int frames, int channels) {
  this->stop_playing_on_virtual_render_ = false;

  // get virtual render device index
  if (this->pa_virtual_render_device_index_ == -1) {
    this->pa_virtual_render_device_index_ = this->GetPaDeviceIdByDeviceName(this->virtual_render_device_name_);
    AT_ASSERT(this->pa_virtual_render_device_index_ != paNoDevice);
  }

  WavData data;
  data.frameIndex = 0;
  data.maxFrameIndex = frames * channels;
  data.samples.resize(data.maxFrameIndex);
  memcpy(data.samples.data(), pcm_data, data.maxFrameIndex * sizeof(float));

  // resample it to 48000hz
  if (samplerate != 48000) {
    std::vector<float> resamples;
    if (!this->ResampleAudio(data.samples, samplerate, 48000, channels, resamples)) {
      AT_ERROR("failed to resample wav");
      return;
    }

    data.maxFrameIndex = (int)resamples.size();
    data.samples = std::move(resamples);
  }

  // resample it to 2 channels
  if (channels != 2) {
    std::vector<float> stereo_samples;
    this->ConvertMonoToStereo(data.samples, stereo_samples);

    data.maxFrameIndex = (int)stereo_samples.size();
    data.samples = std::move(stereo_samples);
  }

  // open wav data with target render device
  PaStreamParameters outputParameters;
  outputParameters.device = this->pa_virtual_render_device_index_;
  outputParameters.channelCount = 2;         
  outputParameters.sampleFormat = paFloat32;
  outputParameters.suggestedLatency = Pa_GetDeviceInfo(this->pa_virtual_render_device_index_)->defaultLowOutputLatency;
  outputParameters.hostApiSpecificStreamInfo = nullptr;

  PaStream* stream;
  PaError err = Pa_OpenStream(&stream, nullptr, &outputParameters, 48000, paFramesPerBufferUnspecified, paNoFlag, PlayCallback, &data);                    
  //PaError err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, 48000, paFramesPerBufferUnspecified, PlayCallback, &data);
  if (err != paNoError) {
    AT_ERROR("Pa_OpenStream failed err=%s", Pa_GetErrorText(err));
    return;
  }

  // start playing wav
  err = Pa_StartStream(stream);
  if (err != paNoError) {
    AT_ERROR("Pa_StartStream failed err=%s", Pa_GetErrorText(err));
    Pa_CloseStream(stream);
    return;
  }

  // wait for the stop signal or playing finished
  while (!this->stop_playing_on_virtual_render_) {
    err = Pa_IsStreamActive(stream);
    if (err != 1) {
      break;
    }
    Pa_Sleep(10);
  }

  // stop it and close the stream
  err = Pa_StopStream(stream);
  if (err != paNoError) {
    AT_ERROR("Pa_StopStream failed err=%s", Pa_GetErrorText(err));
  }
  Pa_CloseStream(stream);
}

void AudioTeleport::PlayWavToVirtualRender(std::string wav_file_path) {
  SF_INFO sfInfo;
  SNDFILE* sf = sf_open(wav_file_path.c_str(), SFM_READ, &sfInfo);
  if (!sf) {
    AT_ERROR("failed to open wav file=%s", wav_file_path.c_str());
    return;
  }

  // read origin wav file to buffer
  auto max_frame_index = sfInfo.frames * sfInfo.channels;
  std::string samples((size_t)max_frame_index * sizeof(float), '\0');
  sf_read_float(sf, (float*)samples.data(), max_frame_index);
  sf_close(sf);

  AT_ASSERT(sfInfo.format == (SF_FORMAT_WAV | SF_FORMAT_PCM_16));

  this->PlayPCMToVirtualRender(samples.data(), sfInfo.samplerate, (int)sfInfo.frames, sfInfo.channels);
}

void AudioTeleport::StopPlayingOnVirtualRender() {
  this->stop_playing_on_virtual_render_ = true;
}

bool AudioTeleport::ResampleAudio(const std::vector<float>& input,
                                  int src_sample_rate, int new_sample_rate,
                                  int src_channels, std::vector<float>& output) {
  SRC_DATA src_data;
  src_data.data_in = input.data();
  src_data.input_frames = static_cast<long>(input.size());
  src_data.src_ratio = static_cast<double>(new_sample_rate) / src_sample_rate;

  long outputFrames = static_cast<long>(src_data.src_ratio * src_data.input_frames);
  output.resize(outputFrames);

  src_data.data_out = output.data();
  src_data.output_frames = outputFrames;

  int error = src_simple(&src_data, SRC_SINC_BEST_QUALITY, src_channels);
  if (error) {
    AT_ERROR("Error resampling=%s", src_strerror(error));
    return false;
  }

  output.resize(src_data.output_frames_gen);
  return true;
}

void AudioTeleport::ConvertMonoToStereo(const std::vector<float>& mono_samples, std::vector<float>& stereo_samples) {
  stereo_samples.resize(mono_samples.size() * 2);
  for (size_t i = 0; i < mono_samples.size(); ++i) {
    stereo_samples[2 * i] = mono_samples[i];      // left
    stereo_samples[2 * i + 1] = mono_samples[i];  // right
  }
}