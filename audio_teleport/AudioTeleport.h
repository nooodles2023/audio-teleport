#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <queue>


struct WavData {
  std::vector<float> samples;
  int frameIndex;
  int maxFrameIndex;
};

struct OpusEncoder;
typedef void (*LOOPBACKDATACALLBACK)(BYTE*, size_t);

class AudioTeleport {
private:
  AudioTeleport();
  ~AudioTeleport();
  AudioTeleport(const AudioTeleport&) = delete;
  AudioTeleport& operator=(const AudioTeleport&) = delete;

  std::string virtual_render_device_name_ = "CABLE Input (VB-Audio Virtual Cable)";
  std::wstring virtual_capture_device_name_ = TEXT("CABLE Output (VB-Audio Virtual Cable)");

  std::wstring default_capture_device_id_;
  int pa_virtual_render_device_index_ = -1;

  std::string loopback_buffer_;
  std::queue<std::string> loopback_opus_packet_queue_;
  std::mutex mutex_;
  std::thread loopback_thread_;
  bool loopback_recording_ = false;
  OpusEncoder* opus_encoder_ = nullptr;
  bool stop_playing_on_virtual_render_ = false;
  LOOPBACKDATACALLBACK loopback_listener_ = nullptr;

  int GetPaDeviceIdByDeviceName(const std::string& device_name);
  bool ResampleAudio(const std::vector<float>& input, int src_sample_rate, int new_sample_rate, int src_channels, std::vector<float>& output);
  void ConvertMonoToStereo(const std::vector<float>& mono_samples, std::vector<float>& stereo_samples);
  void AudioLoopBack();

public:
  static AudioTeleport& GetInstance() {
    static AudioTeleport instance;
    return instance;
  }
    
  std::wstring& DefaultCaptureDeviceID() { return this->default_capture_device_id_; }
  std::wstring& VirtualCaptureDeviceName() { return this->virtual_capture_device_name_; }

  bool GetDefaultCaptureDeviceIDFromSys();
  bool GetDeviceIdByDeviceName(bool capture, std::wstring& device_name, std::wstring& device_id);
  bool SetDefaultDevice(std::wstring& devID);

  void dumploopbuffer();

  void StartLoopbackRecord();
  void StopLoopbackRecord();
  void GetLoopbackBuffer(char* data, size_t size, size_t* out_size);
  void GetOneLoopbackOpusPacket(char* data, size_t size, size_t* out_size);
  void PlayWavToVirtualRender(std::string wav_file_path);
  void PlayPCMToVirtualRender(const char* pcm_data, int samplerate, int frames, int channels);
  void StopPlayingOnVirtualRender();
  void ClearLoopbackCache();
  void RegisterLoopbackListener(LOOPBACKDATACALLBACK callback);
  void UnRegisterLoopbackListener();
};
