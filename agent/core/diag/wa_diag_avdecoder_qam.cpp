/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file
 *
 * @brief Diagnostic functions for AV decoding - implementation
 */

/** @addtogroup WA_DIAG_AVDECODER_QAM
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

#include <string>
#include <memory>
#include <typeinfo>
#include <unistd.h>
#include <sys/time.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/

#include "wa_diag.h"
#include "wa_debug.h"
#include "wa_fileops.h"

/* rdk specific */
#include <glib.h>
#include <string.h>

#include "rmfcore.h"
#include "hnsource.h"
#include "mediaplayersink.h"
#include "rmf_osal_init.h"

#include "wa_sicache.h"
#include "wa_iarm.h"
#include "wa_mgr.h"
#include "wa_vport.h"
#include "wa_rmf.h"

/* module interface */
#include "wa_diag_avdecoder_qam.h"

#if WA_DEBUG
#include "wa_diag_tuner.h"
#define DEBUG_TUNERS 6
#endif

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/
extern int WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(void);
extern int WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(void);

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/
#define USE_WORKAROUND_RETRY_TUNING 1
#define TRACE(x) if (1) {int rc_657821894756 = x; printf("RC: %i " #x "\n", rc_657821894756); } else (void)0

#ifndef USE_WORKAROUND_RETRY_TUNING
#define NUM_SI_ENTRIES 5
#else
#define NUM_SI_ENTRIES 3 /* Must limit used entries due to the total time constraint */
#define RETRIES_NUM 3 /* tests show that at the 3rd time the lock is acquired eventually */
#endif

#define VIDEO_DECODER_STATUS_FILE "/proc/video_status"
#define AUDIO_DECODER_STATUS_FILE "/proc/audio_status"
#define VIDEO_DECODER_STATUS_BRCM_FILE "/proc/brcm/video_decoder"
#define AUDIO_DECODER_STATUS_BRCM_FILE "/proc/brcm/audio"

#define DECODER_CHECK_TOTAL_STEPS 10
#define DECODER_CHECK_STEP_TIME 1 /* in [s] */

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/
class Handler : public IRMFMediaEvents
{
    private:
        std::string id;
        bool * pError;
        void * condHandle;

    public:
        Handler(const std::string & id, bool * pError, void * condHandle)
            : id(id)
            , pError(pError)
            , condHandle(condHandle)
        {
        }

        virtual void playing()
        {
            WA_DBG("Handler[%s].playing\n", id.c_str());
        }
        virtual void paused()
        {
            WA_DBG("Handler[%s].paused\n", id.c_str());
        }
        virtual void stopped()
        {
            WA_DBG("Handler[%s].stopped\n", id.c_str());
        }
        virtual void complete()
        {
            WA_DBG("Handler[%s].complete\n", id.c_str());
        }
        virtual void progress(int position, int duration)
        {
            WA_DBG("Handler[%s].progress: %i %i\n", id.c_str(), position, duration);
        }
        virtual void status(const RMFStreamingStatus& status)
        {
            WA_DBG("Handler[%s].status: %i\n", id.c_str(), (int)status.m_status);
        }
        virtual void error(RMFResult err, const char* msg)
        {
            /* Errors:
             * RMF_RESULT_HNSRC_GENERIC_ERROR       = 0x1001;
             * RMF_RESULT_HNSRC_FORMAT_ERROR        = 0x1002;
             * RMF_RESULT_HNSRC_LEGACY_CA_ERROR     = 0x1003;
             * RMF_RESULT_HNSRC_NETWORK_ERROR       = 0x1004;
             * RMFResult RMF_RESULT_HNSRC_VOD_PAUSE_TIMEOUT   = 0x1005;
             * RMF_RESULT_HNSRC_TRAILER_ERROR       = 0x1006;
             * RMF_RESULT_HNSRC_DTCP_CONN_ERROR     = 0x1007;
             */
            int rc = 0;
            WA_DBG("Handler[%s].error: %i %s\n", id.c_str(), (int)err, msg);

            rc = WA_OSA_CondLock(condHandle);
            if (rc)
            {
                WA_ERROR("Handler[%s].error(): WA_OSA_CondLock returned %i\n", id.c_str(), rc);
            }

            *pError = true;
            rc = WA_OSA_CondSignal(condHandle);
            if (rc)
            {
                WA_ERROR("Handler[%s].error(): WA_OSA_CondSignal returned %i\n", id.c_str(), rc);
            }

            rc = WA_OSA_CondUnlock(condHandle);
            if (rc)
            {
                WA_ERROR("Handler[%s].error(): WA_OSA_CondUnlock returned %i\n", id.c_str(), rc);
            }
        }
};

