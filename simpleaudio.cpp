#include "stdafx.h"
#include "simpleaudio.h"

#include <iostream>
#include <functiondiscoverykeys_devpkey.h>
#include <chrono>
#include <thread>

using std::cout;
using std::wcout;
using std::endl;

void HR_CHECK(HRESULT hr)
{
	if (hr != S_OK)
	{
		cout << "NOOOOOOOOOOO" << endl;
		throw "BAD HRESULT";
	}
}

template <class T> void SAFE_RELEASE(T **ppUnk)
{
	if (*ppUnk)
	{
		(*ppUnk)->Release();
		*ppUnk = NULL;
	}
}

#define HR_CHECK(hr) HR_CHECK(hr)
#define SAFE_RELEASE(pUnk) SAFE_RELEASE(pUnk)


/* Device Interface */
simpleaudio::Interface::Interface()
{
	//Note: To use Compoment Objects (Co's) you have to initialize the COM library on the current thread before they can be used."
	CoInitialize(NULL);
	HR_CHECK(CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void **)&pEnumerator));
}

simpleaudio::Interface::~Interface()
{
	SAFE_RELEASE(&pEnumerator);
	CoUninitialize();
}

void simpleaudio::Interface::getDefaultDevice(Device **ppDevice)
{
	IMMDevice *pMMDevice;
	HR_CHECK(pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pMMDevice));
	*ppDevice = new Device(pMMDevice);
}

void simpleaudio::Interface::deviceIterator(iterator::Iterator<Device> **ppDeviceIterator)
{
	DeviceIteratorProfile *itProf = new DeviceIteratorProfile(this);
	*ppDeviceIterator = new iterator::Iterator<Device>(itProf);
}

/* Device */
simpleaudio::Device::Device(IMMDevice * pDevice)
{
	pDevice->AddRef();
	this->pDevice = pDevice;
	HR_CHECK(pDevice->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void **)&pVolume));
}

simpleaudio::Device::~Device()
{
	SAFE_RELEASE(&pDevice);
	SAFE_RELEASE(&pVolume);
}

void simpleaudio::Device::setVolume(float percent)
{
	HR_CHECK(pVolume->SetMasterVolumeLevelScalar(percent, NULL));
}

float simpleaudio::Device::getVolume()
{
	float percent;
	HR_CHECK(pVolume->GetMasterVolumeLevelScalar(&percent));
	return percent;
}

void simpleaudio::Device::getFriendlyName(wchar_t *friendlyName, int len)
{
	IPropertyStore *pProperties;
	PROPVARIANT prop;
	PropVariantInit(&prop);

	HR_CHECK(pDevice->OpenPropertyStore(STGM_READ, &pProperties));
	HR_CHECK(pProperties->GetValue(PKEY_Device_FriendlyName, &prop));
	wcscpy_s(friendlyName, len, prop.pwszVal);

	PropVariantClear(&prop);
	SAFE_RELEASE(&pProperties);
}

void simpleaudio::Device::sessionIterator(iterator::Iterator<Session> **ppSessionIterator)
{
	SessionIteratorProfile *itProf = new SessionIteratorProfile(this);
	*ppSessionIterator = new iterator::Iterator<Session>(itProf);
}

/* Session */
simpleaudio::Session::Session(IAudioSessionControl * pSession)
{
	pSession->AddRef();
	this->pSession = pSession;
	//Note: This isn't documented in the MSDN use of QueryInterface, but has multiple recent reports of reliable use *EYES-UP* do research and understand why this happens
	HR_CHECK(this->pSession->QueryInterface(IID_ISimpleAudioVolume, (void **)&pVolume));
}

simpleaudio::Session::~Session()
{
	SAFE_RELEASE(&this->pSession);
	SAFE_RELEASE(&this->pVolume);
}

void simpleaudio::Session::setVolume(float percent)
{
	HR_CHECK(pVolume->SetMasterVolume(percent, NULL));
}

