/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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

    
#include "ControlPanelExe.h"
#include "ConfigDialog.h"
#include "ConfigPropertySheet.h"
#include "resource.h"

#include <DebugServices.h>
#include "loclibrary.h"
#include <strsafe.h>


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifndef HeapEnableTerminationOnCorruption
#	define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS) 1
#endif


// Stash away pointers to our resource DLLs

static HINSTANCE g_nonLocalizedResources	= NULL;
static HINSTANCE g_localizedResources		= NULL;


HINSTANCE	GetNonLocalizedResources()
{
	return g_nonLocalizedResources;
}


HINSTANCE	GetLocalizedResources()
{
	return g_localizedResources;
}


//---------------------------------------------------------------------------------------------------------------------------
//	Static Declarations
//---------------------------------------------------------------------------------------------------------------------------
DEFINE_GUID(CLSID_ControlPanel, 

0x1207552c, 0xe59, 0x4d9f, 0x85, 0x54, 0xf1, 0xf8, 0x6, 0xcd, 0x7f, 0xa9);

static LPCTSTR g_controlPanelGUID			=	TEXT( "{1207552C-0E59-4d9f-8554-F1F806CD7FA9}" );
static LPCTSTR g_controlPanelName			=	TEXT( "Bonjour" );
static LPCTSTR g_controlPanelCanonicalName	=	TEXT( "Apple.Bonjour" );
static LPCTSTR g_controlPanelCategory		=	TEXT( "3,8" );

static CCPApp theApp;

//===========================================================================================================================
//	MyRegDeleteKey
//===========================================================================================================================

DEBUG_LOCAL OSStatus MyRegDeleteKey( HKEY hKeyRoot, LPTSTR lpSubKey )
{
    LPTSTR lpEnd;
    OSStatus err;
    DWORD dwSize;
    TCHAR szName[MAX_PATH];
    HKEY hKey;
    FILETIME ftWrite;

    // First, see if we can delete the key without having to recurse.

    err = RegDeleteKey( hKeyRoot, lpSubKey );

    if ( !err )
	{
		goto exit;
	}

    err = RegOpenKeyEx( hKeyRoot, lpSubKey, 0, KEY_READ, &hKey );
	require_noerr( err, exit );

    // Check for an ending slash and add one if it is missing.

    lpEnd = lpSubKey + lstrlen(lpSubKey);

    if ( *( lpEnd - 1 ) != TEXT( '\\' ) ) 
    {
        *lpEnd =  TEXT('\\');
        lpEnd++;
        *lpEnd =  TEXT('\0');
    }

    // Enumerate the keys

    dwSize = MAX_PATH;
    err = RegEnumKeyEx(hKey, 0, szName, &dwSize, NULL, NULL, NULL, &ftWrite);

    if ( !err ) 
    {
        do
		{
            lstrcpy (lpEnd, szName);

            if ( !MyRegDeleteKey( hKeyRoot, lpSubKey ) )
			{
                break;
            }

            dwSize = MAX_PATH;

            err = RegEnumKeyEx( hKey, 0, szName, &dwSize, NULL, NULL, NULL, &ftWrite );

        }
		while ( !err );
    }

    lpEnd--;
    *lpEnd = TEXT('\0');

    RegCloseKey( hKey );

    // Try again to delete the key.

    err = RegDeleteKey(hKeyRoot, lpSubKey);
	require_noerr( err, exit );

exit:

	return err;
}



//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::CCPApp
//---------------------------------------------------------------------------------------------------------------------------
IMPLEMENT_DYNAMIC(CCPApp, CWinApp);

CCPApp::CCPApp()
{
	debug_initialize( kDebugOutputTypeWindowsEventLog, "DNS-SD Control Panel", GetModuleHandle( NULL ) );
	debug_set_property( kDebugPropertyTagPrintLevel, kDebugLevelInfo );
}


//---------------------------------------------------------------------------------------------------------------------------
//	CCPApp::~CCPApp
//---------------------------------------------------------------------------------------------------------------------------

CCPApp::~CCPApp()
{
}


void
CCPApp::Register( LPCTSTR inClsidString, LPCTSTR inName, LPCTSTR inCanonicalName, LPCTSTR inCategory, LPCTSTR inLocalizedName, LPCTSTR inInfoTip, LPCTSTR inIconPath, LPCTSTR inExePath )
{
	typedef struct	RegistryBuilder		RegistryBuilder;
	
	struct	RegistryBuilder
	{
		HKEY		rootKey;
		LPCTSTR		subKey;
		LPCTSTR		valueName;
		DWORD		valueType;
		LPCTSTR		data;
	};
	
	OSStatus			err;
	size_t				n;
	size_t				i;
	HKEY				key;
	TCHAR				keyName[ MAX_PATH ];
	RegistryBuilder		entries[] = 
	{
		{ HKEY_LOCAL_MACHINE,	TEXT( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ControlPanel\\NameSpace\\%s" ),	NULL,									REG_SZ,		inName },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s" ),																			NULL,									NULL,		NULL },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s" ),																			TEXT( "System.ApplicationName" ),		REG_SZ,		inCanonicalName },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s" ),																			TEXT( "System.ControlPanel.Category" ),	REG_SZ,		inCategory },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s" ),																			TEXT( "LocalizedString" ),				REG_SZ,		inLocalizedName },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s" ),																			TEXT( "InfoTip" ),						REG_SZ,		inInfoTip },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s\\DefaultIcon" ),																NULL,									REG_SZ,		inIconPath },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s\\Shell" ),																		NULL,									NULL,		NULL },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s\\Shell\\Open" ),																NULL,									NULL,		NULL },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s\\Shell\\Open\\Command" ),														NULL,									REG_SZ,		inExePath }
	};
	DWORD				size;
	
	// Register the registry entries.

	n = sizeof_array( entries );
	for( i = 0; i < n; ++i )
	{
		StringCbPrintf( keyName, sizeof( keyName ), entries[ i ].subKey, inClsidString );
		err = RegCreateKeyEx( entries[ i ].rootKey, keyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL );
		require_noerr( err, exit );
		
		if ( entries[ i ].data )
		{
			size = (DWORD)( ( lstrlen( entries[ i ].data ) + 1 ) * sizeof( TCHAR ) );
			err = RegSetValueEx( key, entries[ i ].valueName, 0, entries[ i ].valueType, (LPBYTE) entries[ i ].data, size );
			require_noerr( err, exit );
		}

		RegCloseKey( key );
	}
	
