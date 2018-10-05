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
 * @file wa_json.c
 *
 * @brief This file contains functions for generation of unique id.
 */

/** @addtogroup WA_UTILS_JSON
 *  @{
 */

/*****************************************************************************
 * STANDARD INCLUDE FILES
 *****************************************************************************/
#include <string.h>

/*****************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 *****************************************************************************/
#include "wa_debug.h"
#include "wa_json.h"
#include "wa_osa.h"

/*****************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL DEFINITIONS
 *****************************************************************************/

/*****************************************************************************
 * LOCAL TYPES
 *****************************************************************************/

/*****************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 *****************************************************************************/

static bool vNestedSet(json_t * object, json_t * value, const char * firstKey, va_list vl);

/*****************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 *****************************************************************************/

/*****************************************************************************
 * FUNCTION DEFINITIONS
 *****************************************************************************/
int WA_UTILS_JSON_RpcValidate(json_t **pJson)
{
    int status = 1;
    json_t *jId, *jout, *jp;
    char *s;

    WA_ENTER("WA_UTILS_JSON_RpcValidate(json=%p[%p])\n", pJson, pJson?*pJson:NULL);

    if(pJson == NULL)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidate(): invalid param\n");
        status = -1;
        goto end;
    }

    if(*pJson == NULL)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidate(): invalid json\n");
        jout = json_pack("{s:s,s:{s:i,s:s},s:n}", "jsonrpc", "2.0", "error", "code", -32600, "message", "Invalid Request", "id");
        goto end;
    }

    /* check for "id" */
    status = json_unpack(*pJson, "{s:o}", "id", &jId); //reference to jId is NOT modified
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidate(): json_unpack() id error\n");
        jout = json_pack("{s:s,s:{s:i,s:s},s:n}", "jsonrpc", "2.0", "error", "code", -32600, "message", "Invalid Request", "id");
        goto json_out;
    }

    /* check for "jsonrpc 2.0" */
    status = json_unpack(*pJson, "{ss}", "jsonrpc", &s); //s will be released when releasing json object
    if((status != 0) || strcmp(s, "2.0"))
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidate(): json_unpack() jsonrpc 2.0 error\n");

        /* jId reference will be stolen (=json_decref(jId)) here */
        jout = json_pack("{s:s,s:{s:i,s:s},s:O}", "jsonrpc", "2.0", "error", "code", -32602, "message", "Invalid params", "id", jId);
        goto json_out;
    }

    /* check for "method" */
    status = json_unpack(*pJson, "{ss}", "method", &s); //s will be released when releasing json object
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidate(): json_unpack() method undefined\n");

        /* jId reference will be stolen (=json_decref(jId)) here */
        jout = json_pack("{s:s,s:{s:i,s:s},s:O}", "jsonrpc", "2.0", "error", "code", -32602, "message", "Invalid params", "id", jId);
        goto json_out;
    }

    /* check for "params" */
    status = json_unpack(*pJson, "{so}", "params", &jp); //jp will be released when releasing json object
    /* "params" if exist must be array or object */
    if((status == 0) && !(json_is_object(jp) || json_is_array(jp)))
    {
        /* jId reference will be stolen (=json_decref(jId)) here */
        jout = json_pack("{s:s,s:{s:i,s:s},s:O}", "jsonrpc", "2.0", "error", "code", -32602, "message", "Invalid params", "id", jId);
        goto json_out;
    }
    status = 0;
    goto end;

    json_out:
    json_decref(*pJson);
    *pJson = jout;
    status = *pJson ? 1 : -1;

    end:
    WA_RETURN("WA_UTILS_JSON_RpcValidate(): %d json=%p\n", status, pJson?*pJson:NULL);
    return status;
}


int WA_UTILS_JSON_RpcValidateTxt(const char *const msg, size_t len, json_t **pJson)
{
    int status = 1;
    json_error_t jerror;

    WA_ENTER("WA_UTILS_JSON_RpcValidateTxt(msg=%p[%.*s] len=%zu json=%p[%p])\n", msg, msg?(int)len:0, msg?msg:"", len, pJson, pJson?*pJson:NULL);

    if(pJson == NULL)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidateTxt(): invalid param\n");
        status = -1;
        goto end;
    }

    /* general parsing check */
    *pJson = json_loadb(msg, len, JSON_DISABLE_EOF_CHECK, &jerror);
    if(!*pJson)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidateTxt(): json_loads() error\n");
        *pJson = json_pack("{s:s,s:{s:i,s:s},s:n}", "jsonrpc", "2.0", "error", "code", -32700, "message", "Parse error", "id");
        if(*pJson == NULL)
            status = -1;
        goto end;
    }

    status = WA_UTILS_JSON_RpcValidate(pJson);
    if(status != 0)
    {
        WA_ERROR("WA_UTILS_JSON_RpcValidateTxt(): WA_UTILS_JSON_RpcValidate() error\n");
    }

    end:
    WA_RETURN("WA_UTILS_JSON_RpcValidateTxt(): %d *json=%p\n", status, *pJson);
    return status;
}

int WA_UTILS_JSON_Init(void)
{
    json_set_alloc_funcs(WA_OSA_Malloc, WA_OSA_Free);
    return 0;
}


bool WA_UTILS_JSON_NestedSet(json_t * object, json_t * value, const char * firstKey, ...)
{
    bool result = false;
    va_list vl;

    va_start(vl, firstKey);
    result = vNestedSet(object, value, firstKey, vl);
    va_end(vl);

    return result;
}

extern bool WA_UTILS_JSON_NestedSetNew(json_t * object, json_t * value, const char * firstKey, ...)
{
    bool result = false;
    va_list vl;

    va_start(vl, firstKey);
    result = vNestedSet(object, value, firstKey, vl);
    va_end(vl);

    json_decref(value);

    return result;
}

/*****************************************************************************
 * LOCAL FUNCTIONS
 *****************************************************************************/

static bool vNestedSet(json_t * object, json_t * value, const char * firstKey, va_list vl)
{
    const char * key = NULL;
    const char * lastKey = NULL;
    json_t * child = NULL;

    lastKey = firstKey;
    while ((key = va_arg(vl, const char *)) && key[0])
    {
        child = json_object_get(object, lastKey);
        if (child && !json_is_object(child))
        {
            return false;
        }
        if (!child)
        {
            child = json_object();
            json_object_set_new(object, lastKey, child);
        }
        object = child;
        lastKey = key;
    }

    return json_object_set(object, lastKey, value) == 0;
}

/* End of doxygen group */
/*! @} */

/* EOF */
