/*
    File: PrivilegedOperations.c

    Abstract: Interface to "ddnswriteconfig" setuid root tool.

    Copyright: (c) Copyright 2005-2015 Apple Inc. All rights reserved.

    Disclaimer: IMPORTANT: This Apple software is supplied to you by Apple Inc.
    ("Apple") in consideration of your agreement to the following terms, and your
    use, installation, modification or redistribution of this Apple software
    constitutes acceptance of these terms.  If you do not agree with these terms,
    please do not use, install, modify or redistribute this Apple software.

    In consideration of your agreement to abide by the following terms, and subject
    to these terms, Apple grants you a personal, non-exclusive license, under Apple's
    copyrights in this original Apple software (the "Apple Software"), to use,
    reproduce, modify and redistribute the Apple Software, with or without
    modifications, in source and/or binary forms; provided that if you redistribute
    the Apple Software in its entirety and without modifications, you must retain
    this notice and the following text and disclaimers in all such redistributions of
    the Apple Software.  Neither the name, trademarks, service marks or logos of
    Apple Inc. may be used to endorse or promote products derived from the
    Apple Software without specific prior written permission from Apple.  Except as
    expressly stated in this notice, no other rights or licenses, express or implied,
    are granted by Apple herein, including but not limited to any patent rights that
    may be infringed by your derivative works or by other works in which the Apple
    Software may be incorporated.

    The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
    WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
    COMBINATION WITH YOUR PRODUCTS.

    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
    OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
    (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PrivilegedOperations.h"
#include "ConfigurationAuthority.h"
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <AssertMacros.h>
#include <Security/Security.h>

extern char **environ;
Boolean gToolApproved = false;

static pid_t execTool(const char *args[])
{
    pid_t child;

    int err = posix_spawn(&child, args[0], NULL, NULL, (char *const *)args, environ);
    if (err)
    {
        printf("exec of %s failed; err = %d\n", args[0], err);
        return -1;
    }
    else
        return child;
}

OSStatus EnsureToolInstalled(void)
// Make sure that the tool is installed in the right place, with the right privs, and the right version.
{
    CFURLRef bundleURL;
    pid_t toolPID;
    int status = 0;
    OSStatus err = noErr;
    const char      *args[] = { kToolPath, "0", "V", NULL };
    char toolSourcePath[PATH_MAX] = {};
    char toolInstallerPath[PATH_MAX] = {};

    if (gToolApproved)
        return noErr;

    // Check version of installed tool
    toolPID = execTool(args);
    if (toolPID > 0)
    {
        waitpid(toolPID, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == PRIV_OP_TOOL_VERS)
            return noErr;
    }

    // Locate our in-bundle copy of privop tool
    bundleURL = CFBundleCopyBundleURL(CFBundleGetBundleWithIdentifier(CFSTR("com.apple.preference.bonjour")) );
    if (bundleURL != NULL)
    {
        CFURLGetFileSystemRepresentation(bundleURL, false, (UInt8*) toolSourcePath, sizeof toolSourcePath);
        CFURLGetFileSystemRepresentation(bundleURL, false, (UInt8*) toolInstallerPath, sizeof toolInstallerPath);
        CFRelease(bundleURL);

        if (strlcat(toolSourcePath,    "/Contents/Resources/" kToolName,      sizeof toolSourcePath   ) >= sizeof toolSourcePath   ) return(-1);
        if (strlcat(toolInstallerPath, "/Contents/Resources/" kToolInstaller, sizeof toolInstallerPath) >= sizeof toolInstallerPath) return(-1);
    }
    else
        return coreFoundationUnknownErr;

    // Obtain authorization and run in-bundle copy as root to install it
    {
        AuthorizationItem aewpRight = { kAuthorizationRightExecute, strlen(toolInstallerPath), toolInstallerPath, 0 };
        AuthorizationItemSet rights = { 1, &aewpRight };
        AuthorizationRef authRef;

        err = AuthorizationCreate(&rights, (AuthorizationEnvironment*) NULL,
                                  kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights |
                                  kAuthorizationFlagPreAuthorize, &authRef);
        if (err == noErr)
        {
            char *installerargs[] = { toolSourcePath, NULL };
            err = AuthorizationExecuteWithPrivileges(authRef, toolInstallerPath, 0, installerargs, (FILE**) NULL);
            if (err == noErr)
            {
                int pid = wait(&status);
                if (pid > 0 && WIFEXITED(status))
                {
                    err = WEXITSTATUS(status);
                    if (err == noErr)
                    {
                        gToolApproved = true;
                    }
                } else {
                    err = -1;
                }
            }
            (void) AuthorizationFree(authRef, kAuthorizationFlagDefaults);
        }
    }

    return err;
}


static OSStatus ExecWithCmdAndParam(const char *subCmd, CFDataRef paramData)
// Execute our privop tool with the supplied subCmd and parameter
{
    OSStatus err = noErr;
    int commFD, dataLen;
    u_int32_t len;
    pid_t child;
    char fileNum[16];
    UInt8                   *buff;
    const char              *args[] = { kToolPath, NULL, "A", NULL, NULL };
    AuthorizationExternalForm authExt;

    err = ExternalizeAuthority(&authExt);
    require_noerr(err, AuthFailed);

    dataLen = CFDataGetLength(paramData);
    buff = (UInt8*) malloc(dataLen * sizeof(UInt8));
    require_action(buff != NULL, AllocBuffFailed, err=memFullErr;);
    {
        CFRange all = { 0, dataLen };
        CFDataGetBytes(paramData, all, buff);
    }

    commFD = fileno(tmpfile());
    snprintf(fileNum, sizeof(fileNum), "%d", commFD);
    args[1] = fileNum;
    args[3] = subCmd;

    // write authority to pipe
    len = 0;    // tag, unused
    write(commFD, &len, sizeof len);
    len = sizeof authExt;   // len
    write(commFD, &len, sizeof len);
    write(commFD, &authExt, len);

    // write parameter to pipe
    len = 0;    // tag, unused
    write(commFD, &len, sizeof len);
    len = dataLen;  // len
    write(commFD, &len, sizeof len);
    write(commFD, buff, len);

    child = execTool(args);
    if (child > 0)
    {
        int status = 0;
        waitpid(child, &status, 0);
        if (WIFEXITED(status))
            err = WEXITSTATUS(status);
        //fprintf(stderr, "child exited; status = %d (%ld)\n", status, err);
    }

    close(commFD);

    free(buff);
AllocBuffFailed:
AuthFailed:
    return err;
}

OSStatus
WriteBrowseDomain(CFDataRef domainArrayData)
{
    if (!CurrentlyAuthorized())
        return authFailErr;
    return ExecWithCmdAndParam("Wb", domainArrayData);
}

OSStatus
WriteRegistrationDomain(CFDataRef domainArrayData)
{
    if (!CurrentlyAuthorized())
        return authFailErr;
    return ExecWithCmdAndParam("Wd", domainArrayData);
}

OSStatus
WriteHostname(CFDataRef domainArrayData)
{
    if (!CurrentlyAuthorized())
        return authFailErr;
    return ExecWithCmdAndParam("Wh", domainArrayData);
}

OSStatus
SetKeyForDomain(CFDataRef secretData)
{
    if (!CurrentlyAuthorized())
        return authFailErr;
    return ExecWithCmdAndParam("Wk", secretData);
}
