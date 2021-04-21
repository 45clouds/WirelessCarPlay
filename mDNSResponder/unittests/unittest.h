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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#ifndef _UNITTEST_H_
#define _UNITTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __test_item_
{
    struct     __test_item_*   next;
    const      char*           file;
    unsigned   int             line;
    const      char*           func;
    const      char*           s;
    int        iter_count;
}   __test_item;

    
#define UNITTEST_HEADER(X) int X() { int __success = 1; __test_item* __i = NULL;

#define UNITTEST_GROUP(X) { printf("== %s ==\n", #X); __success = X() && __success; }
#define UNITTEST_TEST(X)  { printf("%s: ", #X); fflush(NULL); __success = X() && __success; }

int _unittest_assert_i(const int condition, const int i, const char * const conditionStr,
                       const char * const filename, const unsigned int linenum,
                       const char * const functionname, __test_item ** __i, int * const __success);
#define UNITTEST_ASSERTI(X,I) (_unittest_assert_i((X)!=0, (I), #X, __FILE__, __LINE__, __func__, &__i, &__success))
#define UNITTEST_ASSERT(X)    UNITTEST_ASSERTI(X, -1)
#define UNITTEST_ASSERTI_RETURN(X,I) { if (!UNITTEST_ASSERTI(X,I)) goto __unittest_footer__; }
#define UNITTEST_ASSERT_RETURN(X) UNITTEST_ASSERTI_RETURN(X, -1)

void _unittest_print_list(__test_item* __i);
#define UNITTEST_FOOTER       goto __unittest_footer__; __unittest_footer__: printf("\n"); fflush(NULL); _unittest_print_list(__i); return __success; }

#define UNITTEST_MAIN     int main (int argc, char** argv) \
                          { \
                              (void)(argv); \
                              signal(SIGPIPE, SIG_IGN); \
                              FILE* fp; \
                              unlink("unittest_success"); \
                              if (!run_tests()) return -1; \
                              fp = fopen("unittest_success", "w"); \
                              if (!fp) return -2; \
                              fclose(fp); \
                              printf("unit test SUCCESS\n"); \
                              if (argc != 1) \
                              { \
                                  char c; \
                                  printf("run leaks now\n"); \
                                  read(STDIN_FILENO, &c, 1); \
                              } \
                              return 0; \
                          }

#define UNITTEST_FAIL_ASSERT { assert(((void*)__func__) == 0); }

#ifdef __cplusplus
}
#endif

#endif // ndef _UNITTEST_H_
