/* Initialize render clients without starting playback, no capture or microphone access */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <stdio.h>

/* MinGW declares the KS GUID without storage, use the IEEE-float subtype defined in ksmedia.h */
static const GUID float_format = {WAVE_FORMAT_IEEE_FLOAT, 0, 0x0010, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};

int main(void)
{
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice *device = NULL;
    IAudioClient *last = NULL;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    unsigned failures = 0;
    if (FAILED(hr)) { printf("CoInitializeEx 0x%08lx\n", (unsigned long)hr); return 2; }
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&enumerator);
    if (FAILED(hr)) { printf("enumerator 0x%08lx\n", (unsigned long)hr); return 2; }
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
    if (FAILED(hr)) { printf("default render device 0x%08lx\n", (unsigned long)hr); return 2; }
    const unsigned channels[] = {2, 8, 12, 8, 12};
    const unsigned rates[] = {44100, 44100, 44100, 48000, 48000};
    const DWORD masks[] = {3, 0x63f, 0x2d63f, 0x63f, 0x2d63f};
    for (unsigned i = 0; i < 5; i++) {
        IAudioClient *client = NULL;
        WAVEFORMATEXTENSIBLE format = {0};
        WAVEFORMATEX *closest = NULL;
        UINT32 frames = 0;
        hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void **)&client);
        if (FAILED(hr)) { printf("activate 0x%08lx\n", (unsigned long)hr); failures++; continue; }
        format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        format.Format.nChannels = channels[i];
        format.Format.nSamplesPerSec = rates[i];
        format.Format.wBitsPerSample = 32;
        format.Format.nBlockAlign = channels[i] * sizeof(float);
        format.Format.nAvgBytesPerSec = rates[i] * format.Format.nBlockAlign;
        format.Format.cbSize = sizeof(format) - sizeof(format.Format);
        format.Samples.wValidBitsPerSample = 32;
        format.dwChannelMask = masks[i];
        format.SubFormat = float_format;
        hr = IAudioClient_IsFormatSupported(client, AUDCLNT_SHAREMODE_SHARED, &format.Format, &closest);
        printf("format channels=%u rate=%u mask=0x%lx hr=0x%08lx\n", channels[i], rates[i], (unsigned long)masks[i], (unsigned long)hr);
        if (hr != S_OK) failures++;
        if (closest) CoTaskMemFree(closest);
        hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, &format.Format, NULL);
        printf("initialize channels=%u rate=%u hr=0x%08lx\n", channels[i], rates[i], (unsigned long)hr);
        if (FAILED(hr)) failures++;
        else {
            hr = IAudioClient_GetBufferSize(client, &frames);
            if (FAILED(hr) || !frames) failures++;
            printf("buffer frames=%u hr=0x%08lx\n", frames, (unsigned long)hr);
        }
        if (last) IAudioClient_Release(last);
        last = client;
    }
    printf("RESULT: %s (%u failures, no playback started)\n", failures ? "FAIL" : "PASS", failures);
    fflush(stdout);
    Sleep(3000);
    if (last) IAudioClient_Release(last);
    IMMDevice_Release(device);
    IMMDeviceEnumerator_Release(enumerator);
    CoUninitialize();
    return failures ? 1 : 0;
}
