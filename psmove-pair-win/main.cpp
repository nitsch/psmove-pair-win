/**
 * Copyright (c) 2014 Alexander Nitsch <nitsch@ht.tu-berlin.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Windows.h>
#include <BluetoothAPIs.h>

#include <conio.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>


// A value that indicates the time out for the inquiry, expressed in increments of 1.28 seconds.For example, an inquiry of 12.8 seconds has a cTimeoutMultiplier value of 10. The maximum value for this member is 48. When a value greater than 48 is used, the calling function immediately fails and returns
#define GET_BT_DEVICES_TIMEOUT_MULTIPLIER 1 // 0.2  // 4

// Every x loop issue a new inquiry
#define BT_SCAN_NEW_INQUIRY 5

// Sleep value between bt device scan
// Recommondation: Value should be higher than GET_BT_DEVICES_TIMEOUT_MULTIPLIER * 1.28 * 1000
#define SLEEP_BETWEEN_SCANS (unsigned int) GET_BT_DEVICES_TIMEOUT_MULTIPLIER * 1.28 * 1000 * 1.1

// amount of connection retries
#define CONN_RETRIES 80    //10_BAD   //5_BAD   //20   //60
// the delay (in milliseconds) between connection retries
#define CONN_DELAY   300   //800_BAD  //500_BAD //500  //100


// the number of successive checks that we require to be sure the Bluetooth connection is indeed
// properly established
#define CONN_CHECK_NUM_TRIES 5

// the delay (in milliseconds) between consecutive checks for a properly established Bluetooth
// connection
#define CONN_CHECK_DELAY 300


bool g_exitRequested = false;


BOOL WINAPI ctrlHandler( DWORD ctrlType )
{
	if( ctrlType == CTRL_C_EVENT )
	{
		g_exitRequested = true;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}


void printError( char const* message, DWORD errorCode )
{
	LPVOID lpBuffer;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errorCode,
		MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
		(LPTSTR) &lpBuffer,
		0,
		NULL );

	printf( "[ERROR 0x%08x] %s: %ls", errorCode, message, lpBuffer );

	LocalFree( lpBuffer );
}


void printError( char const* message )
{
	printError( message, GetLastError() );
}


char* bdaddrToString( BLUETOOTH_ADDRESS address )
{
	static char buffer[ 18 ];

	sprintf( buffer, "%02x:%02x:%02x:%02x:%02x:%02x",
		address.rgBytes[ 5 ],
		address.rgBytes[ 4 ],
		address.rgBytes[ 3 ],
		address.rgBytes[ 2 ],
		address.rgBytes[ 1 ],
		address.rgBytes[ 0 ] );

	return buffer;
}

LPWSTR bdaddrToRegString(BLUETOOTH_ADDRESS address)
{
	static WCHAR buffer[13];

	wsprintf(buffer, L"%02x%02x%02x%02x%02x%02x",
		address.rgBytes[5],
		address.rgBytes[4],
		address.rgBytes[3],
		address.rgBytes[2],
		address.rgBytes[1],
		address.rgBytes[0]);

	return buffer;
}

std::vector< HANDLE > getBluetoothRadios()
{
	std::vector< HANDLE > radioHandles;

	BLUETOOTH_FIND_RADIO_PARAMS radioParam;
	radioParam.dwSize = sizeof( radioParam );
	HANDLE hRadio;

	HBLUETOOTH_RADIO_FIND hFind = BluetoothFindFirstRadio( &radioParam, &hRadio );

	if( ! hFind )
	{
		printError( "Failed to enumerate Bluetooth radios" );
	}
	else
	{
		do
		{
			radioHandles.push_back( hRadio );
		}
		while( BluetoothFindNextRadio( hFind, &hRadio ) );

		if( ! BluetoothFindRadioClose( hFind ) )
		{
			printError( "Failed to close Bluetooth radio enumeration handle" );
		}
	}

	return radioHandles;
}


std::vector< BLUETOOTH_DEVICE_INFO > getBluetoothDeviceInfos(HANDLE const hRadio, bool fIssueInquiry)
{
	std::vector< BLUETOOTH_DEVICE_INFO > deviceInfoList;

	BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams;
	searchParams.dwSize               = sizeof( searchParams );
	searchParams.cTimeoutMultiplier   = GET_BT_DEVICES_TIMEOUT_MULTIPLIER;
	searchParams.fIssueInquiry        = fIssueInquiry; // TRUE; // FALSE;
	searchParams.fReturnAuthenticated = TRUE;
	searchParams.fReturnConnected     = TRUE;
	searchParams.fReturnRemembered    = TRUE;
	searchParams.fReturnUnknown       = TRUE;
	searchParams.hRadio               = hRadio;

	BLUETOOTH_DEVICE_INFO deviceInfo;
	deviceInfo.dwSize = sizeof( deviceInfo );

	HBLUETOOTH_DEVICE_FIND hFind = BluetoothFindFirstDevice( &searchParams, &deviceInfo );

	if( ! hFind )
	{
		if( GetLastError() != ERROR_NO_MORE_ITEMS )
		{
			printError( "Failed to enumerate devices" );
		}
	}
	else
	{
		do
		{
			deviceInfoList.push_back( deviceInfo );
		}
		while( BluetoothFindNextDevice( hFind, &deviceInfo ) );

		if( ! BluetoothFindDeviceClose( hFind ) )
		{
			printError( "Failed to close device enumeration handle" );
		}
	}

	return deviceInfoList;
}


bool setUpBluetoothRadio( HANDLE const hRadio )
{
	// NOTE: Order matters for the following two operations: The radio must allow incoming
	//       connections prior to being made discoverable.

	//printf( "Connectable:  %d\n", BluetoothIsConnectable( hRadio )  != FALSE );
	//printf( "Discoverable: %d\n", BluetoothIsDiscoverable( hRadio ) != FALSE );

	if( ! BluetoothIsConnectable( hRadio ) )
	{
		printf( "Making radio accept incoming connections.\n" );
		if( BluetoothEnableIncomingConnections( hRadio, TRUE ) == FALSE )
		{
			printError( "Failed to enable incoming connections" );
		}
	}

	if( ! BluetoothIsDiscoverable( hRadio ) )
	{
		printf( "Making radio discoverable.\n" );
		if( BluetoothEnableDiscovery( hRadio, TRUE ) == FALSE )
		{
			printError( "Failed to make radio discoverable" );
		}
	}

	//printf( "Connectable:  %d\n", BluetoothIsConnectable( hRadio )  != FALSE );
	//printf( "Discoverable: %d\n", BluetoothIsDiscoverable( hRadio ) != FALSE );

	return ( BluetoothIsConnectable( hRadio ) != FALSE )
			&& ( BluetoothIsDiscoverable( hRadio ) != FALSE );
}


void printBluetoothRadioInfo( HANDLE const hRadio )
{
	BLUETOOTH_RADIO_INFO radioInfo;
	radioInfo.dwSize = sizeof( radioInfo );

	DWORD result = BluetoothGetRadioInfo( hRadio, &radioInfo );
	if( result == ERROR_SUCCESS )
	{
		printf( "  %s  %ls\n", bdaddrToString( radioInfo.address ), radioInfo.szName );
	}
	else
	{
		printError( "Failed to retrieve Blutooth radio info", result );
	}
}


void printBluetoothDeviceInfo( BLUETOOTH_DEVICE_INFO const& deviceInfo )
{
	printf(
		"  %s  %ls\n" \
		"                          CoD: 0x%08x, CON: %d, REM: %d, AUTH: %d\n",
		bdaddrToString( deviceInfo.Address ),
		deviceInfo.szName,
		deviceInfo.ulClassofDevice,
		deviceInfo.fConnected     != FALSE,
		deviceInfo.fRemembered    != FALSE,
		deviceInfo.fAuthenticated != FALSE );
}


HANDLE chooseBluetoothRadio( std::vector< HANDLE > const& radioHandles )
{
	size_t numRadios = radioHandles.size();

	assert( numRadios > 0 );

	// list info for each discovered Bluetooth radio
	for( size_t i = 0; i < numRadios; ++i )
	{
		printf( "  (%d)", i );
		printBluetoothRadioInfo( radioHandles[ i ] );
	}
	printf( "\n" );

	// if there is more than one Bluetooth radio, let the user choose which one to use
	if( numRadios > 1 )
	{
		int n = -1;
		do
		{
			printf( "Choose one of the listed Bluetooth radios by number: " );
			n = _getch();
			printf( "%c\n", n );
			n = n - '0';
		}
		while( ( n < 0 ) || ( n >= numRadios ) );

		return radioHandles[ n ];
	}
	else
	{
		printf( "Using the only available Bluetooth radio.\n" );
		return radioHandles[ 0 ];
	}
}


void closeBluetoothRadios( std::vector< HANDLE > const& radioHandles )
{
	for( auto it = radioHandles.cbegin(); it != radioHandles.cend(); ++it )
	{
		if( ! CloseHandle( *it ) )
		{
			printError( "Failed to close Bluetooth radio handle" );
		}
	}
}


bool isMoveMotionController( BLUETOOTH_DEVICE_INFO const& deviceInfo )
{
	// TODO: Should we check deviceInfo.ulClassofDevice == 0x00002508 as well to make this more
	//       robust? Do Move Motion Controllers ever appear with a different Class of Device?

	return wcscmp( deviceInfo.szName, L"Motion Controller" ) == 0;
}


bool isHidServiceEnabled( HANDLE const hRadio, BLUETOOTH_DEVICE_INFO const& deviceInfo )
{
	// retrieve number of installed services
	DWORD numServices = 0;
	DWORD result = BluetoothEnumerateInstalledServices( hRadio, &deviceInfo, &numServices, NULL );
	if( result != ERROR_SUCCESS )
	{
		// NOTE: Sometimes we get ERROR_MORE_DATA, sometimes we do not. The number of services seems to
		//       be correct in any case, so we will just ignore this.
		if( result != ERROR_MORE_DATA )
		{
			printError( "Failed to count installed services", result );
			return false;
		}
	}

	if( numServices == 0 )
	{
		return false;
	}

	// retrieve actual list of installed services
	std::vector< GUID > serviceList( numServices );
	result = BluetoothEnumerateInstalledServices( hRadio, &deviceInfo, &numServices, serviceList.data() );
	if( result != ERROR_SUCCESS )
	{
		printError( "Failed to enumerate installed services", result );
		return false;
	}

	// check if the HID service is part of that list
	return std::find( serviceList.begin(), serviceList.end(), HumanInterfaceDeviceServiceClass_UUID ) != serviceList.end();
}


bool isConnectionEstablished( HANDLE const hRadio, BLUETOOTH_DEVICE_INFO& deviceInfo )
{
	// NOTE: Sometimes the Bluetooth connection appears to be established even though the Move
	//       decided that it is not really connected yet. That is why we cannot simply stop trying
	//       to connect after the first successful check. Instead, we require a minimum number of
	//       successive successful checks to be sure.
	
	for (unsigned int i = 0; i < CONN_CHECK_NUM_TRIES; ++i)
	{
		// read device info again to check if we have a connection
		DWORD result = BluetoothGetDeviceInfo(hRadio, &deviceInfo);
		if (result != ERROR_SUCCESS)
		{
			printf("\n");
			printError("Failed to read device info", result);
			return false;
		}

		if (deviceInfo.fConnected)
		{
			printf("C");
		}
		if (deviceInfo.fRemembered)
		{
			printf("R");
		}
		if (isHidServiceEnabled(hRadio, deviceInfo))
		{
			printf("E");
		}

		if (deviceInfo.fConnected && deviceInfo.fRemembered && isHidServiceEnabled(hRadio, deviceInfo))
		{
			printf(".");
		}
		else
		{
			printf("\n");
			return false;
		}

		Sleep(CONN_CHECK_DELAY);
	}

	printf("\n");
	return true;
}


bool changeRegistry(HANDLE hRadio, BLUETOOTH_DEVICE_INFO& deviceInfo)
{
	HKEY key;

	DWORD pvData;
	DWORD dwData;
	DWORD dwRet;
	DWORD pdwType;

	BLUETOOTH_RADIO_INFO radioInfo;
	radioInfo.dwSize = sizeof(radioInfo);

	dwRet = BluetoothGetRadioInfo(hRadio, &radioInfo);
	if (dwRet != ERROR_SUCCESS)
	{
		printError("Failed to get radio info", dwRet);
		return false;
	}

	WCHAR sSubkey[256];
	wmemset(sSubkey, 0x0, 256);
	wcscat(sSubkey, L"SYSTEM\\CurrentControlSet\\Services\\HidBth\\Parameters\\Devices\\");

	auto sRadio = bdaddrToRegString(radioInfo.address);
	wprintf(L"radioInfo.address: %s\n", sRadio);
	wcscat(sSubkey, sRadio);

	auto sDevice = bdaddrToRegString(deviceInfo.Address);
	wprintf(L"deviceInfo.address: %s\n", sDevice);
	wcscat(sSubkey, sDevice);

	wprintf(L"sSubkey: %s\n", sSubkey);

	dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, sSubkey, 0, KEY_READ | KEY_QUERY_VALUE | KEY_WOW64_64KEY | KEY_ALL_ACCESS, &key);
	if (ERROR_SUCCESS == dwRet)
	{
		do
		{
			//dwRet = RegGetValue(key, 0, L"VirtuallyCabled", RRF_RT_DWORD, &pdwType, &pvData, &dwData);
			dwRet = RegQueryValueEx(key, L"VirtuallyCabled", 0, &pdwType, &pvData, &dwData);
			if (ERROR_SUCCESS == dwRet)
			{
				//printf("pdwType: %d\n", pdwType);
				printf("Get VirtuallyCabled: %d\n", pvData);
				//printf("dwData: %d\n", dwData);
			}
			else
			{
				printError("Failed to get registry value", dwRet);
				// Ignore and continue
			}
		}while(ERROR_MORE_DATA == dwRet);
		
		dwData = 1;
		dwRet = RegSetValueEx(key, L"VirtuallyCabled", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
		if (ERROR_SUCCESS == dwRet)
		{
			//printf("pvData: %d\n", pvData);
			printf("Set VirtuallyCabled: %d\n", dwData);
		}
		else
		{
			printError("Failed to set registry value", dwRet);
			RegCloseKey(key);
			return false;
		}
		/*dwRet = RegSetValueEx(key, L"ConnectionAuthenticated", 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD));
		if (ERROR_SUCCESS == dwRet)
		{
			//printf("pvData: %d\n", pvData);
			printf("Set VirtuallyCabled: %d\n", dwData);
		}
		else
		{
			printError("Failed to set registry value", dwRet);
			RegCloseKey(key);
			return false;
		}*/
		RegCloseKey(key);

		/*
		dwData = 0;
		dwRet = RegGetValue(HKEY_LOCAL_MACHINE, sSubkey, L"VirtuallyCabled", RRF_RT_DWORD, &pdwType, &pvData, &dwData);
		if (ERROR_SUCCESS == dwRet)
		{
			//printf("pdwType: %d\n", pdwType);
			printf("Get VirtuallyCabled: %d\n", pvData);
			//printf("dwData: %d\n", dwData);
		}
		else
		{
			printError("Failed to get registry value", dwRet);
			// Ignore and continue
		}*/
	}
	else
	{
		printError("Failed to open registry key", dwRet);
		return false;
	}
	return true;
}

