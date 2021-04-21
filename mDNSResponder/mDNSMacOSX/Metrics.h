/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2016 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mDNSEmbeddedAPI.h"

#ifndef __Metrics_h
#define __Metrics_h

#ifdef  __cplusplus
extern "C" {
#endif

#if TARGET_OS_EMBEDDED
mStatus MetricsInit(void);
void    MetricsUpdateUDNSQueryStats(const domainname *inQueryName, mDNSu16 inType, const ResourceRecord *inRR, mDNSu32 inSendCount, mDNSu32 inLatencyMs, mDNSBool inForCell);
void    MetricsUpdateUDNSResolveStats(const domainname *inQueryName, const ResourceRecord *inRR, mDNSBool inForCell);
void    LogMetrics(void);
#endif

#ifdef  __cplusplus
}
#endif

#endif // __Metrics_h
