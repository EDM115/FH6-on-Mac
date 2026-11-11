# Checked Wine patch sequence

These patches target the Wine source in the official [CrossOver 26.3.0 source archive](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz). Archive SHA-256 : `ac99c8ca4b3848f3e81784135f023df266b61c2345726ea55a50b3e030dd6872`  
[series.json](series.json) records the order, patch hashes and before/after source hashes for every step. The verification pass applies all nine patches to scratch copies and compares all 13 resulting files with the tested lab sources. A zero exit from BSD patch alone was insufficient in an earlier backport, one test hunk had been skipped

| Patch | Purpose and origin |
| --- | --- |
| 0003 | Search `WINEDLLPATH_PREPEND` before default builtins, Highball-engine blob `60b1c807ee7e0f5ac64ba51404764669a3f0dcae` |
| 0014 | Paired msync wait-registration cleanup, Highball-engine blob `474c82cc40b3b12028b625c2ce915e9094782fa5` |
| 0018 | Mach footprint accounting, Highball-engine blob `84c62bf9a4c16732c544005e5de7e81d386b3231` |
| 0019 | Preserve registry credential matches when Keychain has none, Highball-engine patch 0019 |
| 0022 | Static spatial-object channel order, Highball-engine blob `b78e0a2968dfd63831554de76aa83db0e2acfc17` |
| 0023 | CoreAudio spatial mixer for surround beds, Highball-engine blob `30aaed1c17fee56927a374326528985cd409938c` |
| 0024 | SIO_UDP_NETRESET compatibility backport from Wine commit `19d5fbd382ccef3e5dc929b661dae63c10ec4f82`, this did not fix FH6 online sessions |
| 0025 | Local opt-in TTL255 workaround, consolidated with the later one-byte V6ONLY correction from lab patch 0027 |
| winhttp-metadata-diagnostic | Local bounded metadata diagnostic, retained in the tested DLL but disabled by `FH6_HTTP_DIAG=0` in the quiet baseline |

The active 0025 patch already includes `BYTE v6only = 1`. Do not apply the archived 0027 correction to it again. The historical numeric-audit patch is outside this sequence and is not part of the working quiet DLL  
Use a separate extracted source copy, not an installed runtime :

```zsh
python3 tools/prepare_wine_patches.py --source .local/wine-source
python3 tools/prepare_wine_patches.py --source .local/wine-source --apply
```

The first command only checks. The second writes the selected source copy after the complete scratch pass succeeds. It refuses changed inputs and accepts an already-complete matching tree without applying twice. It does not build or install components  
These patches and their Wine-derived context retain LGPL terms. See [third-party notices](../../THIRD_PARTY_NOTICES.md) and [component rebuild instructions](../../docs/wine-components.md)
