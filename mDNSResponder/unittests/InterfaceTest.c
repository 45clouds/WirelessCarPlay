#include "InterfaceTest.h"
#include "mDNSEmbeddedAPI.h"


NetworkInterfaceInfo *intf;
mDNS *m;

int LocalSubnetTest(void);

UNITTEST_HEADER(InterfaceTest)
    UNITTEST_TEST(LocalSubnetTest)
UNITTEST_FOOTER

UNITTEST_HEADER(LocalSubnetTest)
    // need a way to initialize m before we call into the class of APIs that use a ptr to mDNS
    // should that pointer be common to all tests?
    // mDNS_AddressIsLocalSubnet(mDNS *const m, const mDNSInterfaceID InterfaceID, const mDNSAddr *addr)
    // TEST_ASSERT_RETURN (for IPv4/IPv6 local subnet)
UNITTEST_FOOTER
