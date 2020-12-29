#!/bin/sh
# ============================================================================
# RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
# ============================================================================
# This file (and its contents) are the intellectual property of RDK Management, LLC.
# It may not be used, copied, distributed or otherwise  disclosed in whole or in
# part without the express written permission of RDK Management, LLC.
# ============================================================================
# Copyright (c) 2016 RDK Management, LLC. All rights reserved.
# ============================================================================

FILE=/tmp/.hwselftest_dfltroute

# Input arguments Format: family interface destinationip gatewayip preferred_src metric add/delete
cmd=$7
gtwip=$4

if [ "$cmd" = "add" ]; then
    if ! grep -q "$gtwip" $FILE; then
        echo "$gtwip" >> $FILE
    fi
elif [ "$cmd" = "delete" ]; then
    sed -i "/$gtwip/d" $FILE
fi