struct VideoPosition
{
    int x;
    int y;
    int w;
    int h;
};

struct TuneParameters
{
    unsigned int frequency;
    unsigned int modulation;
    unsigned int programNumber;
};

template<class T>
class RMFManager
{
    public:
        RMFManager()
            : t(new T())
            , initialized(false)
        {
        }

        virtual ~RMFManager()
        {
            term();
        }

        virtual bool init()
        {
            if (!initialized)
            {
                RMFResult rr = t->init();
                WA_DBG("RMFManager.init(): %p->init() = %i\n", (void*)getObject(), (int)rr);
                if (rr != RMF_RESULT_SUCCESS)
                {
                    WA_ERROR("RMFManager.init(): Failed to initialize %s, result=%i\n", typeid(T).name(), (int)rr);
                }
                else
                {
                    initialized = true;
                }
            }
            return initialized;
        }

        virtual bool term()
        {
            if (initialized)
            {
                RMFResult rr = t->term();
                WA_DBG("RMFManager.term(): %p->term() = %i\n", (void*)getObject(), (int)rr);
                if (rr != RMF_RESULT_SUCCESS)
                {
                    WA_ERROR("RMFManager.term(): Failed to terminate %s, result=%i\n", typeid(T).name(), (int)rr);
                }
                else
                {
                    initialized = false;
                }
            }
            return !initialized;
        }

        virtual T * getObject() const
        {
            return t.get();
        }

    private:
        bool initialized;
        std::auto_ptr<T> t;
};

template<class T>
class SourceManager : public RMFManager<T>
{
    public:
        SourceManager()
            : opened(false)
            , played(false)
        {
        }

        ~SourceManager()
        {
            close();
        }

        bool open(const char* uri, char* mimetype)
        {
            if (!opened)
            {
                RMFResult rr = this->getObject()->open(uri, mimetype);
                WA_DBG("SourceManager.open(): %p->open(%s, %s) = %i\n", (void*)this->getObject(), (char*)uri, (char*)mimetype, (int)rr);
                if (rr != RMF_RESULT_SUCCESS)
                {
                    WA_ERROR("SourceManager.open(): Failed to open source, result=%i\n", (int)rr);
                }
                else
                {
                    opened = true;
                }
            }
            return opened;
        }

        bool close()
        {
            if (opened)
            {
                RMFResult rr = this->getObject()->close();
                WA_DBG("SourceManager.close(): %p->close() = %i\n", (void*)this->getObject(), (int)rr);
                if (rr != RMF_RESULT_SUCCESS)
                {
                    WA_ERROR("SourceManager.close(): Failed to close source, result=%i\n", (int)rr);
                }
                else
                {
                    opened = false;
                }
            }
            return !opened;
        }

        bool play()
        {
            if (!played)
            {
                RMFResult rr = this->getObject()->play();
                WA_DBG("SourceManager.play(): %p->play() = %i\n", (void*)this->getObject(), (int)rr);
                if (rr != RMF_RESULT_SUCCESS)
                {
                    WA_ERROR("SourceManager.play(): Failed to play source, result=%i\n", (int)rr);
                }
                else
                {
                    played = true;
                }
            }
            return played;
        }

