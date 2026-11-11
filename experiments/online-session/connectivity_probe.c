/* Read Windows connectivity classifications and adapter counts, without addresses or account data */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <winsock2.h>
#include <windows.h>
#include <initguid.h>
#include <iphlpapi.h>
#include <netlistmgr.h>
#include <roapi.h>
#include <winstring.h>
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include <windows.foundation.h>
#define WIDL_using_Windows_Networking_Connectivity
#include <windows.networking.connectivity.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

int main(void)
{
    INetworkListManager *manager = NULL;
    INetworkInformationStatics *statics = NULL;
    IConnectionProfile *profile = NULL;
    HSTRING name = NULL;
    NLM_CONNECTIVITY flags = 0;
    VARIANT_BOOL internet = 0, connected = 0;
    NetworkConnectivityLevel level = 0;
    HRESULT hr;
    ULONG size = 16384, error;
    IP_ADAPTER_ADDRESSES *adapters = malloc(size), *adapter;
    unsigned int count = 0, ipv4 = 0, ipv6 = 0, gateways = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    hr = RoInitialize(RO_INIT_MULTITHREADED);
    printf("RoInitialize hr=0x%08lx\n", (ULONG)hr);
    hr = CoCreateInstance(&CLSID_NetworkListManager, NULL, CLSCTX_INPROC_SERVER, &IID_INetworkListManager, (void **)&manager);
    printf("NetworkListManager hr=0x%08lx\n", (ULONG)hr);
    if (SUCCEEDED(hr)) {
        hr = INetworkListManager_GetConnectivity(manager, &flags);
        printf("GetConnectivity hr=0x%08lx flags=0x%08x\n", (ULONG)hr, flags);
        hr = INetworkListManager_IsConnectedToInternet(manager, &internet);
        printf("IsConnectedToInternet hr=0x%08lx value=%d\n", (ULONG)hr, internet);
        hr = INetworkListManager_IsConnected(manager, &connected);
        printf("IsConnected hr=0x%08lx value=%d\n", (ULONG)hr, connected);
    }
    const WCHAR *class_name = L"Windows.Networking.Connectivity.NetworkInformation";
    hr = WindowsCreateString(class_name, wcslen(class_name), &name);
    if (FAILED(hr)) return 2;
    hr = RoGetActivationFactory(name, &IID_INetworkInformationStatics, (void **)&statics);
    printf("NetworkInformationFactory hr=0x%08lx\n", (ULONG)hr);
    if (SUCCEEDED(hr)) {
        hr = INetworkInformationStatics_GetInternetConnectionProfile(statics, &profile);
        printf("GetInternetConnectionProfile hr=0x%08lx present=%d\n", (ULONG)hr, profile != NULL);
        if (profile) {
            hr = IConnectionProfile_GetNetworkConnectivityLevel(profile, &level);
            printf("GetNetworkConnectivityLevel hr=0x%08lx level=%u\n", (ULONG)hr, (unsigned)level);
            IConnectionProfile_Release(profile);
        }
        INetworkInformationStatics_Release(statics);
    }
    error = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS, NULL, adapters, &size);
    if (error == ERROR_BUFFER_OVERFLOW) {
        free(adapters);
        adapters = malloc(size);
        if (!adapters) return 3;
        error = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS, NULL, adapters, &size);
    }
    if (!error) for (adapter = adapters; adapter; adapter = adapter->Next) {
        ++count;
        for (IP_ADAPTER_UNICAST_ADDRESS *item = adapter->FirstUnicastAddress; item; item = item->Next) {
            if (item->Address.lpSockaddr->sa_family == AF_INET) ++ipv4;
            if (item->Address.lpSockaddr->sa_family == AF_INET6) ++ipv6;
        }
        for (IP_ADAPTER_GATEWAY_ADDRESS *item = adapter->FirstGatewayAddress; item; item = item->Next) ++gateways;
    }
    printf("GetAdaptersAddresses error=%lu adapters=%u ipv4=%u ipv6=%u gateways=%u\n", error, count, ipv4, ipv6, gateways);
    free(adapters);
    if (manager) INetworkListManager_Release(manager);
    WindowsDeleteString(name);
    RoUninitialize();
    Sleep(1000);
    return 0;
}
