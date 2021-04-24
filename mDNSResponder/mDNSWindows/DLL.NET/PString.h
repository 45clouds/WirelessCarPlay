/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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

using namespace System;
using namespace System::Text;

namespace Apple
{
	__gc class PString
	{
	public:

		PString(String* string)
		{
			if (string != NULL)
			{
				Byte unicodeBytes[] = Encoding::Unicode->GetBytes(string);
				Byte utf8Bytes[] = Encoding::Convert(Encoding::Unicode, Encoding::UTF8, unicodeBytes);
				m_p = Marshal::AllocHGlobal(utf8Bytes->Length + 1);

				Byte __pin * p = &utf8Bytes[0];
				char * hBytes = static_cast<char*>(m_p.ToPointer());
				memcpy(hBytes, p, utf8Bytes->Length);
				hBytes[utf8Bytes->Length] = '\0';
			}
			else
			{
				m_p = NULL;
			}
		}

		~PString()
		{
			Marshal::FreeHGlobal(m_p);
		}

		const char*
		c_str()
		{
			if (m_p != NULL)
			{
				return static_cast<const char*>(m_p.ToPointer());
			}
			else
			{
				return NULL;
			}
		}
		
	protected:

		IntPtr m_p;
	};
}