        bool stop()
        {
            if (opened)
            {
                RMFResult rr = this->getObject()->stop();
                WA_DBG("SourceManager.stop(): %p->stop() = %i\n", (void*)this->getObject(), (int)rr);
                if (rr != RMF_RESULT_SUCCESS)
                {
                    WA_ERROR("SourceManager.stop(): Failed to stop source, result=%i\n", (int)rr);
                }
                else
                {
                    opened = false;
                }
            }
            return !opened;
        }

    private:
        bool opened;
        bool played;
};

class TestException: public std::exception
{
    public:
        TestException(int result, const char * text)
            : result(result)
            , text(text)
        {
        }
        ~TestException() throw()
        {
        }
        int getResult() const throw()
        {
            return result;
        }
        const char * what() const throw ()
        {
            return text.c_str();
        }
    private:
        int result;
        std::string text;
};

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static void * gmlTaskFunc(void * arg);
static void stopQam();
static int parseAvPlayParameters(json_t * parameters, json_t * config, VideoPosition * pVideoPosition, TuneParameters * pTuneParameters);
static bool parseTuneParameters(json_t * json, TuneParameters * pTuneParameters);
static int getInteger(json_t * json, const char * key, bool * pOk);
static bool useSi = false;

/*****************************************************************************
 * INLINE FUNCTIONS
 *****************************************************************************/

static inline void scale(int * value, int original, int target)
{
    *value = *value * target / original;
}

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

static std::auto_ptr<SourceManager<HNSource> > source;
static std::auto_ptr<RMFManager<MediaPlayerSink> > sink;
static std::auto_ptr<Handler> sourceHandler;
static std::auto_ptr<Handler> sinkHandler;

static bool rmfOsalInitialized;
static GMainLoop * gml;
static void * gmlThread;
static bool playError;
static void * playErrorCond;
static bool avUnderflow, videoPES, videoFrame, audioPES, audioFrame;

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
void SinkCbSignal(void * condHandle)
{
    int rc;

    rc = WA_OSA_CondLock(condHandle);
    if (rc)
    {
    WA_ERROR("SinkCbSignal(): WA_OSA_CondLock returned %i\n", rc);
    }

    rc = WA_OSA_CondSignal(condHandle);
    if (rc)
    {
    WA_ERROR("SinkCbSignal(): WA_OSA_CondSignal returned %i\n", rc);
    }

    rc = WA_OSA_CondUnlock(condHandle);
    if (rc)
    {
    WA_ERROR("SinkCbSignal(): WA_OSA_CondUnlock returned %i\n", rc);
    }
}

void SinkCbVideoPES(void *pCond)
{
    WA_DBG("SinkCbVideoPES()\n");
    videoPES = true;
    SinkCbSignal(pCond);
}

void SinkCbVideoFrame(void *pCond)
{
    WA_DBG("SinkCbVideoFrame()\n");
    videoFrame = true;
}

void SinkCbAudioPES(void *pCond)
{
    WA_DBG("SinkCbAudioPES()\n");
    audioPES = true;
    SinkCbSignal(pCond);
}

void SinkCbAudioFrame(void *pCond)
{
    WA_DBG("SinkCbAudioFrame()\n");
    audioFrame = true;
}

void SinkCbAVUnderflow(void *pCond)
{
    WA_DBG("SinkCbAVUnderflow()\n");
    avUnderflow = true;
}

static void SigQuitHandler(void)
{
    WA_DBG("SigQuitHandler()\n");
    SinkCbSignal(playErrorCond);
}

void * WA_DIAG_AVDECODER_QAM_init(struct WA_DIAG_proceduresConfig_t * diag)
{
    /* Consideration: Currently this function is used only for the purpose
     *                of this diag only. For multiple usege it must be modified.
     */
    if (!rmfOsalInitialized)
    {
        rmf_Error re = rmf_osal_init("/etc/rmfconfig.ini", "/etc/debug.ini");
        if (re != RMF_SUCCESS)
        {
            WA_ERROR("WA_DIAG_AVDECODER_QAM_init(): Failed to initialize RMF OSAL\n");
        }
        else
        {
            rmfOsalInitialized = true;
        }
    }

    return diag;
}

