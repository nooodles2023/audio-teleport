#include <iostream>
#include <windows.h>
#include <fstream>
#include <iostream>

typedef bool(*INIT)(void);
typedef void(*QUIT)(void);
typedef void (*PLAYWAVTOVIRTUALRENDER)(const char*);
typedef void (*STARTLOOPBACKRECORD)(void);
typedef void (*STOPLOOPBACKRECORD)(void);
typedef void (*GETLOOPBACKBUFFER)(char*, size_t, size_t*);
typedef void (*GETONELOOPBACKOPUSPACKET)(char*, size_t, size_t*);


#ifdef _WIN64
#include "../libs/x64/libopus/include/opus.h"
#pragma comment(lib, "../libs/x64/libopus/lib/opus.lib")
#else
#include "../libs/x86/libopus/include/opus.h"
#pragma comment(lib, "../libs/x86/libopus/lib/opus.lib")
#endif  // _WIN64


int main() {
  int error;
  OpusDecoder* decoder = opus_decoder_create(48000, 2, &error);
  if (error != OPUS_OK) {
    std::cerr << "无法创建解码器: " << opus_strerror(error) << std::endl;
    return -1;
  } 

  HMODULE handle = LoadLibraryA("audioteleport.dll");
  if (handle)
  {
    printf("handle=%p\n", handle);
    INIT init = (INIT)GetProcAddress(handle, "init");
    if (init) {
      init();
    }

    STARTLOOPBACKRECORD StartLoopbackRecord = (STARTLOOPBACKRECORD)GetProcAddress(handle, "StartLoopbackRecord");
    if (StartLoopbackRecord) {
      StartLoopbackRecord();
    }

    Sleep(10000);

    STOPLOOPBACKRECORD StopLoopbackRecord = (STOPLOOPBACKRECORD)GetProcAddress(handle, "StopLoopbackRecord");
    if (StopLoopbackRecord) {
      StopLoopbackRecord();
    }

    //PLAYWAVTOVIRTUALRENDER PlayWavToVirtualRender = (PLAYWAVTOVIRTUALRENDER)GetProcAddress(handle, "PlayWavToVirtualRender");
    //if (PlayWavToVirtualRender) {
    //  PlayWavToVirtualRender("c:\\w1.wav");
    //}

    GETONELOOPBACKOPUSPACKET GetOneLoopbackOpusPacket = (GETONELOOPBACKOPUSPACKET)GetProcAddress(handle, "GetOneLoopbackOpusPacket");
    if (GetOneLoopbackOpusPacket) {
      size_t out_size;
      size_t packet_size = 1024*1024*10;
      char* buffer = (char*)malloc(packet_size);
      char* pcm = (char*)malloc(packet_size);
      std::ofstream file("C:\\Users\\cair\\Desktop\\test222.opus.pcm", std::ios::binary);

      while (true) {
        memset(buffer, 0, packet_size);
        memset(pcm, 0, packet_size);
        GetOneLoopbackOpusPacket(buffer, packet_size, &out_size);
        //printf("out_size=%d\n", out_size);

        if (out_size == 0) {
          break;
        }

        int frame_size = opus_decode_float(decoder, (const unsigned char*)buffer, out_size, (float*)pcm, 480 , 0);
        if (frame_size < 0) {
          opus_decoder_destroy(decoder);
          printf("decode error=%s\n", opus_strerror(frame_size));
          return -1;
        }
        file.write((const char*)pcm, frame_size * sizeof(float));
      }

      file.close();
      free(buffer);
      free(pcm);
    }

    /*GETLOOPBACKBUFFER GetLoopbackBuffer = (GETLOOPBACKBUFFER)GetProcAddress(handle, "GetLoopbackBuffer");
    if (GetLoopbackBuffer) {
      size_t out_size;
      size_t buffer_size = 1024 * 1024 * 5;
      char* buffer = (char*)malloc(1024 * 1024 * 5);
      memset(buffer, 0, buffer_size);

      GetLoopbackBuffer(buffer, buffer_size, &out_size);
      printf("out_size=%d\n", out_size);

      if (out_size > 0) {
        std::ofstream file("C:\\Users\\cair\\Desktop\\test111.pcm", std::ios::binary);
        file.write(buffer, out_size);
        file.close();
      }
      free(buffer);
    }*/

    opus_decoder_destroy(decoder);

    QUIT quit = (QUIT)GetProcAddress(handle, "quit");
    if (quit) {
      quit();
    }

    FreeLibrary(handle);
  }

  std::cout << "work done!\n";
}
