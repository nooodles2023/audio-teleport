// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the AUDIOTELEPORT_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// AUDIOTELEPORT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef AUDIOTELEPORT_EXPORTS
#define AUDIOTELEPORT_API __declspec(dllexport)
#else
#define AUDIOTELEPORT_API __declspec(dllimport)
#endif


#ifdef __cplusplus
extern "C" {
#endif

  AUDIOTELEPORT_API bool init();
  AUDIOTELEPORT_API void quit();

  AUDIOTELEPORT_API void PlayWavToVirtualRender(const char* wav_file_path);
  AUDIOTELEPORT_API void PlayPCMToVirtualRender(const char* pcm_data, int samplerate, int frames, int channels);
  AUDIOTELEPORT_API void StopPlayingOnVirtualRender();
  AUDIOTELEPORT_API void StartLoopbackRecord();
  AUDIOTELEPORT_API void StopLoopbackRecord();
  AUDIOTELEPORT_API void GetLoopbackBuffer(char* data, size_t size, size_t* out_size);
  AUDIOTELEPORT_API void GetOneLoopbackOpusPacket(char* data, size_t size, size_t* out_size);
  AUDIOTELEPORT_API void RegisterLoopbackListener(void* callback);
  AUDIOTELEPORT_API void UnRegisterLoopbackListener();


#ifdef __cplusplus
}
#endif