int WA_DIAG_AVDECODER_QAM_status(void* instanceHandle, void *initHandle, json_t ** pJsonInOut)
{
    /* Consideration: Currently the rmf streamer address/port is hardcoded. */
    static const char * videoUrlFormat = "http://%s:%d/vldms/tuner?deviceId=P0116419060&DTCP1HOST=127.0.0.1&DTCP1PORT=5000&ocap_locator=ocap://tune://frequency=%i_modulation=%i__pgmno=%i_%u%u";
#ifdef USE_WORKAROUND_RETRY_TUNING
    int retries = 0;
#endif
    static char * videoContentType = const_cast<char*>("");

    int result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
    VideoPosition videoPosition;
    TuneParameters tuneParameters[NUM_SI_ENTRIES];
    char * videoUrl = NULL;
    json_t * config = NULL;
    int videoDecoderStatus, audioDecoderStatus;
    int paramSet;
    unsigned int numberOfTuners = 0;
    int parametersNum;
    struct timeval timeNow;
#if WA_DEBUG
    WA_DIAG_TUNER_TunerStatus_t tunerStatus[DEBUG_TUNERS];
    int numLocked;
#endif

    config = ((WA_DIAG_proceduresConfig_t*)initHandle)->config;

    parametersNum = parseAvPlayParameters(*pJsonInOut, config, &videoPosition, tuneParameters);
    if (parametersNum <= 0)
    {
        *pJsonInOut = json_string("Missing SICache.");
        result = WA_DIAG_ERRCODE_SI_CACHE_MISSING;
        goto end;
    }

    json_decref(*pJsonInOut);
    *pJsonInOut = NULL;

    if (!WA_UTILS_RMF_GetNumberOfTuners(&numberOfTuners) || (numberOfTuners == 0))
    {
        *pJsonInOut = json_string("Not applicable.");
        result = WA_DIAG_ERRCODE_NOT_APPLICABLE;
        goto end;
    }

    videoDecoderStatus = WA_DIAG_AVDECODER_QAM_VideoDecoderStatus();
    audioDecoderStatus = WA_DIAG_AVDECODER_QAM_AudioDecoderStatus();
    if(!videoDecoderStatus && !audioDecoderStatus)
    {
        WA_INFO("WA_DIAG_AVDECODER_QAM_status(): Decoders already in use.\n");
        return WA_DIAG_ERRCODE_SUCCESS;
    }

    playErrorCond = WA_OSA_CondCreate();
    if (!playErrorCond)
    {
        *pJsonInOut = json_string("Unable to create a condition variable");
        result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        goto end;
    }

    if(WA_OSA_TaskSetSignalHandler(SigQuitHandler) != 0)
    {
        *pJsonInOut = json_string("Unable to assign for cancel.");
        result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
        goto end;
    }
    for(paramSet = 0; (paramSet < parametersNum) && (result != WA_DIAG_ERRCODE_SUCCESS); ++paramSet)
    {
#ifndef USE_WORKAROUND_RETRY_TUNING
        WA_DIAG_SendProgress(instanceHandle, (paramSet*DECODER_CHECK_TOTAL_STEPS*100)/(DECODER_CHECK_TOTAL_STEPS * parametersNum));
#else
        for(retries = 0; (retries < RETRIES_NUM) && (result != WA_DIAG_ERRCODE_SUCCESS); ++retries)
        {
        WA_DIAG_SendProgress(instanceHandle, (((paramSet*RETRIES_NUM)+retries)*DECODER_CHECK_TOTAL_STEPS*100)/
                                             (DECODER_CHECK_TOTAL_STEPS * parametersNum * RETRIES_NUM));
        WA_DBG("WA_DIAG_AVDECODER_QAM_status(): try: %d\n", retries);
#endif

        json_decref(*pJsonInOut);
        *pJsonInOut = NULL;

        gettimeofday(&timeNow, NULL);
        if (asprintf(&videoUrl, videoUrlFormat,
            WA_UTILS_RMF_MEDIASTREAMER_IP,
            WA_UTILS_RMF_GetMediastreamerPort(),
            (int)tuneParameters[paramSet].frequency,
            (int)tuneParameters[paramSet].modulation,
            (int)tuneParameters[paramSet].programNumber,
            (unsigned int)timeNow.tv_sec, (unsigned int)timeNow.tv_usec) == -1)
        {
            *pJsonInOut = json_string("Unable to prepare video URL");
            result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
            continue;
        }

        gml = g_main_loop_new(NULL, FALSE);
        gmlThread = WA_OSA_TaskCreate(NULL, 0, gmlTaskFunc, gml, WA_OSA_SCHED_POLICY_NORMAL, WA_OSA_TASK_PRIORITY_MAX);
        if (!gmlThread)
        {
            *pJsonInOut = json_string("Unable to start a thread");
            result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
            goto end;
        }

        try
        {
            playError = false;
            avUnderflow = false;
            videoPES = false;
            videoFrame = false;
            audioPES = false;
            audioFrame = false;

            sourceHandler.reset(new Handler("source", &playError, playErrorCond));
            sinkHandler.reset(new Handler("sink", &playError, playErrorCond));

            WA_DBG("WA_DIAG_AVDECODER_QAM_status(): About to create source and sink");

            source.reset(new SourceManager<HNSource>());
            sink.reset(new RMFManager<MediaPlayerSink>());

            WA_DBG("WA_DIAG_AVDECODER_QAM_status(): Source and sink created");

            if (!source->init())
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to initialize HNSource");
            }
            if (!sink->init())
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to initialize MediaPlayerSink");
            }
            sink->getObject()->setMediaWarningCallback(SinkCbAVUnderflow, playErrorCond);
            sink->getObject()->setHaveVideoCallback(SinkCbVideoPES, playErrorCond);
            sink->getObject()->setVideoPlayingCallback(SinkCbVideoFrame, playErrorCond);
            sink->getObject()->setHaveAudioCallback(SinkCbAudioPES, playErrorCond);
            sink->getObject()->setAudioPlayingCallback(SinkCbAudioFrame, playErrorCond);
            if (source->getObject()->addEventHandler(sourceHandler.get()) != RMF_RESULT_SUCCESS)
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to attach event handler to HNSource");
            }
            if (sink->getObject()->addEventHandler(sinkHandler.get()) != RMF_RESULT_SUCCESS)
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to attach event handler to MediaPlayerSink");
            }
