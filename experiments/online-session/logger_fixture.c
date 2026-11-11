/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Exported from the October 2026 FH6 experiments, see THIRD_PARTY_NOTICES.md
 */
/* Actual diagnostic helper extracted from the private source copy */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef wchar_t WCHAR;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uintptr_t ULONG_PTR;
#define ARRAY_SIZE(value) (sizeof(value)/sizeof((value)[0]))
#define wcsicmp wcscasecmp
#define wcsnicmp wcsncasecmp
#define WINHTTP_HANDLE_TYPE_REQUEST 3
struct object_header { DWORD type; void *handle, *callback; DWORD notify_mask; };
struct request { struct object_header hdr; const char *fh6_diag_operation, *fh6_diag_family; };
static DWORD last_error;
static int enabled_for_test, records_for_test;
static DWORD GetLastError(void) { return last_error; }
static void SetLastError(DWORD value) { last_error=value; }
static DWORD GetEnvironmentVariableA(const char *name,char *value,DWORD count) {
    assert(!strcmp(name,"FH6_HTTP_DIAG") && count==2);
    value[0]=enabled_for_test?'1':'0'; value[1]=0; last_error=999; return 1;
}
static LONG InterlockedCompareExchange(LONG *target,LONG value,LONG expected) {
    return __sync_val_compare_and_swap(target,expected,value);
}
static LONG InterlockedIncrement(LONG *target) { return __sync_add_and_fetch(target,1); }
#define MESSAGE(...) do { ++records_for_test; last_error=888; } while (0)
/* Only constant API names and numeric results are emitted, never URL parameters, headers or bodies */
static const char *fh6_http_operation( const WCHAR *path )
{
    static const struct { const WCHAR *wide; const char *name; } routes[] = {
        {L"/Authentication/GetEntityToken", "/Authentication/GetEntityToken"},
        {L"/Authentication/ValidateEntityToken", "/Authentication/ValidateEntityToken"},
        {L"/Client/AddFriend", "/Client/AddFriend"},
        {L"/Client/AddOrUpdateContactEmail", "/Client/AddOrUpdateContactEmail"},
        {L"/Client/DeletePlayerCustomProperties", "/Client/DeletePlayerCustomProperties"},
        {L"/Client/ExecuteCloudScript", "/Client/ExecuteCloudScript"},
        {L"/Client/GetAccountInfo", "/Client/GetAccountInfo"},
        {L"/Client/GetFriendsList", "/Client/GetFriendsList"},
        {L"/Client/GetPlayFabIDsFromBattleNetAccountIds", "/Client/GetPlayFabIDsFromBattleNetAccountIds"},
        {L"/Client/GetPlayFabIDsFromFacebookIDs", "/Client/GetPlayFabIDsFromFacebookIDs"},
        {L"/Client/GetPlayFabIDsFromFacebookInstantGamesIds", "/Client/GetPlayFabIDsFromFacebookInstantGamesIds"},
        {L"/Client/GetPlayFabIDsFromGameCenterIDs", "/Client/GetPlayFabIDsFromGameCenterIDs"},
        {L"/Client/GetPlayFabIDsFromGoogleIDs", "/Client/GetPlayFabIDsFromGoogleIDs"},
        {L"/Client/GetPlayFabIDsFromGooglePlayGamesPlayerIDs", "/Client/GetPlayFabIDsFromGooglePlayGamesPlayerIDs"},
        {L"/Client/GetPlayFabIDsFromKongregateIDs", "/Client/GetPlayFabIDsFromKongregateIDs"},
        {L"/Client/GetPlayFabIDsFromNintendoServiceAccountIds", "/Client/GetPlayFabIDsFromNintendoServiceAccountIds"},
        {L"/Client/GetPlayFabIDsFromNintendoSwitchDeviceIds", "/Client/GetPlayFabIDsFromNintendoSwitchDeviceIds"},
        {L"/Client/GetPlayFabIDsFromPSNAccountIDs", "/Client/GetPlayFabIDsFromPSNAccountIDs"},
        {L"/Client/GetPlayFabIDsFromPSNOnlineIDs", "/Client/GetPlayFabIDsFromPSNOnlineIDs"},
        {L"/Client/GetPlayFabIDsFromSteamIDs", "/Client/GetPlayFabIDsFromSteamIDs"},
        {L"/Client/GetPlayFabIDsFromSteamNames", "/Client/GetPlayFabIDsFromSteamNames"},
        {L"/Client/GetPlayFabIDsFromTwitchIDs", "/Client/GetPlayFabIDsFromTwitchIDs"},
        {L"/Client/GetPlayFabIDsFromXboxLiveIDs", "/Client/GetPlayFabIDsFromXboxLiveIDs"},
        {L"/Client/GetPlayerCombinedInfo", "/Client/GetPlayerCombinedInfo"},
        {L"/Client/GetPlayerCustomProperty", "/Client/GetPlayerCustomProperty"},
        {L"/Client/GetPlayerProfile", "/Client/GetPlayerProfile"},
        {L"/Client/GetPlayerSegments", "/Client/GetPlayerSegments"},
        {L"/Client/GetPlayerTags", "/Client/GetPlayerTags"},
        {L"/Client/GetPublisherData", "/Client/GetPublisherData"},
        {L"/Client/GetTime", "/Client/GetTime"},
        {L"/Client/GetTitleData", "/Client/GetTitleData"},
        {L"/Client/GetTitleNews", "/Client/GetTitleNews"},
        {L"/Client/GetUserData", "/Client/GetUserData"},
        {L"/Client/GetUserPublisherData", "/Client/GetUserPublisherData"},
        {L"/Client/GetUserPublisherReadOnlyData", "/Client/GetUserPublisherReadOnlyData"},
        {L"/Client/GetUserReadOnlyData", "/Client/GetUserReadOnlyData"},
        {L"/Client/LinkBattleNetAccount", "/Client/LinkBattleNetAccount"},
        {L"/Client/LinkCustomID", "/Client/LinkCustomID"},
        {L"/Client/LinkOpenIdConnect", "/Client/LinkOpenIdConnect"},
        {L"/Client/LinkSteamAccount", "/Client/LinkSteamAccount"},
        {L"/Client/LinkXboxAccount", "/Client/LinkXboxAccount"},
        {L"/Client/ListPlayerCustomProperties", "/Client/ListPlayerCustomProperties"},
        {L"/Client/LoginWithBattleNet", "/Client/LoginWithBattleNet"},
        {L"/Client/LoginWithCustomID", "/Client/LoginWithCustomID"},
        {L"/Client/LoginWithOpenIdConnect", "/Client/LoginWithOpenIdConnect"},
        {L"/Client/LoginWithSteam", "/Client/LoginWithSteam"},
        {L"/Client/LoginWithXbox", "/Client/LoginWithXbox"},
        {L"/Client/RemoveContactEmail", "/Client/RemoveContactEmail"},
        {L"/Client/RemoveFriend", "/Client/RemoveFriend"},
        {L"/Client/ReportPlayer", "/Client/ReportPlayer"},
        {L"/Client/SetFriendTags", "/Client/SetFriendTags"},
        {L"/Client/UnlinkBattleNetAccount", "/Client/UnlinkBattleNetAccount"},
        {L"/Client/UnlinkCustomID", "/Client/UnlinkCustomID"},
        {L"/Client/UnlinkOpenIdConnect", "/Client/UnlinkOpenIdConnect"},
        {L"/Client/UnlinkSteamAccount", "/Client/UnlinkSteamAccount"},
        {L"/Client/UnlinkXboxAccount", "/Client/UnlinkXboxAccount"},
        {L"/Client/UpdateAvatarUrl", "/Client/UpdateAvatarUrl"},
        {L"/Client/UpdatePlayerCustomProperties", "/Client/UpdatePlayerCustomProperties"},
        {L"/Client/UpdateUserData", "/Client/UpdateUserData"},
        {L"/Client/UpdateUserPublisherData", "/Client/UpdateUserPublisherData"},
        {L"/Client/UpdateUserTitleDisplayName", "/Client/UpdateUserTitleDisplayName"},
        {L"/Lobby/CreateLobby", "/Lobby/CreateLobby"},
        {L"/Lobby/DeleteLobby", "/Lobby/DeleteLobby"},
        {L"/Lobby/FindFriendLobbies", "/Lobby/FindFriendLobbies"},
        {L"/Lobby/FindLobbies", "/Lobby/FindLobbies"},
        {L"/Lobby/GetLobby", "/Lobby/GetLobby"},
        {L"/Lobby/InviteToLobby", "/Lobby/InviteToLobby"},
        {L"/Lobby/JoinArrangedLobby", "/Lobby/JoinArrangedLobby"},
        {L"/Lobby/JoinLobby", "/Lobby/JoinLobby"},
        {L"/Lobby/JoinLobbyAsServer", "/Lobby/JoinLobbyAsServer"},
        {L"/Lobby/LeaveLobby", "/Lobby/LeaveLobby"},
        {L"/Lobby/LeaveLobbyAsServer", "/Lobby/LeaveLobbyAsServer"},
        {L"/Lobby/RemoveMember", "/Lobby/RemoveMember"},
        {L"/Lobby/SubscribeToLobbyResource", "/Lobby/SubscribeToLobbyResource"},
        {L"/Lobby/UnsubscribeFromLobbyResource", "/Lobby/UnsubscribeFromLobbyResource"},
        {L"/Lobby/UpdateLobby", "/Lobby/UpdateLobby"},
        {L"/Lobby/UpdateLobbyAsServer", "/Lobby/UpdateLobbyAsServer"},
        {L"/Match/CancelAllMatchmakingTicketsForPlayer", "/Match/CancelAllMatchmakingTicketsForPlayer"},
        {L"/Match/CancelAllServerBackfillTicketsForPlayer", "/Match/CancelAllServerBackfillTicketsForPlayer"},
        {L"/Match/CancelMatchmakingTicket", "/Match/CancelMatchmakingTicket"},
        {L"/Match/CancelServerBackfillTicket", "/Match/CancelServerBackfillTicket"},
        {L"/Match/CreateMatchmakingTicket", "/Match/CreateMatchmakingTicket"},
        {L"/Match/CreateServerBackfillTicket", "/Match/CreateServerBackfillTicket"},
        {L"/Match/GetMatch", "/Match/GetMatch"},
        {L"/Match/GetMatchmakingTicket", "/Match/GetMatchmakingTicket"},
        {L"/Match/GetServerBackfillTicket", "/Match/GetServerBackfillTicket"},
        {L"/Match/JoinMatchmakingTicket", "/Match/JoinMatchmakingTicket"},
        {L"/Match/SubscribeToMatchmakingResource", "/Match/SubscribeToMatchmakingResource"},
        {L"/Match/UnsubscribeFromMatchmakingResource", "/Match/UnsubscribeFromMatchmakingResource"},
        {L"/MultiplayerServer/ListBuildAliases", "/MultiplayerServer/ListBuildAliases"},
        {L"/MultiplayerServer/ListBuildSummariesV2", "/MultiplayerServer/ListBuildSummariesV2"},
        {L"/MultiplayerServer/ListQosServersForTitle", "/MultiplayerServer/ListQosServersForTitle"},
        {L"/MultiplayerServer/RequestMultiplayerServer", "/MultiplayerServer/RequestMultiplayerServer"},
    };
    unsigned int i;
    if (!path) return "other";
    for (i = 0; i < ARRAY_SIZE(routes); ++i)
    {
        size_t length = wcslen( routes[i].wide );
        if (!wcsnicmp( path, routes[i].wide, length ) &&
            (!path[length] || path[length] == '?' || path[length] == '#')) return routes[i].name;
    }
    return "other";
}

