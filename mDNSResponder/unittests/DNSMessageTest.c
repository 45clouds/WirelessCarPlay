#include "mDNSEmbeddedAPI.h"
#include "DNSMessageTest.h"
#include "../mDNSCore/DNSCommon.h"

int SizeTest(void);
int InitializeTest(void);
int PutDomainNameAsLabels(void);
int PutRData(void);
int Finalize(void);


DNSMessage *msg;


UNITTEST_HEADER(DNSMessageTest)
    UNITTEST_TEST(SizeTest)
    UNITTEST_TEST(InitializeTest)
    UNITTEST_TEST(Finalize)
UNITTEST_FOOTER


UNITTEST_HEADER(SizeTest)
    msg = (DNSMessage *)malloc (sizeof(DNSMessage));
    UNITTEST_ASSERT_RETURN(msg != NULL);

    // message header should be 12 bytes
    UNITTEST_ASSERT(sizeof(msg->h) == 12);
UNITTEST_FOOTER


UNITTEST_HEADER(InitializeTest)
    // Initialize the message
    InitializeDNSMessage(&msg->h, onesID, QueryFlags);

    // Check that the message is initialized properly
    UNITTEST_ASSERT(msg->h.numAdditionals  == 0);
    UNITTEST_ASSERT(msg->h.numAnswers      == 0);
    UNITTEST_ASSERT(msg->h.numQuestions    == 0);
    UNITTEST_ASSERT(msg->h.numAuthorities  == 0);
UNITTEST_FOOTER


UNITTEST_HEADER(PutDomainNameAsLabels)

UNITTEST_FOOTER

UNITTEST_HEADER(Finalize)
    UNITTEST_ASSERT_RETURN(msg != NULL)
    free(msg);
UNITTEST_FOOTER