#include <jni.h>


// for opensles
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "SoundTouch.h"
//打印日志
#include <android/log.h>
#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"ywl5320",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"ywl5320",FORMAT,##__VA_ARGS__);

// 引擎接口
SLObjectItf engineObject = NULL;
SLEngineItf engineEngine = NULL;

//混音器
SLObjectItf outputMixObject = NULL;
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

//pcm
SLObjectItf pcmPlayerObject = NULL;
SLPlayItf pcmPlayerPlay = NULL;
SLVolumeItf pcmPlayerVolume = NULL;

//缓冲器队列接口
SLAndroidSimpleBufferQueueItf pcmBufferQueue;



using namespace soundtouch;
SoundTouch *soundTouch;
FILE *pcmFile;
SAMPLETYPE *sd_buffer;
uint8_t *pcm_buffer; //用8位读取PCM数据
//uint16_t *pcm_buffer; //用16位读取PCM数据

void release()
{

    if (pcmPlayerObject != NULL) {
        (*pcmPlayerObject)->Destroy(pcmPlayerObject);
        pcmPlayerObject = NULL;
        pcmPlayerPlay = NULL;
        pcmPlayerVolume = NULL;
        pcmBufferQueue = NULL;
        pcmFile = NULL;
        pcm_buffer = NULL;
        sd_buffer = NULL;
    }

    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
        outputMixEnvironmentalReverb = NULL;
    }

    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
}
bool isfinish = true;
int num = 0;
int readPcmData()
{
    int size = 0;
    while(!feof(pcmFile))
    {
        if(isfinish)
        {
            isfinish = false;
            size = fread(pcm_buffer, 1, 4096 * 2, pcmFile);
            if(size > 0)
            {
                /**
                 * 用uint_16来存储PCM数据，就不需要下面的转换。
                 */
                for (int i = 0; i < size / 2 + 1; i++)
                {
                    sd_buffer[i] = (pcm_buffer[i * 2] | (pcm_buffer[i * 2 + 1] << 8));
                }
                soundTouch->putSamples((const SAMPLETYPE *) pcm_buffer, size / 4);
            } else{
                num = 0;
                soundTouch->clear();
            }
        }
        num = soundTouch->receiveSamples(sd_buffer, size / 4);
        if(num == 0)
        {
            isfinish = true;
            continue;
        }
        return num;
    }
    return size;
}

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void * context)
{
    int size = readPcmData();
    if(size > 0)
    {
        (*pcmBufferQueue)->Enqueue(pcmBufferQueue, sd_buffer, size * 4);
    } else{
        LOGI("解码完成...");
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_ywl5320_com_soundtouchdemo_SoundTouch_playAudioByOpenSL_1pcm(JNIEnv *env, jobject instance,
                                                                  jstring pamPath_) {
    release();
    const char *pamPath = env->GetStringUTFChars(pamPath_, 0);
    pcmFile = fopen(pamPath, "r");
    if(pcmFile == NULL)
    {
        LOGE("%s", "fopen file error");
        return;
    }
    pcm_buffer = (uint8_t *) malloc(4096 * 2 * 2);
    sd_buffer = (SAMPLETYPE *) malloc(4096 * 2 * 2);
    SLresult result;

    soundTouch = new SoundTouch();
    soundTouch->setSampleRate(44100); // 设置采样率
    soundTouch->setChannels(2); // 设置通道数
    soundTouch->setTempo(1.5);
    // TODO
    //第一步，创建引擎
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);

    //第二步，创建混音器
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mids, mreq);
    (void)result;
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    (void)result;
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void)result;
    }
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};


    // 第三步，配置PCM格式信息
    SLDataLocator_AndroidSimpleBufferQueue android_queue={SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};
    SLDataFormat_PCM pcm={
            SL_DATAFORMAT_PCM,//播放pcm格式的数据
            2,//2个声道（立体声）
            SL_SAMPLINGRATE_44_1,//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    SLDataSource slDataSource = {&android_queue, &pcm};


    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &pcmPlayerObject, &slDataSource, &audioSnk, 3, ids, req);
    //初始化播放器
    (*pcmPlayerObject)->Realize(pcmPlayerObject, SL_BOOLEAN_FALSE);

//    得到接口后调用  获取Player接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_PLAY, &pcmPlayerPlay);

//    注册回调缓冲区 获取缓冲队列接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_BUFFERQUEUE, &pcmBufferQueue);
    //缓冲接口回调
    (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue, pcmBufferCallBack, NULL);
//    获取音量接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_VOLUME, &pcmPlayerVolume);

//    获取播放状态接口
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay, SL_PLAYSTATE_PLAYING);

//    主动调用回调函数开始工作
    pcmBufferCallBack(pcmBufferQueue, NULL);

    env->ReleaseStringUTFChars(pamPath_, pamPath);
}