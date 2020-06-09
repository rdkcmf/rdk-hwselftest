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

var DIAG_ERRCODE = {
        FAILURE:                1,
        SUCCESS:                0,
        
        /* generic codes */
        NOT_APPLICABLE:         -1,
        CANCELLED:              -2,
        INTERNAL_TEST_ERROR:    -3,

        /* specific codes */
        HDD_STATUS_MISSING:     -100,
        HDMI_NO_DISPLAY:        -101,
        HDMI_NO_HDCP:           -102,
        MOCA_NO_CLIENTS:        -103,
        MOCA_DISABLED:          -104,
        SI_CACHE_MISSING:       -105,
        TUNER_NO_LOCK:          -106,
        AV_NO_SIGNAL:           -107,
        IR_NOT_DETECTED:        -108,
        CM_NO_SIGNAL:           -109,

        /* currently mapped to DIAG_ERRCODE.FAILURE in wa_diag_errcodes.h
        MCARD_CERT_INVALID:     -110, */

        TUNER_BUSY:             -111,
        RF4CE_NO_RESPONSE:      -112,
        WIFI_NO_CONNECTION:     -113,
        AV_URL_NOT_REACHABLE:   -114,

        NON_RF4CE_INPUT:        -117,

        /* Failure Specific Error Codes */
        EMMC_MAX_LIFE_EXCEED_FAILURE:   -118,
        EMMC_ZERO_LIFETIME_FAILURE:     -119,
        RF4CE_CTRLM_NO_RESPONSE:        -120,

        /* Also defined and used in wa_agg.c and hwst_diag_prev_results.cpp for initial value assignment */
        DEFAULT_RESULT_VALUE:           -200
};
