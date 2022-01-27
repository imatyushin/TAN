#ifdef _WIN32

#include "RegisterBrowser.h"
#include <stdio.h>
#include <tchar.h>
#include <cstring>

WindowsRegister::WindowsRegister()
{
}

WindowsRegister::WindowsRegister(HKEY key, const std::string & subkey)
{
	loadKey(key, subkey);
}

void WindowsRegister::loadKey(HKEY key, const std::string & subkey)
{
	m_sSubKeyName = subkey;
	::RegOpenKeyExA(
		key,
		subkey.c_str(),
		0,
		KEY_READ,
		&m_RegisterKey
	    );


	DWORD    cchClassName = MAX_PATH;  // size of class string 
	DWORD    cbMaxSubKey;              // longest subkey size 
	DWORD    cchMaxClass;              // longest class string 
	DWORD    cchMaxValue;          // longest value name 
	DWORD    cbMaxValueData;       // longest value data 
	DWORD    cbSecurityDescriptor; // size of security descriptor 
								   // Get the class name and the value count. 
	DWORD retcode = ::RegQueryInfoKeyA(
		m_RegisterKey,           // key handle 
		m_cpName,                // buffer for class name 
		&cchClassName,           // size of class string 
		NULL,                    // reserved 
		&m_iNumOfSubKeys,        // number of subkeys 
		&cbMaxSubKey,            // longest subkey size 
		&cchMaxClass,            // longest class string 
		&m_iNumOfValue,                // number of values for this key 
		&cchMaxValue,            // longest value name 
		&cbMaxValueData,         // longest value data 
		&cbSecurityDescriptor,   // security descriptor 
		&m_iLastWriteTime);       // last write time

								  // Getting the subkeys name
	m_cpSubKeyNames = new char*[m_iNumOfSubKeys];
	for(int i =0 ; i < m_iNumOfSubKeys; i++)
	{
		m_cpSubKeyNames[i] = new char[MAX_KEY_LENGTH+1];
		m_cpSubKeyNames[i][0] = '\0';
	}
	m_cpValueNames = new char*[m_iNumOfValue];
	for(int i = 0; i < m_iNumOfValue; i++)
	{
		m_cpValueNames[i] = new char[MAX_VALUE_NAME+1];
		m_cpValueNames[i][0] = '\0';
	}
	getSubKeysNames();
	getValueNames();
}

WindowsRegister::~WindowsRegister()
{
	for(int i =0 ; i < m_iNumOfSubKeys;i++)
	{
		delete[]m_cpSubKeyNames[i];
	}
	delete[] m_cpSubKeyNames;

	for (int i = 0; i < m_iNumOfValue;i++)
	{
		delete[]m_cpValueNames[i];
	}
	delete[] m_cpValueNames;
	RegCloseKey(m_RegisterKey);
}

bool WindowsRegister::getSubKeysNames()
{	
	FILETIME ftLastWriteTime = { 0 };
	
	for (int i = 0; i<m_iNumOfSubKeys; i++)
	{
		DWORD cbName = MAX_KEY_LENGTH;
		DWORD retCode = ::RegEnumKeyExA(m_RegisterKey, i,
			m_cpSubKeyNames[i],
			&cbName,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);
		if (retCode != ERROR_SUCCESS)
		{
			return false;
		}
	}
	return true;
}

bool WindowsRegister::getValueNames()
{
	for (int i = 0, retCode = ERROR_SUCCESS; i<m_iNumOfValue; i++)
	{
		DWORD cchValue = MAX_VALUE_NAME;
		retCode = ::RegEnumValueA(m_RegisterKey, i,
			m_cpValueNames[i],
			&cchValue,
			NULL,
			NULL,
			NULL,
			NULL);

		if (retCode != ERROR_SUCCESS)
		{
			return false;
		}
	}

	return true;
}

void WindowsRegister::printSubKeyInfo()
{
	char    achKey[MAX_KEY_LENGTH] = { 0 };   // buffer for subkey name
	DWORD    cbName;                   // size of name string 
	FILETIME ftLastWriteTime;      // last write time 
	printf("\nNumber of subkeys: %d\n", m_iNumOfSubKeys);

	for (int i = 0; i<m_iNumOfSubKeys; i++)
	{
		cbName = MAX_KEY_LENGTH;
		DWORD retCode = ::RegEnumKeyExA(m_RegisterKey, i,
			achKey,
			&cbName,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);
		if (retCode == ERROR_SUCCESS)
		{
			_tprintf(TEXT("(%d) %s\n"), i + 1, achKey);
		}
	}
}

void WindowsRegister::printValueInfo()
{
	printf("\nNumber of values: %d\n", m_iNumOfValue);


	char  achValue[MAX_VALUE_NAME];
	DWORD cchValue = MAX_VALUE_NAME;

	for (int i = 0, retCode = ERROR_SUCCESS; i<m_iNumOfValue; i++)
	{
		cchValue = MAX_VALUE_NAME;
		achValue[0] = '\0';
		retCode = RegEnumValueA(m_RegisterKey, i,
			achValue,
			&cchValue,
			NULL,
			NULL,
			NULL,
			NULL);

		if (retCode == ERROR_SUCCESS)
		{
			_tprintf(TEXT("(%d) %s\n"), i + 1, achValue);
		}
	}
}

WindowsRegister* WindowsRegister::getSubKeys()
{
	//WindowsRegister
	WindowsRegister* return_registers = new WindowsRegister[m_iNumOfSubKeys];
	for(int i = 0; i<m_iNumOfSubKeys; i++)
	{
		return_registers[i].loadKey(this->m_RegisterKey, m_cpSubKeyNames[i]);
	}
	return return_registers;
}

WindowsRegister* WindowsRegister::getSubKey(const std::string & subkey)
{
	for (int i = 0; i < m_iNumOfSubKeys; i++)
	{
		if (!std::strcmp(m_cpSubKeyNames[i], subkey.c_str()))
		{
			return new WindowsRegister(m_RegisterKey, subkey);
		}
	}
	return nullptr;
}

std::string WindowsRegister::getStringValue(const std::string & valuename)
{	
	for(int i = 0; i < m_iNumOfValue; i++)
	{
		if(!std::strcmp(m_cpValueNames[i],valuename.c_str()))
		{
			char  achValue[MAX_VALUE_NAME];
			DWORD cchValue = MAX_VALUE_NAME;
			DWORD size;
			DWORD type;
			RegEnumValueA(m_RegisterKey, i,
				achValue,
				&cchValue,
				NULL,
				&type,
				NULL,
				&size);

			if(type == REG_SZ)
			{
				std::string ret;
				ret.reserve(size);
				RegGetValueA(m_RegisterKey, NULL, valuename.c_str(), RRF_RT_REG_SZ, NULL, &ret.front(), &size);

				return ret;
			}

			break;
		}
	}

	return std::string();
}

bool WindowsRegister::hasSubKey(const std::string & subkey)
{
	for(int i = 0 ; i < m_iNumOfSubKeys; i++)
	{
		if(!std::strcmp(m_cpSubKeyNames[i],subkey.c_str()))
		{
			return true;
		}
	}
	return false;
}

#endif