static const char *fh6_http_family( const WCHAR *hostname )
{
    static const struct { const WCHAR *suffix; const char *name; } families[] = {
        {L"playfabapi.com", "playfab"}, {L"playfab.com", "playfab"},
        {L"xboxlive.com", "xbox"}, {L"xboxservices.com", "xbox"},
        {L"live.com", "live"}, {L"microsoft.com", "microsoft"},
        {L"forzamotorsport.net", "forza"}, {L"forza.net", "forza"},
        {L"steampowered.com", "steam"}, {L"steamcommunity.com", "steam"},
        {L"127.0.0.1", "loopback"}
    };
    size_t length, suffix_length;
    unsigned int i;
    if (!hostname) return "other";
    length = wcslen( hostname );
    for (i = 0; i < ARRAY_SIZE(families); ++i)
    {
        suffix_length = wcslen( families[i].suffix );
        if (length >= suffix_length && !wcsicmp( hostname + length - suffix_length, families[i].suffix ) &&
            (length == suffix_length || hostname[length - suffix_length - 1] == '.')) return families[i].name;
    }
    return "other";
}

void fh6_http_diag( struct object_header *hdr, const char *event, DWORD code, DWORD error, ULONG_PTR api )
{
    static LONG enabled = -1, records;
    DWORD saved_error = GetLastError();
    LONG active = InterlockedCompareExchange( &enabled, -1, -1 );
    const char *family = "other", *operation = "other";
    if (active == -1)
    {
        char value[2];
        active = GetEnvironmentVariableA( "FH6_HTTP_DIAG", value, sizeof(value) ) == 1 && value[0] == '1';
        InterlockedCompareExchange( &enabled, active, -1 );
    }
    if (active && InterlockedIncrement( &records ) <= 8192)
    {
        if (hdr->type == WINHTTP_HANDLE_TYPE_REQUEST)
        {
            struct request *request = (struct request *)hdr;
            if (request->fh6_diag_family) family = request->fh6_diag_family;
            if (request->fh6_diag_operation) operation = request->fh6_diag_operation;
        }
        MESSAGE( "FH6_HTTP_DIAG pid=%lu tid=%lu tick=%llu handle=%p type=%lu event=%s code=%lu error=%lu api=%Iu callback=%u family=%s operation=%s\n",
                 GetCurrentProcessId(), GetCurrentThreadId(), GetTickCount64(), hdr->handle, hdr->type,
                 event, code, error, api, !!(!strcmp( event, "callback" ) && hdr->callback && (hdr->notify_mask & code)), family, operation );
    }
    SetLastError( saved_error );
}


