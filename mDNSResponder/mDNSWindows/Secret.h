/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2013 Apple Computer, Inc. All rights reserved.
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

#ifndef _Secret_h
#define _Secret_h

#include "mDNSEmbeddedAPI.h"


#if defined(__cplusplus )
extern "C" {
#endif


extern mDNSBool
LsaGetSecret( const char * inDomain, char * outDomain, unsigned outDomainSize, char * outKey, unsigned outKeySize, char * outSecret, unsigned outSecretSize );


extern mDNSBool
LsaSetSecret( const char * inDomain, const char * inKey, const char * inSecret );


#if defined(__cplusplus)
}
#endif


#endif