float simpleaudio::Session::getVolume()
{
	float volume;
	HR_CHECK(pVolume->GetMasterVolume(&volume));
	return volume;
}

void simpleaudio::Session::getDisplayName(wchar_t *sessionName, int len)
{
	IAudioSessionControl2 *pSessionControl2;
	DWORD processId;
	HWND handle;
	HR_CHECK(pSession->QueryInterface(IID_IAudioSessionControl2, (void **)&pSessionControl2));
	if (pSessionControl2->IsSystemSoundsSession())
	{
		HR_CHECK(pSessionControl2->GetProcessId(&processId));
		handle = find_main_window(processId);
		*sessionName = L'\0';
		GetWindowText(handle, sessionName, len);
		if (*sessionName == L'\0')
		{
			wcscpy_s(sessionName, len, UNKNOWN.c_str());
		}
	}
	else
	{
		wcscpy_s(sessionName, len, SYS_SOUNDS.c_str());
	}

	SAFE_RELEASE(&pSessionControl2);
}

/* DeviceIteratorProfile */
simpleaudio::DeviceIteratorProfile::DeviceIteratorProfile(Interface *pInterface)
{
	HR_CHECK(pInterface->pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices));
}

simpleaudio::DeviceIteratorProfile::~DeviceIteratorProfile()
{
	SAFE_RELEASE(&pDevices);
}

void simpleaudio::DeviceIteratorProfile::get(unsigned int index, Device **ppDevice)
{
	IMMDevice *pMMDevice;
	HR_CHECK(pDevices->Item(index, &pMMDevice));
	*ppDevice = new Device(pMMDevice);
	SAFE_RELEASE(&pMMDevice);
}

unsigned int simpleaudio::DeviceIteratorProfile::count()
{
	unsigned int count;
	HR_CHECK(pDevices->GetCount(&count));
	return count;
}

/* SessionIteratorProfile */
simpleaudio::SessionIteratorProfile::SessionIteratorProfile(Device *pDevice)
{
	IAudioSessionManager2 *pSessionManager;
	HR_CHECK(pDevice->pDevice->Activate(IID_IAudioSessionManager2, CLSCTX_ALL, NULL, (void **)&pSessionManager));
	HR_CHECK(pSessionManager->GetSessionEnumerator(&pSessionEnumerator));
	SAFE_RELEASE(&pSessionManager);
}

simpleaudio::SessionIteratorProfile::~SessionIteratorProfile()
{
	SAFE_RELEASE(&pSessionEnumerator);
}

void simpleaudio::SessionIteratorProfile::get(unsigned int index, Session **ppSession)
{
	IAudioSessionControl *pSessionControl;
	HR_CHECK(pSessionEnumerator->GetSession((int)index, &pSessionControl));
	*ppSession = new Session(pSessionControl);
	SAFE_RELEASE(&pSessionControl);
}

unsigned int simpleaudio::SessionIteratorProfile::count()
{
	int count;
	HR_CHECK(pSessionEnumerator->GetCount(&count));
	return static_cast<unsigned int>(count);
}

/* helper-functions */
BOOL simpleaudio::is_main_window(HWND handle) {
	WCHAR sessionName[200] = { '\0' };
	int len = GetWindowText(handle, sessionName, 200);
	//printf("FUCKIN WHAT: %S\n", sessionName);
	return GetWindow(handle, GW_OWNER) == (HWND)0 && len > 0;// && IsWindowVisible(handle);
}

BOOL CALLBACK simpleaudio::enum_windows_callback(HWND handle, LPARAM lParam)
{
	handle_data& data = *(handle_data*)lParam;
	DWORD process_id = 0;
	GetWindowThreadProcessId(handle, &process_id);
	if (data.process_id != process_id || !is_main_window(handle))
	{
		return TRUE;
	}

	data.best_handle = handle;
	return FALSE;
}

