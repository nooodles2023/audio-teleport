#include "export.h"
#include <windows.h>
#include "AudioTeleport.h"
#include "debug.h"


bool init() {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.GetDefaultCaptureDeviceIDFromSys();

  std::wstring virtual_capture_device_id;
  if (!at.GetDeviceIdByDeviceName(true, at.VirtualCaptureDeviceName(), virtual_capture_device_id)) {
    AT_ERROR("failed to get device=%s", at.VirtualCaptureDeviceName().c_str());
    return false;
  }

  if (!at.SetDefaultDevice(virtual_capture_device_id)) {
    AT_ERROR("failed to set default deviceID=%s", virtual_capture_device_id.c_str());
    return false;
  }

  AT_LOG("init success");
  return true;
}

void quit() {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.StopLoopbackRecord();
  at.ClearLoopbackCache();
  std::wstring& defaultDeviceID = at.DefaultCaptureDeviceID();
  if (!defaultDeviceID.empty() && !at.SetDefaultDevice(defaultDeviceID)) {
    AT_ERROR("failed to recover default deviceID=%s", defaultDeviceID.c_str());
  }
}

void PlayWavToVirtualRender(const char* wav_file_path) {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.PlayWavToVirtualRender(wav_file_path);
}

 void PlayPCMToVirtualRender(const char* pcm_data, int samplerate, int frames, int channels) {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.PlayPCMToVirtualRender(pcm_data, samplerate, frames, channels);
 }

void StopPlayingOnVirtualRender() {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.StopPlayingOnVirtualRender();
}

void StartLoopbackRecord() {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.StartLoopbackRecord();
}

void StopLoopbackRecord() {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.StopLoopbackRecord();
}

void GetLoopbackBuffer(char* data, size_t size, size_t* out_size) {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.GetLoopbackBuffer(data, size, out_size);
}

void GetOneLoopbackOpusPacket(char* data, size_t size, size_t* out_size) {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.GetOneLoopbackOpusPacket(data, size, out_size);
}

void RegisterLoopbackListener(void* callback) {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.RegisterLoopbackListener((LOOPBACKDATACALLBACK)callback);
}

void UnRegisterLoopbackListener() {
  AT_TAG();

  AudioTeleport& at = AudioTeleport::GetInstance();
  at.UnRegisterLoopbackListener();
}