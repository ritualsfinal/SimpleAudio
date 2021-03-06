#include "stdafx.h"

#include "iterator.h"

#include "simpleaudio.win.h"
#include "iterator-impl.win.h"
#include "winconstants.hpp"
#include "winhelpers.h"
#include "wincallbacks.h"

#include <functiondiscoverykeys_devpkey.h>

#include <string>
#include <iostream>

#define HR_CHECK(hr) winhelpers::HR_CHECK(hr)
#define SAFE_RELEASE(pUnk) winhelpers::SAFE_RELEASE(pUnk)

/* Device Interface */
simpleaudio_win::Interface::Interface()
{
	//Note: To use Compoment Objects (Co's) you have to initialize the COM library on the current thread before they can be used."
	winhelpers::write_text_to_log_file("FIRST\n");
	//HR_CHECK(CoInitialize(NULL));
	CoInitialize(NULL);
	winhelpers::write_text_to_log_file("SECOND\n");
	HR_CHECK(CoCreateInstance(winconstants::CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, winconstants::IID_IMMDeviceEnumerator, (void **)&pEnumerator));
	winhelpers::write_text_to_log_file("THIRD\n");
	pDevices = new map<IMMDevice *, simpleaudio::IDevice *>();
	winhelpers::write_text_to_log_file("FOURTH\n");
}

simpleaudio_win::Interface::~Interface()
{
	destroyWrappers();
	if(pDevices) delete pDevices;
	SAFE_RELEASE(&pEnumerator);
	CoUninitialize();
}

