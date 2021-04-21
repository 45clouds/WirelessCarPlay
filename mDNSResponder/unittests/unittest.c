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
#include <stdlib.h>
#include "unittest.h"

int _unittest_assert_i(const int condition, const int i, const char * const conditionStr,
                       const char * const filename, const unsigned int linenum,
                       const char * const functionname, __test_item ** __i, int * const __success)
{
    if (!condition)
    {
        __test_item* tba = malloc(sizeof(__test_item));
        tba->next = *__i;
        tba->file = filename;
        tba->line = linenum;
        tba->func = functionname;
        tba->s = conditionStr;
        tba->iter_count = i;
        *__i = tba;
        *__success = 0;
        printf("F");
    }
    else
    {
        printf(".");
    }

    fflush(NULL);
    return condition;
}

void _unittest_print_list(__test_item* __i)
{
    __test_item* __tmp = NULL;
    while (__i)
    {
        __test_item* __o = __i->next;
        __i->next = __tmp;
        __tmp = __i;
        __i = __o;
    }
    __i = __tmp;

    while(__i)
    {
        printf("%s: In function `%s':\n%s:%d: error: failed UNITTEST_ASSERT", __i->file, __i->func, __i->file, __i->line);
        if (__i->iter_count != -1) printf(" at iteration %d", __i->iter_count);
        printf(": %s\n", __i->s);
        __test_item* tbd = __i;
        __i = __i->next;
        free(tbd);
    }
}

// test by building like:
//   gcc -g -Wall -Werror -DTEST_UNITTEST_SCAFFOLD unittest.c
// #define TEST_UNITTEST_SCAFFOLD 1
#if TEST_UNITTEST_SCAFFOLD

// modify this test as necessary to test the scaffold
UNITTEST_HEADER(test1)
    int i = 0;
    int j = 1;
    int k = 2;

    UNITTEST_ASSERT(i==j);
    UNITTEST_ASSERTI(j==i, k);
    UNITTEST_ASSERT(i==i);
    UNITTEST_ASSERTI(j==j, k);
    UNITTEST_ASSERT_RETURN(j==j);
    UNITTEST_ASSERTI_RETURN(j==j, k);
UNITTEST_FOOTER

UNITTEST_HEADER(test2)
    UNITTEST_ASSERT(1);
    UNITTEST_ASSERT(0);
    UNITTEST_ASSERT(1);
UNITTEST_FOOTER

UNITTEST_HEADER(unittest_tests)
UNITTEST_TEST(test1)
UNITTEST_TEST(test2)
UNITTEST_FOOTER

UNITTEST_HEADER(run_tests)
UNITTEST_GROUP(unittest_tests)
UNITTEST_FOOTER

UNITTEST_MAIN

#endif // TEST_UNITTEST_SCAFFOLD