#if 0
            if ((videoPosition.w > 0) && (videoPosition.h > 0))
            {
                WA_DBG("WA_DIAG_AVDECODER_QAM_status(): Setting video position: %i %i %i %i\n", videoPosition.x, videoPosition.y, videoPosition.w, videoPosition.h);
                sink->getObject()->setVideoRectangle(videoPosition.x, videoPosition.y, videoPosition.w, videoPosition.h);
                // In this test we don't really care if video rectangle can be set properly
            }
            else
            {
                WA_DBG("WA_DIAG_AVDECODER_QAM_status(): Not setting video position\n");
            }
#else
            /* As for recent agreement, no A/V should go out */
            sink->getObject()->setMuted(true);
#endif
            if (sink->getObject()->setSource(source->getObject()) != RMF_RESULT_SUCCESS)
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to connect HNSource to MediaPlayerSink");
            }

            WA_DBG("WA_DIAG_AVDECODER_QAM_status(): videoUrl: %s, videoContentType: %s\n", (char*)videoUrl, (char*)videoContentType);

            if (!source->open(videoUrl, videoContentType))
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to open source");
            }
            if (!source->play())
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to play");
            }

            // Since apparently no "success" event is fired when the playback starts,
            // lets just wait for a moment and treat no "error" event as a success.

            if (WA_OSA_CondLock(playErrorCond) != 0)
            {
                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Unable to lock condition variable");
            }

            if(!WA_OSA_TaskCheckQuit())
            {
                WA_OSA_CondTimedWait(playErrorCond, 15000);
            }

            WA_OSA_CondUnlock(playErrorCond);

            if(WA_OSA_TaskCheckQuit())
            {
                WA_DBG("WA_DIAG_AVDECODER_QAM_status(): decoder start: test cancelled\n");
                throw TestException(WA_DIAG_ERRCODE_CANCELLED, "Test cancelled.");
            }

            if (playError) // playError - actually it stands for error BEFORE the decoder
            {
                if(useSi)
                {
                    WA_UTILS_SICACHE_TuningSetLuckyId(-1);
                }

                WA_DBG("WA_DIAG_AVDECODER_QAM_status(): Playback error - test failure\n");
                throw TestException(WA_DIAG_ERRCODE_AV_NO_SIGNAL, "No stream data.");
            }
            if(useSi)
            {
                WA_UTILS_SICACHE_TuningSetLuckyId(paramSet);
            }

            videoDecoderStatus = -1;
            audioDecoderStatus = -1;
            /* The status is not ready immediately */
            for(int step = 1; step <= DECODER_CHECK_TOTAL_STEPS; ++step)
            {
#ifndef USE_WORKAROUND_RETRY_TUNING
                WA_DIAG_SendProgress(instanceHandle, ((paramSet*DECODER_CHECK_TOTAL_STEPS + step)*100)/(DECODER_CHECK_TOTAL_STEPS * parametersNum));
#else
                WA_DIAG_SendProgress(instanceHandle, ((((paramSet*RETRIES_NUM)+retries)*DECODER_CHECK_TOTAL_STEPS + step)*100)/
                                                     (DECODER_CHECK_TOTAL_STEPS * parametersNum*RETRIES_NUM));
#endif
                if(videoDecoderStatus)
                {
                    videoDecoderStatus = WA_DIAG_AVDECODER_QAM_VideoDecoderStatus();
                }
                if(audioDecoderStatus)
                {
                        audioDecoderStatus = WA_DIAG_AVDECODER_QAM_AudioDecoderStatus();
                }
                if(!videoDecoderStatus && !audioDecoderStatus)
                {
                    break;
                }
                sleep(DECODER_CHECK_STEP_TIME);
                if(WA_OSA_TaskCheckQuit())
                {
                    WA_DBG("WA_DIAG_AVDECODER_QAM_status(): decoder check: test cancelled\n");
                    throw TestException(WA_DIAG_ERRCODE_CANCELLED, "Test cancelled.");
                }
            }
            if((videoDecoderStatus > 0) || (audioDecoderStatus > 0))
            {
                throw TestException(WA_DIAG_ERRCODE_FAILURE, "Decoder error.");
            }
            if((videoDecoderStatus < 0) || (audioDecoderStatus < 0))
            {
                if(WA_OSA_TaskCheckQuit())
                    throw TestException(WA_DIAG_ERRCODE_CANCELLED, "Test cancelled.");

                throw TestException(WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR, "Decoder error.");
            }
            result = WA_DIAG_ERRCODE_SUCCESS;
        }
        catch (const TestException & e)
        {
            *pJsonInOut = json_string(e.what());
            result = e.getResult();
            WA_DBG("WA_DIAG_AVDECODER_QAM_status(): catch\n");
        }
        catch (...)
        {
            *pJsonInOut = json_string("Unknown exception occurred");
            result = WA_DIAG_ERRCODE_INTERNAL_TEST_ERROR;
            WA_DBG("WA_DIAG_AVDECODER_QAM_status(): unknown catch\n");
        }
        if (videoUrl)
        {
            free(videoUrl);
        }
        stopQam(); // currently no more need to play
        WA_DBG("WA_DIAG_AVDECODER_QAM_status(): step:%d result:%d,%s\n", paramSet, result,
               *pJsonInOut == NULL ? "null" : json_string_value(*pJsonInOut));

