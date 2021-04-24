/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2009 Apple Computer, Inc. All rights reserved.
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



#pragma once

#include "resource.h"       // main symbols



#include "DLLX.h"

#include "_IDNSSDEvents_CP.H"





#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)

#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."

#endif







// CDNSSDEventManager



class ATL_NO_VTABLE CDNSSDEventManager :

	public CComObjectRootEx<CComSingleThreadModel>,

	public CComCoClass<CDNSSDEventManager, &CLSID_DNSSDEventManager>,

	public IConnectionPointContainerImpl<CDNSSDEventManager>,

	public CProxy_IDNSSDEvents<CDNSSDEventManager>,

	public IDispatchImpl<IDNSSDEventManager, &IID_IDNSSDEventManager, &LIBID_Bonjour, /*wMajor =*/ 1, /*wMinor =*/ 0>

{

public:

	CDNSSDEventManager()

	{

	}



DECLARE_REGISTRY_RESOURCEID(IDR_DNSSDEVENTMANAGER)





BEGIN_COM_MAP(CDNSSDEventManager)

	COM_INTERFACE_ENTRY(IDNSSDEventManager)

	COM_INTERFACE_ENTRY(IDispatch)

	COM_INTERFACE_ENTRY(IConnectionPointContainer)

END_COM_MAP()



BEGIN_CONNECTION_POINT_MAP(CDNSSDEventManager)

	CONNECTION_POINT_ENTRY(__uuidof(_IDNSSDEvents))

END_CONNECTION_POINT_MAP()





	DECLARE_PROTECT_FINAL_CONSTRUCT()



	HRESULT FinalConstruct()

	{

		return S_OK;

	}



	void FinalRelease()

	{

	}



public:



};



OBJECT_ENTRY_AUTO(__uuidof(DNSSDEventManager), CDNSSDEventManager)