exit:
	return;
}


//-----------------------------------------------------------
//	CCPApp::Unregister
//-----------------------------------------------------------
void
CCPApp::Unregister( LPCTSTR clsidString )
{
	TCHAR keyName[ MAX_PATH * 2 ];

	StringCbPrintf( keyName, sizeof( keyName ), L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\ControlPanel\\NameSpace\\%s", clsidString );
	MyRegDeleteKey( HKEY_LOCAL_MACHINE, keyName );

	StringCbPrintf( keyName, sizeof( keyName ), L"CLSID\\%s", clsidString );
	MyRegDeleteKey( HKEY_CLASSES_ROOT, keyName );
}



//-----------------------------------------------------------
//	CCPApp::InitInstance
//-----------------------------------------------------------

BOOL
CCPApp::InitInstance()
{
	CCommandLineInfo	commandLine;
	wchar_t				resource[MAX_PATH];
	CString				errorMessage;
	CString				errorCaption;
	int					res;
	OSStatus			err = kNoErr;

	HeapSetInformation( NULL, HeapEnableTerminationOnCorruption, NULL, 0 );

	//
	// initialize the debugging framework
	//
	debug_initialize( kDebugOutputTypeWindowsDebugger, "ControlPanel", NULL );
	debug_set_property( kDebugPropertyTagPrintLevel, kDebugLevelTrace );

	// Before we load the resources, let's load the error string

	errorMessage.LoadString( IDS_REINSTALL );
	errorCaption.LoadString( IDS_REINSTALL_CAPTION );

	res = PathForResource( NULL, L"ControlPanelResources.dll", resource, MAX_PATH );
	err = translate_errno( res != 0, kUnknownErr, kUnknownErr );
	require_noerr( err, exit );

	g_nonLocalizedResources = LoadLibrary( resource );
	translate_errno( g_nonLocalizedResources, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	res = PathForResource( NULL, L"ControlPanelLocalized.dll", resource, MAX_PATH );
	err = translate_errno( res != 0, kUnknownErr, kUnknownErr );
	require_noerr( err, exit );

	g_localizedResources = LoadLibrary( resource );
	translate_errno( g_localizedResources, GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	AfxSetResourceHandle( g_localizedResources );

	// InitCommonControls() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.

	InitCommonControls();

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	ParseCommandLine( commandLine );

	if ( commandLine.m_nShellCommand == CCommandLineInfo::AppRegister )
	{
		CString		localizedName;
		CString		toolTip;
		TCHAR		iconPath[ MAX_PATH + 12 ]	= TEXT( "" );
		TCHAR		exePath[ MAX_PATH ]			= TEXT( "" );
		DWORD		nChars;
		OSStatus	err;

		nChars = GetModuleFileName( NULL, exePath, sizeof_array( exePath ) );

		err = translate_errno( nChars > 0, (OSStatus) GetLastError(), kUnknownErr );

		require_noerr( err, exit );

		StringCbPrintf( iconPath, sizeof( iconPath ), L"%s,-%d", exePath, IDR_APPLET );

		localizedName.LoadString( IDS_APPLET_NAME );
		toolTip.LoadString( IDS_APPLET_TOOLTIP );

		Register( g_controlPanelGUID, g_controlPanelName, g_controlPanelCanonicalName, g_controlPanelCategory, localizedName, toolTip, iconPath, exePath );
	}
	else if ( commandLine.m_nShellCommand == CCommandLineInfo::AppUnregister )
	{
		Unregister( g_controlPanelGUID );
	}
	else
	{
		CString					name;
		CConfigPropertySheet	dlg;
		
		name.LoadString( IDR_APPLET );
		dlg.Construct( name, NULL, 0 );

		m_pMainWnd = &dlg;

		try
		{
			INT_PTR nResponse = dlg.DoModal();
		
			if (nResponse == IDOK)
			{
				// TODO: Place code here to handle when the dialog is
				//  dismissed with OK
			}
			else if (nResponse == IDCANCEL)
			{
				// TODO: Place code here to handle when the dialog is
				//  dismissed with Cancel
			}
		}
		catch (...)
		{
			MessageBox(NULL, L"", L"", MB_OK|MB_ICONEXCLAMATION);
		}
	}

	if ( err )
	{
		MessageBox( NULL, L"", L"", MB_ICONERROR | MB_OK );
	}

exit:

	if ( err )
	{
		MessageBox( NULL, errorMessage, errorCaption, MB_ICONERROR | MB_OK );
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}
