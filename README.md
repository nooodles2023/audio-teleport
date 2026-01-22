EXPORT:
  
   void PlayWavToVirtualRender(const char* wav_file_path);
  
   void PlayPCMToVirtualRender(const char* pcm_data, int samplerate, int frames, int channels);
  
   void StopPlayingOnVirtualRender();
  
   void StartLoopbackRecord();
  
   void StopLoopbackRecord();
  
   void GetLoopbackBuffer(char* data, size_t size, size_t* out_size);
  
   void GetOneLoopbackOpusPacket(char* data, size_t size, size_t* out_size);
  
   void RegisterLoopbackListener(void* callback);
  
   void UnRegisterLoopbackListener();
  