HWND simpleaudio::find_main_window(DWORD process_id)
{
	handle_data data;
	data.process_id = process_id;
	data.best_handle = 0;
	EnumWindows(enum_windows_callback, (LPARAM)&data);
	return data.best_handle;
}

/* demo-function */
HRESULT simpleaudio::demo()
{
	/* Initilization */
	const int textlen = 200;
	std::chrono::seconds twosecs(2);

	/* Create instance of mmdeviceenumerator of pEnumerator */
	cout << "Hey, good start." << endl;
	cout << "Now try importing mmdeviceapi.h and create an instance of MMDeviceEnumerator." << endl;

	Interface *pInterface = new Interface();

	iterator::Iterator<Device> *pDeviceIterator;
	pInterface->deviceIterator(&pDeviceIterator);

	cout << "Did you make the enumerator? Let's check..." << endl;
	cout << "\tTesting: If pEnumerator was filled..." << ((pDeviceIterator != NULL) ? "SUCCESS" : "FAIL") << endl;

	cout << endl;

	/* Get the mmdevice that is default audio playback and it's friendly name. */
	cout << "Great, now lets get the primary mmdevice from the enumerator and ask for their name. You know, what you listen music with." << endl;

	Device *pDevice;
	pInterface->getDefaultDevice(&pDevice);
	wchar_t deviceName[textlen];
	pDevice->getFriendlyName(deviceName, textlen);


	cout << "Were you able to discover the default mmdevice? Let's check..." << endl;
	cout << "\tTesting: If pDevice was filled..." << ((pDevice != NULL) ? "SUCCESS" : "FAIL") << endl;
	wcout << L"\tTheir name is..." << deviceName << L"\n";

	/* Test the master volume change on the default mmdevices audioendpointvolume */
	cout << "Excellent! Now let's just try to change the master volume of the Speakers." << endl;
	cout << "In particular, try setting the volume to 20/100 for 5 seconds and then back to 100/100." << endl;

	pDevice->setVolume(0.2f); // 20% volume
	cout << "...waiting for 2 seconds..." << endl;
	std::this_thread::sleep_for(twosecs);
	cout << "...finished waiting..." << endl;
	pDevice->setVolume(1.0f); //full volume

	cout << "Did you hear the volume change? If not just tinker around till you hear the change." << endl;

	cout << endl;

	/*  */
	cout << "Alright now let's try to change the volume of a random session using the default device. Like Spotify or Chrome." << endl;
	cout << "First well get the SessionEnumerator from a SessionManager2." << endl;
	cout << "Great! Now lets try getting the names of active sessions in the same block." << endl;
	cout << "\tThere's a problem with Discord. The returned Process ID doesn't correspond with any top-level or children of top-level windows. This is unintended behavior, find out why." << endl;
	cout << "Great! Now lets try getting the icons of active sessions in the same block. THey may work, but for now lets wait till the C# portion." << endl;
	cout << endl;

	iterator::Iterator<Session> *pSessionIterator;
	pDevice->sessionIterator(&pSessionIterator);
	while (pSessionIterator->hasNext())
	{
		Session *pSession;
		pSessionIterator->next(&pSession);
		wchar_t sessionName[textlen];
		pSession->getDisplayName(sessionName, textlen);

		pSession->setVolume(0.2f);
		wcout << L"Setting Session: " << sessionName << L" to 20% volume..." << L"\n";
		cout << "...waiting for 2 seconds..." << endl;
		std::this_thread::sleep_for(twosecs);
		cout << "...finished waiting..." << endl;
		pSession->setVolume(1.0f);
		wcout << L"Setting Session: " << sessionName << L" to 100% volume..." << L"\n";
		cout << endl;

		delete pSession;
	}

	cout << "This is also a listening part, tyring opening the mixer to check if the changes are taking effect and listen if they are too." << endl;

	cout << endl;

	/* Release/Clear/Uninitialize */
	delete pSessionIterator;
	delete pDevice;
	delete pDeviceIterator;
	delete pInterface;

	return 0;
}