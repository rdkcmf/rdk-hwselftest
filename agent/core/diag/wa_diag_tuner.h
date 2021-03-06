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
 * @brief Diagnostic functions for tuner - interface
 */

/** @addtogroup WA_DIAG_TUNER
 *  @{
 */

#ifndef WA_DIAG_TUNER_H
#define WA_DIAG_TUNER_H

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * EXPORTED DEFINITIONS
 *****************************************************************************/
#define BUFFER_LEN       256
#define NUM_DECODERS     2
#define NUM_HVD_DATA     4
#define NUM_BYTES        16
#define NUM_PARSER_BANDS 6
#define NUM_PARSER_DATA  3
#define NUM_PID_CHANNELS 128
#define NUM_PID_DATA     2

/*****************************************************************************
 * EXPORTED TYPES
 *****************************************************************************/
typedef struct VideoDecoder_tag
{
    bool started;
    char tsm_data[NUM_HVD_DATA][NUM_BYTES];     /* enabled_pts, pts_stc_diff, pts_offset, errors */
    int  decode_data[NUM_HVD_DATA];             /* decoded, drops, errors, overflows */
    int  display_data[NUM_HVD_DATA];            /* displayed, drops, errors, underflows */
} VideoDecoder_t;

typedef struct ParserBand_tag
{
    int parser_band[NUM_PARSER_DATA]; /* cc errors, tei errors, length errors */
} ParserBand_t;

typedef struct PIDChannel_tag
{
    char pid_channel[NUM_BYTES];  /* 32 bit channel number */
    int  cc_errors;               /* cc errors */
} PIDChannel_t;

typedef struct Frontend_tag
{
    int  frontend;
    char lock_status[NUM_BYTES];
    char ber[NUM_BYTES];
    char snr[NUM_BYTES];
    char corrected[NUM_BYTES];
    char uncorrected[NUM_BYTES];
} Frontend_t;

typedef struct WA_DIAG_TUNER_TunerStatus_tag
{
    bool used;
    bool locked;
} WA_DIAG_TUNER_TunerStatus_t;

typedef struct WA_DIAG_TUNER_DocsisParams_tag
{
    int  DOCSIS_BootState;
    char *DOCSIS_DwStreamChPwr;
    char *DOCSIS_UpStreamChPwr;
    char *DOCSIS_SNR;
} WA_DIAG_TUNER_DocsisParams_t;

typedef struct WA_DIAG_TUNER_QamParams_tag
{
    char *numLocked;
    char *QAM_ChPwr;
    char *QAM_SNR;
} WA_DIAG_TUNER_QamParams_t;

/*****************************************************************************
 * EXPORTED VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * EXPORTED FUNCTIONS
 *****************************************************************************/
extern int WA_DIAG_TUNER_status(void* instanceHandle, void *initHandle, json_t **params);

bool WA_DIAG_TUNER_GetVideoDecoderData(VideoDecoder_t* hvd, int* num_hvd);
bool WA_DIAG_TUNER_GetTransportData(ParserBand_t* parserBand, int* num_parser, PIDChannel_t* pidChannel, int* num_pid);
bool WA_DIAG_TUNER_GetTunerStatuses(WA_DIAG_TUNER_TunerStatus_t * statuses, size_t statusCount, int * pNumLocked, bool* freqLocked, char* freq, Frontend_t* frontendStatus);
bool WA_DIAG_TUNER_GetDocsisParams(WA_DIAG_TUNER_DocsisParams_t * params);
bool WA_DIAG_TUNER_GetQamParams(WA_DIAG_TUNER_QamParams_t * params);

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif
