
#include <iostream>
#include <string>
#include <cwchar>

#include <windows.h>
#include <setupapi.h>
#include <combaseapi.h>
#include <newdev.h>
#include <Usbiodef.h>

using namespace std::string_literals;



// Absolute path to inf file
static const wchar_t* INF_FILE = L"<absolute_path_to_inf_file>";

// Device name - take from DeviceManmager from after manual install
static const wchar_t* DEVICE_NAME = L"ROOT\\UNKNOWN\\0000";

// HWID - driver is assigned by driverid
static const wchar_t* HWID = L"root\\usbip_vhci";

// Description which we can see in Device Manager
// Description is overriden by driver (dyuring driver installation)
static const wchar_t* DEVICE_DESCRIPTION = L"USB/IP VHCI Test";

#define BUFFER_LENGTH 256



int main()
{
	GUID class_guid = {};
	GUID interface_class_guid = {};

	SP_DEVINFO_DATA DeviceInfoData_dup = {};
	SP_DEVINFO_DATA DeviceInfoData;

	wchar_t buffer[BUFFER_LENGTH] = {};
	DWORD required_size = 0;
	if (!SetupDiGetINFClass(INF_FILE,
			&class_guid, buffer, BUFFER_LENGTH, &required_size)) {
		std::cerr << "Cannot get Class GUID" << std::endl;
		return 1;
	}

	HDEVINFO devinfoset = SetupDiCreateDeviceInfoList(&class_guid, NULL);
	if (devinfoset == INVALID_HANDLE_VALUE) {
		std::cerr << "Cannot create DeviceInfoList" << std::endl;
		return 1;
	}

	// Try create DeviceInfo
	// If Device with given name already exist, delete it and try
	// to create new device info.
	bool try_twice = false;
	do {
		memset(&DeviceInfoData, 0, sizeof(SP_DEVINFO_DATA));
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		const bool devinfo_ok = SetupDiCreateDeviceInfo(devinfoset,
			DEVICE_NAME,
			&class_guid,
			DEVICE_DESCRIPTION,
			GetConsoleWindow(),
			0,
			&DeviceInfoData);
		if (!devinfo_ok) {
			DWORD last_error = GetLastError();
			if (last_error == ERROR_DEVINST_ALREADY_EXISTS) {
				memset(&DeviceInfoData, 0, sizeof(SP_DEVINFO_DATA));
				DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
				const BOOL open_ok = SetupDiOpenDeviceInfo(devinfoset,
					DEVICE_NAME,
					GetConsoleWindow(),
					DIOD_CANCEL_REMOVE, &DeviceInfoData);
				if (!open_ok) {
					std::cerr << "Cannot open or create" << std::endl;
					goto end;
				}
				if (!DiUninstallDevice(GetConsoleWindow(),
						devinfoset,
						&DeviceInfoData,
						0, false)) {
					std::cerr << "Cannot uninstall existing device. Try another device name, restart computer, ..." << std::endl;
					goto end;
				}
				try_twice = !try_twice;
				continue;
			}
			else {
				goto end;
			}
		}
		break;
	} while (try_twice);

	// Set HWID for device. Needed for driver.
	if (!SetupDiSetDeviceRegistryProperty(devinfoset,
			&DeviceInfoData, SPDRP_HARDWAREID,
			(const BYTE*)HWID,
			(DWORD)(sizeof(wchar_t) * std::wcslen(HWID)))) {
		std::cerr << "Cannot set device HWID" << std::endl;
		goto end;
	}

	// Register new device
	DeviceInfoData_dup.cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiRegisterDeviceInfo(devinfoset,
			&DeviceInfoData,
			SPRDI_FIND_DUPS, NULL, NULL,
			&DeviceInfoData_dup)) {
		std::cerr << "Cannot register device" << std::endl;
		goto end;
	}

	// Install driver for device
	if(!UpdateDriverForPlugAndPlayDevices(GetConsoleWindow(),
			HWID,
			INF_FILE,
			INSTALLFLAG_FORCE,
			false)) {
		std::cerr << "Cannot install driver" << std::endl;
		goto end;
	}

	SetupDiDestroyDeviceInfoList(&devinfoset);
	return 0;

end:
	SetupDiDestroyDeviceInfoList(&devinfoset);
	return 1;
}