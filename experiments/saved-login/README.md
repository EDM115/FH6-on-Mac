# Saved Xbox sign-in, 24 October 2026

A clean unpatched relaunch asked for Xbox sign-in again. The synthetic [credential_filter.c](credential_filter.c) probe writes one uniquely named generic fixture, reads that exact fixture, tests matching and missing prefixes, then deletes its own fixture. It does not enumerate all credentials or read Xbox tokens

| Component | Matching-prefix result |
| --- | --- |
| Shipped CrossOver advapi32 | Failure, ERROR_NOT_FOUND 1168, count 1 |
| Matched unpatched source build | Same failure |
| Source build with patch 0019 | Success, count 1, correct synthetic value |

Both controls could read the exact name. The missing prefix remained a failure with count 0 after the patch. A successful Win32 call can leave an earlier LastError value behind, that value is defined only on failure  
[Patch 0019](../../patches/wine/0019-advapi32-credenumerate-empty-keychain-is-not-an-error.patch) preserves registry matches when the separate Keychain query has no matches. The rebuilt DLL kept all 587 export names/ordinals. Three clean patched game relaunches remembered sign-in, the third also enabled the vertex-layout fix with the same advapi32 bytes  
The public probe builds as `.local/build/probes/credential-filter.exe`. Use a separate account-free bottle for the synthetic API check. Same-day relaunch persistence does not establish every later token expiry or system-restart case
