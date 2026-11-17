# Device Enumeration Hardening

## Ownership and acceptance

- Owner: `@Dbq0917` (PortAudio fork maintainer)
- Backup: Audio Runtime on-call
- Acceptance deadline: 2026-11-20 18:00 Asia/Shanghai
- Release gate: the results below must be reproduced on the release commit before
  the change is promoted beyond the 25% cohort.

## What changed

- Host API initialization is isolated. A backend that returns an error or
  publishes an inconsistent host API/device table is skipped; healthy backends
  remain available in their original order.
- Null device tables, null device entries, null names, invalid counts, and
  invalid default indexes are rejected before they can reach `Pa_GetDeviceInfo`.
- The PulseAudio source/sink callbacks and ALSA device-name helpers now guard
  null callback payloads and allocation failures.
- `Pa_Initialize()` returns `paDeviceEnumerationFailed` if every configured
  backend fails, runs the enumeration self-check on success, and returns a
  partial-failure code through `Pa_GetLastDeviceEnumerationError()` when healthy
  backends survive.
- `Pa_RefreshDeviceList()` and `Pa_InvalidateDeviceCache()` provide an
  explicit hotplug/controller-thread refresh path. On Linux, a change to
  `/dev/snd` also marks the cache stale. Refreshes are deferred while streams
  are open.
- Enumeration is cached. The default age limit is 5000 ms and can be changed
  with `PA_DEVICE_CACHE_TTL_MS` (0 disables age-based refresh). Repeated
  enumeration does not rescan backends before the cache expires or is
  invalidated.
- Result order is never sorted or reordered. The host API initializer order and
  each backend's device order are preserved.

## Rollout and rollback

1. Canary: 5% of recording-tool installs for 24 hours.
2. Gate: no increase in startup crashes, `self_check=0`, and the normal-device
   output hash matches the baseline.
3. Expand to 25% for 24 hours, then 100%.

Rollback is runtime-only and takes effect after restarting the recording tool:

```sh
PA_DEVICE_ENUMERATION_MODE=compat
PA_DEVICE_ENUMERATION_LOG=0
PA_DEVICE_CACHE_TTL_MS=0
```

`compat` restores the pre-change initialization behavior. `PA_DEVICE_ENUMERATION_LOG=0`
disables enumeration diagnostics, and `PA_DEVICE_CACHE_TTL_MS=0` disables
age-based refreshes. Existing callers need no source changes.

## Listener integration

The existing native device listener should only set an atomic/queued "devices
changed" flag. Its controller thread then calls `Pa_InvalidateDeviceCache()`
(or `Pa_RefreshDeviceList()` when an immediate result is required), followed by
`Pa_GetDeviceCount()` and `Pa_GetDeviceInfo()`. Do not call refresh from a
signal handler or from the native listener callback itself. If a stream is open,
the refresh is deferred and the active list remains valid until the stream is
closed.

## Acceptance evidence

Environment date: 2026-11-18.

### Problem environment

Ubuntu 26.04 and Debian 12 were both run with an unavailable PulseAudio/JACK
backend and no physical sound card. The original build failed initialization;
the hardened build completed enumeration and exposed the remaining sound
server device.

| Distribution | Build | Initialize | Devices | Last enum error | Self-check |
|---|---|---:|---:|---:|---:|
| Ubuntu 26.04 | before | 241 (`-9999`) | n/a | n/a | n/a |
| Ubuntu 26.04 | after | 0 | 1 (`sndio/default`) | `-9999` | 0 |
| Debian 12 | before | 241 (`-9999`) | n/a | n/a | n/a |
| Debian 12 | after | 0 | 1 (`sndio/default`) | `-9999` | 0 |

The fault-injection test also publishes a deliberately malformed backend
(`deviceCount=1`, `deviceInfos=NULL`) and a failing backend. Both were skipped;
the valid devices remained in order.

### Normal environment

With healthy backends enabled, `pa_devs` output was byte-for-byte identical
before and after on both distributions:

```text
SHA-256 580d0c32377e1b077f7d6b6647a95b859cd0142d7690e38fa6feb2173b0c2b1b
```

The compared output includes the device count, names, host API, channel counts,
defaults, latencies, rates, and order. `cmp` returned 0 on Ubuntu 26.04 and
Debian 12.

### Hotplug and cache

The long-running fault-injection test performs:

1. Enumerate 3 devices, then repeatedly enumerate from cache.
2. Remove a device from the backend and call `Pa_InvalidateDeviceCache()`.
3. Observe 3 to 2 devices without restarting PortAudio.
4. Insert the device and call `Pa_RefreshDeviceList()`.
5. Observe 2 to 3 devices with the original order restored.

The test passes under AddressSanitizer and UndefinedBehaviorSanitizer.

### Performance and low-resource runs

Five healthy-environment `pa_devs` process runs before and after both had a
median wall time of 0.02 s and a peak RSS median of 26624 KiB on Ubuntu 26.04
(no measurable process-level regression).

Ubuntu 26.04, normal run:

```text
devices=1 last_enum_error=-9999 self_check=0 init_ms=20.6..28.4
warm_pair_us=0.53..0.62 refresh_ms=13.2..20.4 maxrss_kb=26240..27392
```

Debian 12, normal run:

```text
devices=1 last_enum_error=-9999 self_check=0 init_ms=5.1..5.6
warm_pair_us=1.1..1.6 refresh_ms=4.2..4.3 maxrss_kb=10136..10216
```

Ubuntu 26.04, one CPU and 128 MiB address-space limit:

```text
devices=1 last_enum_error=-9999 self_check=0 init_ms=15.7..47.9
warm_pair_us=0.76..0.86 refresh_ms=14.8..15.8 maxrss_kb=12476..12956
```

The same Ubuntu binary also completed at a 64 MiB address-space limit with
`self_check=0` and one device. The memory-constrained backend reported
`paInsufficientMemory` (`-9992`), was isolated, and did not prevent healthy
enumeration.

Debian 12 completed the same one-CPU/128 MiB run with `init_ms=5.127`,
`warm_pair_us=1.171`, `refresh_ms=6.777`, and `maxrss_kb=10016`.

## Repeatable commands

```sh
cmake -S . -B build -DPA_BUILD_TESTS=ON -DPA_BUILD_EXAMPLES=ON
cmake --build build -j

PA_DEVICE_ENUMERATION_LOG=0 build/test/patest_device_enumeration
PA_DEVICE_CACHE_TTL_MS=60000 PA_DEVICE_ENUMERATION_LOG=0 \
  build/test/patest_enumeration_bench

taskset -c 0 prlimit --as=134217728 --cpu=5 \
  build/test/patest_enumeration_bench
```

Physical USB hotplug and a low-end hardware run remain part of the owner's
release gate; the automated backend-mutation test is the permanent regression
guard for the refresh/order behavior.