#if WA_DEBUG
        memset(tunerStatus, 0, sizeof(tunerStatus));
        WA_DIAG_TUNER_TunerStatus_t tunerStatus[DEBUG_TUNERS];
        WA_DIAG_TUNER_GetTunerStatuses(tunerStatus, DEBUG_TUNERS, &numLocked);
#endif
    }
#ifdef USE_WORKAROUND_RETRY_TUNING
    }
#endif
    WA_OSA_TaskSetSignalHandler(NULL);

end:
    if (playErrorCond)
    {
        WA_OSA_CondDestroy(playErrorCond);
        playErrorCond = NULL;
    }

    return result;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static void * gmlTaskFunc(void * arg)
{
    GMainLoop * gml = (GMainLoop*) arg;

    g_main_loop_run(gml);

    return NULL;
}

static void stopQam()
{
    if (source.get())
    {
        TRACE(source->stop());
        TRACE(source->close());

        if (sink.get())
        {
            TRACE(sink->term());
        }

        TRACE(source->term());
    }

    sink.reset();
    source.get();

    sourceHandler.reset();
    sinkHandler.reset();

    if (gml)
    {
        g_main_loop_quit(gml);
    }

    if (gmlThread)
    {
        WA_OSA_TaskJoin(gmlThread, NULL);
        WA_OSA_TaskDestroy(gmlThread);
        gmlThread = NULL;
    }

    if (gml)
    {
        g_main_loop_unref(gml);
        gml = NULL;
    }
}


