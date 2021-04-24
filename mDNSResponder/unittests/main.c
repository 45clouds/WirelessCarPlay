/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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


#include "unittest.h"
#include "DNSMessageTest.h"

const char *HWVersionString  = "unittestMac1,1";
const char *OSVersionString  = "unittest 1.1.1 (1A111)";
const char *BinaryNameString = "unittest";
const char *VersionString    = "unittest mDNSResponer-00 (Jan  1 1970 00:00:00)";

int run_tests(void);

UNITTEST_HEADER(run_tests)
UNITTEST_GROUP(DNSMessageTest)
UNITTEST_FOOTER

UNITTEST_MAIN
