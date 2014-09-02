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


std::vector< BLUETOOTH_DEVICE_INFO > getBluetoothDeviceInfos( HANDLE const hRadio )
{
	std::vector< BLUETOOTH_DEVICE_INFO > deviceInfoList;

	BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams;
	searchParams.dwSize               = sizeof( searchParams );
	searchParams.cTimeoutMultiplier   = 4;
	searchParams.fIssueInquiry        = FALSE;
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


	printf( "\n\n" );
	printf( "==== Connecting new controllers ====\n\n" );

	while( ! g_exitRequested )
	{
		auto deviceInfoList = getBluetoothDeviceInfos( hRadio );
		if( deviceInfoList.empty() )
		{
			printf( "No Bluetooth devices found.\n" );
		}
		else
		{
			for( size_t i = 0; i < deviceInfoList.size(); ++i )
			{
				BLUETOOTH_DEVICE_INFO deviceInfo = deviceInfoList[ i ];

				printf( "  (%d)", i );
				printBluetoothDeviceInfo( deviceInfo );
				printf( "\n" );

				if( isMoveMotionController( deviceInfo ) )
				{
					printf( "device #%d: Move Motion Controller detected\n", i );

					if( deviceInfo.fConnected )
					{
						// enable HID service only if necessary
						printf( "- checking HID service\n" );
						if( ! isHidServiceEnabled( hRadio, deviceInfo ) )
						{
							printf( "- enabling HID service\n" );
							DWORD result = BluetoothSetServiceState( hRadio, &deviceInfo, &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE );
							if( result != ERROR_SUCCESS )
							{
								printError( "Failed to enable HID service", result );
							}
						}

						// read device info again to check if we have a connection
						DWORD result = BluetoothGetDeviceInfo( hRadio, &deviceInfo );
						if( result != ERROR_SUCCESS )
						{
							printError( "Failed to read device info", result );
						}
						else
						{
							// if we have a connection, stop trying to connect
							printf( "- verifying successful connection\n" );
							if( deviceInfo.fConnected && deviceInfo.fRemembered && isHidServiceEnabled( hRadio, deviceInfo ) )
							{
								// TODO: Sometimes we get here even though the Move decided that it is not really connected
								//       yet. That is why we cannot simply stop trying to connect. We could probably add a
								//       little Sleep() before running the final check, or even check multiple times.

								printf( "- !!!! Successfully connected device %s !!!!\n", bdaddrToString( deviceInfo.Address ) );
							}
						}
					}
				}
				printf( "\n" );
			}
		}

		Sleep( 1000 );
	}


	// close Blueooth radio handles
	closeBluetoothRadios( radioHandles );

	return 0;
}