int main(int argc,char **argv) {
    enabled_for_test=argc>1 && atoi(argv[1]);
    assert(!strcmp(fh6_http_operation(L"/Match/CreateMatchmakingTicket?token=SECRET"),"/Match/CreateMatchmakingTicket"));
    assert(!strcmp(fh6_http_operation(L"/Match/CreateMatchmakingTicketSECRET"),"other"));
    assert(!strcmp(fh6_http_operation(L"/UNKNOWN_SECRET"),"other"));
    assert(!strcmp(fh6_http_operation(NULL),"other"));
    assert(!strcmp(fh6_http_family(L"title.playfabapi.com"),"playfab"));
    assert(!strcmp(fh6_http_family(L"secretplayfabapi.com"),"other"));
    assert(!strcmp(fh6_http_family(L"playfabapi.com.secret.test"),"other"));
    assert(!strcmp(fh6_http_family(NULL),"other"));
    struct request request={{WINHTTP_HANDLE_TYPE_REQUEST,NULL,NULL,0},"/Match/GetMatchmakingTicket","playfab"};
    for(int index=0;index<8300;++index) {
        last_error=12345;
        fh6_http_diag(&request.hdr,"callback",0,10060,1);
        assert(last_error==12345);
    }
    assert(records_for_test==(enabled_for_test?8192:0));
    printf("PASS enabled=%d emitted=%d last_error=%u\n",enabled_for_test,records_for_test,last_error);
    return 0;
}
