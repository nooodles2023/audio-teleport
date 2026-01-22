import ctypes
import time
import threading
import soundfile as sf
import io

CALLBACK_FUNC_TYPE = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t)
dll_path = 'F:\\source\\audio-teleport\\x64\\Debug\\audioteleport.dll'  # 替换为你DLL的实际路径
audio_teleport = ctypes.CDLL(dll_path)


def stop_play_wav_on_virtual_render():
    # 等待播放2s
    time.sleep(2)
    
    audio_teleport.StopPlayingOnVirtualRender.argtypes = []
    audio_teleport.StopPlayingOnVirtualRender.restype = ctypes.c_voidp
    audio_teleport.StopPlayingOnVirtualRender()


@CALLBACK_FUNC_TYPE
def loopback_data_callback(data, size):
    buffer = bytes(data[:size])
    with open("output.pcm", 'ab') as file:
        file.write(buffer)


if __name__ == "__main__":
    # 初始化audio_teleport
    audio_teleport.init.argtypes = []
    audio_teleport.init.restype = ctypes.c_bool
    if audio_teleport.init():
        # # 播放pcm数据到虚拟麦克风
        # with open('C:\\Users\\cair\\Desktop\\test111.pcm', 'rb') as file:
        #     wav_data = file.read()
            
        #     audio_teleport.PlayPCMToVirtualRender.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
        #     audio_teleport.PlayPCMToVirtualRender.restype = ctypes.c_voidp
        #     audio_teleport.PlayPCMToVirtualRender(wav_data, 48000, 400000, 2)

        # # 播放wav数据到虚拟麦克风
        # with open('c:\\w1.wav', 'rb') as file:
        #     wav_data = file.read()
        #     virtual_file = io.BytesIO(wav_data)

        #     # 使用 soundfile 库打开类文件对象
        #     with sf.SoundFile(virtual_file) as sound:
        #         data = sound.read(dtype='float32')
        #         audio_teleport.PlayPCMToVirtualRender.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
        #         audio_teleport.PlayPCMToVirtualRender.restype = ctypes.c_voidp
        #         audio_teleport.PlayPCMToVirtualRender(data.ctypes.data_as(ctypes.POINTER(ctypes.c_char)), sound.samplerate, sound.frames, sound.channels)
            
        # 创建一个线程模拟执行停止播放动作
        # thread = threading.Thread(target=stop_play_wav_on_virtual_render)
        # thread.start()
        
        # 播放wav文件到虚拟麦克风，观察系统的虚拟麦克风设备，此时应该有信号波动
        # PlayWavToVirtualRender默认会等待文件播放完成后返回
        # audio_teleport.PlayWavToVirtualRender.argtypes = [ctypes.c_char_p]
        # audio_teleport.PlayWavToVirtualRender.restype = ctypes.c_voidp
        # audio_teleport.PlayWavToVirtualRender('c:\\w1.wav'.encode('utf-8'))
        # thread.join()
        
        # 开始监听扬声器
        audio_teleport.StartLoopbackRecord.argtypes = []
        audio_teleport.StartLoopbackRecord.restype = ctypes.c_voidp
        audio_teleport.StartLoopbackRecord()
        
        audio_teleport.RegisterLoopbackListener.argtypes = [CALLBACK_FUNC_TYPE]
        audio_teleport.RegisterLoopbackListener.restype = ctypes.c_voidp
        audio_teleport.RegisterLoopbackListener(loopback_data_callback)
        
        time.sleep(5)
        
        audio_teleport.UnRegisterLoopbackListener.argtypes = []
        audio_teleport.UnRegisterLoopbackListener.restype = ctypes.c_voidp
        audio_teleport.UnRegisterLoopbackListener()
        
        time.sleep(5)
        
        # 创建字符缓冲区
        buffer_size = 1024 * 1024 * 5
        buffer = ctypes.create_string_buffer(buffer_size)

        # 创建用于接收输出大小的变量，初始化为0
        out_size = ctypes.c_size_t(0)

        # 获取从开始监听到目前为止所有的声音数据（pcm格式）
        audio_teleport.GetLoopbackBuffer.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p]
        audio_teleport.GetLoopbackBuffer.restype = ctypes.c_voidp
        audio_teleport.GetLoopbackBuffer(buffer, buffer_size, ctypes.byref(out_size))

        # 截取实际pcm数据长度
        data_received = buffer[:out_size.value]
        
        # 将缓冲区的数据写入文件，ffplay.exe -f f32le -ar 48000 -ac 2 -i output.pcm 可用来测试播放pcm
        file_path = 'output1.pcm'  # 你希望保存数据的文件路径和文件名
        with open(file_path, 'wb') as file:
            file.write(data_received)
        
        # 停止监听扬声器
        audio_teleport.StopLoopbackRecord.argtypes = []
        audio_teleport.StopLoopbackRecord.restype = ctypes.c_voidp
        audio_teleport.StopLoopbackRecord()
        
        # 退出audio_teleport
        audio_teleport.quit.argtypes = []
        audio_teleport.quit.restype = ctypes.c_voidp
        audio_teleport.quit()
    else:
        print("init failed")