int main( int argc, char* argv[] )
{
	g_exitRequested = false;

	if( ! SetConsoleCtrlHandler( ctrlHandler, TRUE ) )
	{
		printError( "Failed to install ctrl handler" );
		return 0;
	}


	printf( "==== Looking for Bluetooth radios ====\n\n" );

	auto radioHandles = getBluetoothRadios();

	if( radioHandles.size() == 0 )
	{
		printf( "No Bluetooth radios found.\n" );
		return 0;
	}

	HANDLE hRadio = chooseBluetoothRadio( radioHandles );

	if( ! setUpBluetoothRadio( hRadio ) )
	{
		printError( "Failed to configure Bluetooth radio for use" );
		return 0;
	}


	printf( "\n\n" );
	printf( "==== Connecting new controllers ====\n\n" );

	unsigned int loop = 0;

	while( ! g_exitRequested )
	{
		auto deviceInfoList = getBluetoothDeviceInfos(hRadio, 0==loop%BT_SCAN_NEW_INQUIRY);
		if( deviceInfoList.empty() )
		{
			printf( "No Bluetooth devices found.\n" );
		}
		else
		{
			for( size_t i = 0; i < deviceInfoList.size(); ++i )
			{
				BLUETOOTH_DEVICE_INFO deviceInfo = deviceInfoList[ i ];

				printf( "Device: (%d)", i );
				printBluetoothDeviceInfo(deviceInfo);
				printf("\n");

				if (isMoveMotionController(deviceInfo))
				{
					printf("device #%d: Move Motion Controller detected\n", i);

					for (size_t i = 0; i < CONN_RETRIES; ++i)
					{
						DWORD result = BluetoothGetDeviceInfo(hRadio, &deviceInfo);
						if (result != ERROR_SUCCESS)
						{
							printError("Failed to read device info", result);
							break;
						}
						
						//result = BluetoothAuthenticateDevice(NULL, hRadio, &deviceInfo, L"0000", 4);
						//if (result != ERROR_SUCCESS)
						//{
						//	printf("BluetoothAuthenticateDevice ret %d\n", result);
						//}

						if (deviceInfo.fConnected)
						{
							changeRegistry(hRadio, deviceInfo);

							// enable HID service only if necessary
							printf("- checking HID service\n");
							if (!isHidServiceEnabled(hRadio, deviceInfo))
							{
								printf("- enabling HID service\n");
								DWORD result = BluetoothSetServiceState(hRadio, &deviceInfo, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);
								if (result != ERROR_SUCCESS)
								{
									printError("Failed to enable HID service", result);
									break;
								}

								changeRegistry(hRadio, deviceInfo);
							}

							printf("- verifying successful connection ");
							if (isConnectionEstablished(hRadio, deviceInfo))
							{
								// TODO: If we have a connection, stop trying to connect this device.
								//       For now, we will just keep on running endlessly.

								printf("- !!!! Successfully connected device %s !!!!\n", bdaddrToString(deviceInfo.Address));
								break;
							}

							// If we do not exit here, VirtuallyCabled will be reseted ??
							//g_exitRequested = true;
							//break;
						}
						else
						{
							//printf("Connected?\n");
						}

						Sleep(CONN_DELAY);

					} // for CONN_RETRIES

					if (!deviceInfo.fConnected)
					{
						BluetoothRemoveDevice(&deviceInfo.Address);
						printf("Device removed - retry");
					}
					
				}
				printf( "\n" );
			
			} // for deviceInfoList
		}
		loop++;
		Sleep(SLEEP_BETWEEN_SCANS );
	}


	// close Blueooth radio handles
	closeBluetoothRadios( radioHandles );

	return 0;
}