EFLAG simpleaudio_win::Interface::getDefaultDevice(simpleaudio::IDevice **ppDevice)
{
	try
	{
		*ppDevice = fetchDefaultDevice();
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Interface::isDefaultDevice(simpleaudio::IDevice *pDevice, bool *pRet)
{
	try
	{
		*pRet = (pDevice == fetchDefaultDevice());
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Interface::deviceIterator(simpleaudio::IIterator<simpleaudio::IDevice *> **ppDeviceIterator)
{
	try
	{
		generateWrappers();
		iterator::MapValueIteratorProfile<IMMDevice *, simpleaudio::IDevice *> *itProf = new iterator::MapValueIteratorProfile<IMMDevice *, simpleaudio::IDevice *>(pDevices);
		*ppDeviceIterator = new iterator::Iterator<simpleaudio::IDevice *>(itProf);
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

void simpleaudio_win::Interface::generateWrappers()
{
	iteratorprofiles_win::MMDeviceIteratorProfile *itProf = new iteratorprofiles_win::MMDeviceIteratorProfile(pEnumerator);
	iterator::Iterator<IMMDevice *> *it = new iterator::Iterator<IMMDevice *>(itProf);
	while (it->hasNext())
	{
		IMMDevice *pMMDevice;
		it->next(&pMMDevice);
		generateWrapper(pMMDevice);
	}
	delete it;
}

void simpleaudio_win::Interface::destroyWrappers()
{
	std::map<IMMDevice *, simpleaudio::IDevice *>::iterator it;
	for (it = pDevices->begin(); it != pDevices->end(); ++it)
	{
		delete it->second;
		CoTaskMemFree(it->first);
	}
}

simpleaudio::IDevice * simpleaudio_win::Interface::generateWrapper(IMMDevice *pMMDevice)
{
	if (pDevices->find(pMMDevice) == pDevices->end())
	{
		(*pDevices)[pMMDevice] = new Device(pMMDevice);
	}
	return pDevices->at(pMMDevice);
}

simpleaudio::IDevice * simpleaudio_win::Interface::fetchDefaultDevice()
{
	simpleaudio::IDevice *pDevice;
	IMMDevice *pMMDevice = NULL;
	HR_CHECK(pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pMMDevice));
	pDevice = generateWrapper(pMMDevice);
	SAFE_RELEASE(&pMMDevice);
	return pDevice;
}

/* Device */
simpleaudio_win::Device::Device(IMMDevice * pMMDevice)
{
	pMMDevice->AddRef();
	this->pDevice = pMMDevice;
	HR_CHECK(pDevice->Activate(winconstants::IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void **)&pVolume));
	HR_CHECK(pDevice->OpenPropertyStore(STGM_READ, &pProperties));
	pSessions = new map<IAudioSessionControl *, simpleaudio::ISession *>();

}

simpleaudio_win::Device::~Device()
{
	destroyWrappers();
	if(pSessions) delete pSessions;
	SAFE_RELEASE(&pProperties);
	SAFE_RELEASE(&pVolume);
	SAFE_RELEASE(&pDevice);
}

EFLAG simpleaudio_win::Device::setVolume(float percent)
{
	try
	{
		HR_CHECK(pVolume->SetMasterVolumeLevelScalar(percent, &winconstants::program_id));
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Device::getVolume(float *pRet)
{
	try
	{
		float percent;
		HR_CHECK(pVolume->GetMasterVolumeLevelScalar(&percent));
		*pRet = percent;
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Device::mute(bool *pRet)
{
	try
	{
		bool muted = fetchMute();
		HR_CHECK(pVolume->SetMute(!muted, &winconstants::program_id));
		*pRet = !muted;
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Device::isMuted(bool *pRet)
{
	try
	{
		*pRet = fetchMute();
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Device::getName(wchar_t *name, int len)
{
	wstring *pDeviceName = NULL;
	try
	{
		pDeviceName = fetchName();
		wcscpy_s(name, len, pDeviceName->c_str());
		delete pDeviceName;
	}
	catch (EFLAG flag)
	{
		if (pDeviceName) delete pDeviceName;
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Device::sessionIterator(simpleaudio::IIterator<simpleaudio::ISession *> **ppSessionIterator)
{
	try
	{
		generateWrappers();
		iterator::MapValueIteratorProfile<IAudioSessionControl *, simpleaudio::ISession *> *itProf = new iterator::MapValueIteratorProfile<IAudioSessionControl *, simpleaudio::ISession *>(pSessions);
		*ppSessionIterator = new iterator::Iterator<simpleaudio::ISession *>(itProf);
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

void simpleaudio_win::Device::generateWrappers()
{
	iteratorprofiles_win::AudioSessionControlIteratorProfile *itProf = new iteratorprofiles_win::AudioSessionControlIteratorProfile(pDevice);
	iterator::Iterator<IAudioSessionControl *> *it = new iterator::Iterator<IAudioSessionControl *>(itProf);
	while (it->hasNext())
	{
		IAudioSessionControl *pSessionControl;
		it->next(&pSessionControl);
		generateWrapper(pSessionControl);
	}
	delete it;
}

void simpleaudio_win::Device::destroyWrappers()
{
	std::map<IAudioSessionControl *, simpleaudio::ISession *>::iterator it;
	for (it = pSessions->begin(); it != pSessions->end(); ++it)
	{
		delete it->second;
		IAudioSessionControl *first = it->first;
		SAFE_RELEASE(&first);
	}
}

simpleaudio::ISession * simpleaudio_win::Device::generateWrapper(IAudioSessionControl *sessionID)
{
	if( pSessions->find(sessionID) == pSessions->end())
	{
		(*pSessions)[sessionID] = new Session(sessionID);
	}
	return pSessions->at(sessionID);
}

float simpleaudio_win::Device::fetchVolume()
{
	float percent;
	HR_CHECK(pVolume->GetMasterVolumeLevelScalar(&percent));
	return percent;
}

bool simpleaudio_win::Device::fetchMute()
{
	BOOL muted;
	HR_CHECK(pVolume->GetMute(&muted));
	return (bool)muted;
}

wstring *simpleaudio_win::Device::fetchName()
{
	wstring *name;
	PROPVARIANT prop;
	PropVariantInit(&prop);
	try 
	{
		HR_CHECK(pProperties->GetValue(PKEY_Device_FriendlyName, &prop));
		name = new wstring(prop.pwszVal);
	}
	catch (EFLAG flag)
	{
		PropVariantClear(&prop);
		throw flag;
	}
	PropVariantClear(&prop);
	return name;
}

/* Session */
simpleaudio_win::Session::Session(IAudioSessionControl * pSession)
{
	pSession->AddRef();
	this->pSession = pSession;
	//Note: This isn't documented in the MSDN use of QueryInterface, but has multiple recent reports of reliable use *EYES-UP* do research and understand why this happens
	HR_CHECK(this->pSession->QueryInterface(winconstants::IID_ISimpleAudioVolume, (void **)&pVolume));
}

simpleaudio_win::Session::~Session()
{
	SAFE_RELEASE(&pSession);
	SAFE_RELEASE(&pVolume);
}

EFLAG simpleaudio_win::Session::setVolume(float percent)
{
	try
	{
		HR_CHECK(pVolume->SetMasterVolume(percent, NULL));
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Session::getVolume(float *pRet)
{
	try 
	{
		*pRet = fetchVolume();
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Session::mute(bool *pRet)
{
	try
	{
		bool muted = fetchMute();
		pVolume->SetMute(!muted, &winconstants::program_id);
		*pRet = !muted;
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Session::isMuted(bool *pRet)
{
	try
	{
		*pRet = fetchMute();
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

EFLAG simpleaudio_win::Session::getName(wchar_t *sessionName, int len)
{
	try
	{
		fetchName(sessionName,len);
	}
	catch (EFLAG flag)
	{
		return flag;
	}
	return S_OK;
}

float simpleaudio_win::Session::fetchVolume()
{
	float volume;
	HR_CHECK(pVolume->GetMasterVolume(&volume));
	return volume;
}

bool simpleaudio_win::Session::fetchMute()
{
	BOOL muted;
	HR_CHECK(pVolume->GetMute(&muted));
	return (bool)muted;
}


void simpleaudio_win::Session::fetchName(wchar_t *sessionName, int len)
{
	IAudioSessionControl2 *pSessionControl2;
	DWORD processId;
	HWND handle;
	HR_CHECK(pSession->QueryInterface(winconstants::IID_IAudioSessionControl2, (void **)&pSessionControl2));
	if (pSessionControl2->IsSystemSoundsSession())
	{
		HR_CHECK(pSessionControl2->GetProcessId(&processId));
		handle = winhelpers::find_main_window(processId);
		*sessionName = L'\0';
		GetWindowText(handle, sessionName, len);
		if (*sessionName == L'\0')
		{
			wcscpy_s(sessionName, len, winconstants::UNKNOWN.c_str());
		}
	}
	else
	{
		wcscpy_s(sessionName, len, winconstants::SYS_SOUNDS.c_str());
	}
	SAFE_RELEASE(&pSessionControl2);
}


/* Interface User */
simpleaudio::IInterface * APIENTRY GetSimpleAudioInterface()
{
	winhelpers::write_text_to_log_file("FUCK");
	return new simpleaudio_win::Interface();
}