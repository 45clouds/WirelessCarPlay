#include "mDNSEmbeddedAPI.h"
#include "../mDNSCore/DNSCommon.h"
#include "ResourceRecordTest.h"

int TXTSetupTest(void);
int ASetupTest(void);
int OPTSetupTest(void);


UNITTEST_HEADER(ResourceRecordTest)
    UNITTEST_TEST(TXTSetupTest)
    UNITTEST_TEST(ASetupTest)
    UNITTEST_TEST(OPTSetupTest)
UNITTEST_FOOTER


UNITTEST_HEADER(TXTSetupTest)

    AuthRecord authRec;
    mDNS_SetupResourceRecord(&authRec, mDNSNULL, mDNSInterface_Any, kDNSType_TXT, kStandardTTL, kDNSRecordTypeShared, AuthRecordAny,mDNSNULL, mDNSNULL);
    UNITTEST_ASSERT_RETURN(authRec.resrec.RecordType == kDNSType_TXT);
    UNITTEST_ASSERT_RETURN(authRec.resrec.rdata->MaxRDLength == sizeof(RDataBody));

    // Retest with a RDataStorage set to a a buffer
UNITTEST_FOOTER


UNITTEST_HEADER(ASetupTest)
    AuthRecord authRec;
    mDNS_SetupResourceRecord(&authRec, mDNSNULL, mDNSInterface_Any, kDNSType_A, kHostNameTTL, kDNSRecordTypeUnique, AuthRecordAny, mDNSNULL, mDNSNULL);
    UNITTEST_ASSERT_RETURN(authRec.resrec.RecordType == kDNSType_A);
    // Add more verifications

UNITTEST_FOOTER


UNITTEST_HEADER(OPTSetupTest)
    AuthRecord opt;
    mDNSu32    updatelease = 7200;
/*  mDNSu8     data[AbsoluteMaxDNSMessageData];
    mDNSu8     *p = data;
    mDNSu16    numAdditionals;
*/
    // Setup the OPT Record
    mDNS_SetupResourceRecord(&opt, mDNSNULL, mDNSInterface_Any, kDNSType_OPT, kStandardTTL, kDNSRecordTypeKnownUnique, AuthRecordAny, mDNSNULL, mDNSNULL);

    // Verify the basic initialization is all ok

    opt.resrec.rrclass    = NormalMaxDNSMessageData;
    opt.resrec.rdlength   = sizeof(rdataOPT);   // One option in this OPT record
    opt.resrec.rdestimate = sizeof(rdataOPT);
    opt.resrec.rdata->u.opt[0].opt           = kDNSOpt_Lease;
    opt.resrec.rdata->u.opt[0].u.updatelease = updatelease;

    // Put the resource record in and verify everything is fine
    // p = PutResourceRecordTTLWithLimit(&data, p, &numAdditionals, &opt.resrec, opt.resrec.rroriginalttl, data + AbsoluteMaxDNSMessageData);


    // Repeat with bad data to make sure it bails out cleanly
UNITTEST_FOOTER