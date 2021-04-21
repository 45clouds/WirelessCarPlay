#include "DomainNameTest.h"
#include "mDNSEmbeddedAPI.h"
#include "../mDNSCore/DNSCommon.h"

int SameDomainNameTest(void);
int SameDomainLabelTest(void);
int LocalDomainTest(void);


UNITTEST_HEADER(DomainNameTest)
    UNITTEST_TEST(SameDomainLabelTest)
    UNITTEST_TEST(SameDomainNameTest)
    UNITTEST_TEST(LocalDomainTest)
UNITTEST_FOOTER




UNITTEST_HEADER(SameDomainLabelTest)
UNITTEST_FOOTER


UNITTEST_HEADER(SameDomainNameTest)
UNITTEST_FOOTER


UNITTEST_HEADER(LocalDomainTest)
UNITTEST_FOOTER