static int parseAvPlayParameters(json_t * parameters, json_t * config, VideoPosition * pVideoPosition, TuneParameters * pTuneParameters)
{
    /* Consideration: Currently the actual screen dimensions (units suitable for MediaPlayerSink) are hardcoded. */
    static const int screen_width = 1280;
    static const int screen_height = 720;
    int i, numEntries;
    useSi = false;

    memset(pTuneParameters, 0, sizeof(TuneParameters)*NUM_SI_ENTRIES);

    numEntries = (int)parseTuneParameters(parameters, pTuneParameters);
    if (numEntries == 0)
    {
        WA_UTILS_SICACHE_Entries_t *pEntries=NULL;
        numEntries = WA_UTILS_SICACHE_TuningRead(NUM_SI_ENTRIES, &pEntries);
        if(numEntries <= 0)
        {
            return numEntries;
        }

        useSi = true;

        for(i=0; i<numEntries; ++i)
        {
            pTuneParameters[i].frequency = pEntries[i].freq;
            pTuneParameters[i].modulation = pEntries[i].mod;
            pTuneParameters[i].programNumber = pEntries[i].prog;
        }
        free(pEntries);
    }

    bool ok = false;
    json_t * window = json_object_get(parameters, "window");

    if (json_is_array(window) && (json_array_size(window) >= 4))
    {
        ok = true;
        for (int i = 0; i < 4; ++i)
        {
            if (!json_is_integer(json_array_get(window, i)))
            {
                ok = false;
                break;
            }
        }
    }

    if (!ok)
    {
        pVideoPosition->x = 0;
        pVideoPosition->y = 0;
        pVideoPosition->w = screen_width;
        pVideoPosition->h = screen_height;
        return numEntries;
    }

    pVideoPosition->x = json_integer_value(json_array_get(window, 0));
    pVideoPosition->y = json_integer_value(json_array_get(window, 1));
    pVideoPosition->w = json_integer_value(json_array_get(window, 2));
    pVideoPosition->h = json_integer_value(json_array_get(window, 3));

    if ((json_array_size(window) >= 6) && json_is_integer(json_array_get(window, 4)) && json_is_integer(json_array_get(window, 5)))
    {
        int req_width = json_integer_value(json_array_get(window, 4));
        int req_height = json_integer_value(json_array_get(window, 5));

        if ((req_width != 0) && (req_height != 0))
        {
            scale(&pVideoPosition->x, req_width, screen_width);
            scale(&pVideoPosition->w, req_width, screen_width);
            scale(&pVideoPosition->y, req_height, screen_height);
            scale(&pVideoPosition->w, req_height, screen_height);
        }
    }

    return numEntries;
}

static bool parseTuneParameters(json_t * json, TuneParameters * pTuneParameters)
{
    bool ok = true;

    pTuneParameters->frequency = getInteger(json, "frequency", &ok);
    pTuneParameters->modulation = getInteger(json, "modulation", &ok);
    pTuneParameters->programNumber = getInteger(json, "program", &ok);

    return ok;
}

static int getInteger(json_t * json, const char * key, bool * pOk)
{
    json_t * o = json_object_get(json, key);

    if (!json_is_integer(o))
    {
        if (pOk)
        {
            *pOk = false;
        }
    }

    return json_integer_value(o);
}

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/

int WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(void)
{
    char *o = NULL;

    o = WA_UTILS_FILEOPS_OptionFind(VIDEO_DECODER_STATUS_FILE, "VIDEO[%*d,%*d] started:%*[ ]ye");

    if(WA_OSA_TaskCheckQuit())
    {
        if(o)
            free(o);

        WA_DBG("WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(): test cancelled\n");
        return -1;
    }

    if(o && !strcmp(o,"s"))
    {
        free(o);
        WA_DBG("WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(): 0\n");
        return 0;
    }
    o = WA_UTILS_FILEOPS_OptionFind(VIDEO_DECODER_STATUS_FILE, "VIDEO[%*d,%*d] started:");

    if(WA_OSA_TaskCheckQuit())
    {
        if(o)
            free(o);

        WA_DBG("WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(): test cancelled\n");
        return -1;
    }

    if(o)
    {
        free(o);
        WA_DBG("WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(): 1\n");
        return 1;
    }

    /* fall back to trying the /proc/brcm/ file */
    o = WA_UTILS_FILEOPS_OptionFind(VIDEO_DECODER_STATUS_BRCM_FILE, "%*[ ]started=");
    if (o)
    {
        int status = o[0] == 'y'? 0 : 1;
        WA_DBG("WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(): *%i\n", status);
        return status;
    }

    WA_DBG("WA_DIAG_AVDECODER_QAM_VideoDecoderStatus(): -1\n");
    return -1;
}

int WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(void)
{
    char *o = NULL;

    o = WA_UTILS_FILEOPS_OptionFind(AUDIO_DECODER_STATUS_FILE, "AUDIO[%*d] started:%*[ ]ye");
    if(WA_OSA_TaskCheckQuit())
    {
        if(o)
            free(o);

        WA_DBG("WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(): test cancelled\n");
        return -1;
    }

    if(o && !strcmp(o,"s"))
    {
        free(o);
        WA_DBG("WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(): 0\n");
        return 0;
    }
    o = WA_UTILS_FILEOPS_OptionFind(AUDIO_DECODER_STATUS_FILE, "AUDIO[%*d] started:");

    if(o)
    {
        free(o);
        WA_DBG("WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(): 1\n");
        return 1;
    }

    /* fall back to trying the /proc/brcm/ file */
    o = WA_UTILS_FILEOPS_OptionFind(AUDIO_DECODER_STATUS_BRCM_FILE, "%*[ ]%*s%*[ ]started=");
    if (o)
    {
        int status = o[0] == 'y'? 0 : 1;
        WA_DBG("WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(): *%i\n", status);
        return status;
    }

    WA_DBG("WA_DIAG_AVDECODER_QAM_AudioDecoderStatus(): -1\n");
    return -1;
}


/* End of doxygen group */
/*! @} */
