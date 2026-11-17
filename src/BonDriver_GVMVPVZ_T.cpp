#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <deque>
#include <string>

#pragma comment(lib, "bcrypt.lib")

// ============================================================================
// Standard IBonDriver / IBonDriver2 interfaces (MSVC x64 UNICODE ABI)
// ============================================================================
class IBonDriver {
public:
    virtual const BOOL OpenTuner(void) = 0;
    virtual void CloseTuner(void) = 0;

    virtual const BOOL SetChannel(const BYTE bCh) = 0;
    virtual const float GetSignalLevel(void) = 0;

    virtual const DWORD WaitTsStream(const DWORD dwTimeOut = 0) = 0;
    virtual const DWORD GetReadyCount(void) = 0;

    virtual const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain) = 0;
    virtual const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain) = 0;

    virtual void PurgeTsStream(void) = 0;
    virtual void Release(void) = 0;
};

class IBonDriver2 : public IBonDriver {
public:
    virtual LPCTSTR GetTunerName(void) = 0;
    virtual const BOOL IsTunerOpening(void) = 0;

    virtual LPCTSTR EnumTuningSpace(const DWORD dwSpace) = 0;
    virtual LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) = 0;

    virtual const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel) = 0;

    virtual const DWORD GetCurSpace(void) = 0;
    virtual const DWORD GetCurChannel(void) = 0;
};

// ============================================================================
// libusb-win32 (libusb0.dll) dynamic bindings
// ============================================================================
#pragma pack(push, 1)
struct usb_device_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
};
#pragma pack(pop)

struct usb_bus;
struct usb_device {
    struct usb_device *next, *prev;
    char filename[512];
    struct usb_bus *bus;
    struct usb_device_descriptor descriptor;
    void *config;
    void *dev;
    uint8_t devnum;
    unsigned char num_children;
    struct usb_device **children;
};

struct usb_bus {
    struct usb_bus *next, *prev;
    char dirname[512];
    struct usb_device *devices;
    uint32_t location;
    struct usb_device *root_dev;
};

typedef struct usb_dev_handle usb_dev_handle;

namespace {

typedef void (*pfn_usb_init)(void);
typedef int (*pfn_usb_find_busses)(void);
typedef int (*pfn_usb_find_devices)(void);
typedef struct usb_bus* (*pfn_usb_get_busses)(void);
typedef usb_dev_handle* (*pfn_usb_open)(struct usb_device *dev);
typedef int (*pfn_usb_close)(usb_dev_handle *dev);
typedef int (*pfn_usb_set_configuration)(usb_dev_handle *dev, int configuration);
typedef int (*pfn_usb_claim_interface)(usb_dev_handle *dev, int iface);
typedef int (*pfn_usb_release_interface)(usb_dev_handle *dev, int iface);
typedef int (*pfn_usb_control_msg)(usb_dev_handle *dev, int requesttype, int request, int value, int index, char *bytes, int size, int timeout);
typedef int (*pfn_usb_bulk_write)(usb_dev_handle *dev, int ep, const char *bytes, int size, int timeout);
typedef int (*pfn_usb_bulk_read)(usb_dev_handle *dev, int ep, char *bytes, int size, int timeout);
typedef int (*pfn_usb_interrupt_read)(usb_dev_handle *dev, int ep, char *bytes, int size, int timeout);
typedef int (*pfn_usb_reset_ex)(usb_dev_handle *dev, unsigned int reset_type);
typedef int (*pfn_usb_get_string_simple)(usb_dev_handle *dev, int index, char *buf, size_t buflen);
typedef int (*pfn_usb_bulk_setup_async)(usb_dev_handle *dev, void **context, unsigned char ep);
typedef int (*pfn_usb_submit_async)(void *context, char *bytes, int size);
typedef int (*pfn_usb_reap_async)(void *context, int timeout);
typedef int (*pfn_usb_cancel_async)(void *context);
typedef int (*pfn_usb_free_async)(void **context);
typedef int (*pfn_usb_clear_halt)(usb_dev_handle *dev, unsigned int ep);

struct LibusbApi {
    HMODULE mod = nullptr;
    pfn_usb_init usb_init = nullptr;
    pfn_usb_find_busses usb_find_busses = nullptr;
    pfn_usb_find_devices usb_find_devices = nullptr;
    pfn_usb_get_busses usb_get_busses = nullptr;
    pfn_usb_open usb_open = nullptr;
    pfn_usb_close usb_close = nullptr;
    pfn_usb_set_configuration usb_set_configuration = nullptr;
    pfn_usb_claim_interface usb_claim_interface = nullptr;
    pfn_usb_release_interface usb_release_interface = nullptr;
    pfn_usb_control_msg usb_control_msg = nullptr;
    pfn_usb_bulk_write usb_bulk_write = nullptr;
    pfn_usb_bulk_read usb_bulk_read = nullptr;
    pfn_usb_interrupt_read usb_interrupt_read = nullptr;
    pfn_usb_reset_ex usb_reset_ex = nullptr;
    pfn_usb_get_string_simple usb_get_string_simple = nullptr;
    pfn_usb_bulk_setup_async usb_bulk_setup_async = nullptr;
    pfn_usb_submit_async usb_submit_async = nullptr;
    pfn_usb_reap_async usb_reap_async = nullptr;
    pfn_usb_cancel_async usb_cancel_async = nullptr;
    pfn_usb_free_async usb_free_async = nullptr;
    pfn_usb_clear_halt usb_clear_halt = nullptr;
    bool loaded = false;

    bool EnsureLoaded() {
        if (loaded) return true;
        mod = LoadLibraryW(L"libusb0.dll");
        if (!mod) return false;
        usb_init = (pfn_usb_init)GetProcAddress(mod, "usb_init");
        usb_find_busses = (pfn_usb_find_busses)GetProcAddress(mod, "usb_find_busses");
        usb_find_devices = (pfn_usb_find_devices)GetProcAddress(mod, "usb_find_devices");
        usb_get_busses = (pfn_usb_get_busses)GetProcAddress(mod, "usb_get_busses");
        usb_open = (pfn_usb_open)GetProcAddress(mod, "usb_open");
        usb_close = (pfn_usb_close)GetProcAddress(mod, "usb_close");
        usb_set_configuration = (pfn_usb_set_configuration)GetProcAddress(mod, "usb_set_configuration");
        usb_claim_interface = (pfn_usb_claim_interface)GetProcAddress(mod, "usb_claim_interface");
        usb_release_interface = (pfn_usb_release_interface)GetProcAddress(mod, "usb_release_interface");
        usb_control_msg = (pfn_usb_control_msg)GetProcAddress(mod, "usb_control_msg");
        usb_bulk_write = (pfn_usb_bulk_write)GetProcAddress(mod, "usb_bulk_write");
        usb_bulk_read = (pfn_usb_bulk_read)GetProcAddress(mod, "usb_bulk_read");
        usb_interrupt_read = (pfn_usb_interrupt_read)GetProcAddress(mod, "usb_interrupt_read");
        usb_reset_ex = (pfn_usb_reset_ex)GetProcAddress(mod, "usb_reset_ex");
        usb_get_string_simple = (pfn_usb_get_string_simple)GetProcAddress(mod, "usb_get_string_simple");
        usb_bulk_setup_async = (pfn_usb_bulk_setup_async)GetProcAddress(mod, "usb_bulk_setup_async");
        usb_submit_async = (pfn_usb_submit_async)GetProcAddress(mod, "usb_submit_async");
        usb_reap_async = (pfn_usb_reap_async)GetProcAddress(mod, "usb_reap_async");
        usb_cancel_async = (pfn_usb_cancel_async)GetProcAddress(mod, "usb_cancel_async");
        usb_free_async = (pfn_usb_free_async)GetProcAddress(mod, "usb_free_async");
        usb_clear_halt = (pfn_usb_clear_halt)GetProcAddress(mod, "usb_clear_halt");
        loaded = usb_init && usb_find_busses && usb_find_devices && usb_get_busses &&
                 usb_open && usb_close && usb_set_configuration && usb_claim_interface &&
                 usb_release_interface && usb_control_msg && usb_bulk_write &&
                 usb_bulk_read && usb_interrupt_read && usb_reset_ex &&
                 usb_bulk_setup_async && usb_submit_async && usb_reap_async && usb_free_async;
        if (loaded) usb_init();
        return loaded;
    }
};

static LibusbApi g_usb;
static HMODULE g_hModule = nullptr;

// ============================================================================
// IVDRM 1024-bit Diffie-Hellman + AES-128 (BCrypt)
// ============================================================================
static const uint8_t IVDRM_DH_P[128] = {
    0xff,0x63,0xab,0x76,0x95,0x02,0xc0,0xf0,0x02,0xf6,0x58,0x4b,0xdb,0x5f,0x17,0x7e,
    0x11,0xa5,0xe6,0x52,0x85,0x2f,0x1e,0xc1,0x34,0x8d,0x05,0x47,0xd5,0xba,0x7d,0x79,
    0x2d,0xd7,0xb4,0x56,0x7e,0x75,0x8d,0x9d,0xda,0xbd,0x19,0x5c,0x86,0x74,0xd7,0xe4,
    0xf2,0x96,0xa2,0x4d,0x29,0x6e,0x50,0x2e,0xe3,0x92,0x5b,0x20,0x87,0xf0,0x8a,0x65,
    0xf8,0x4e,0x0d,0xa2,0x0d,0x73,0x48,0xed,0x86,0xf4,0x0c,0x22,0x1f,0x4d,0x1e,0x0c,
    0x20,0x8e,0x53,0x06,0x39,0xc6,0xa2,0xb6,0x83,0x69,0x90,0xe1,0x06,0x58,0x2a,0x70,
    0x15,0x0c,0xe7,0x7c,0x60,0x9e,0xed,0xfb,0x68,0x54,0xca,0x55,0x8f,0x87,0xbc,0xb1,
    0x50,0x8a,0xd9,0x88,0x1d,0x17,0x06,0x09,0x9d,0x16,0x71,0x11,0x86,0x87,0xd5,0xc7
};

struct Big1024 {
    uint32_t w[32];
};

static void BigFromBE(Big1024& out, const uint8_t* be, size_t len) {
    memset(out.w, 0, sizeof(out.w));
    for (size_t i = 0; i < len && i < 128; i++) {
        uint8_t b = be[len - 1 - i];
        out.w[i / 4] |= ((uint32_t)b) << ((i % 4) * 8);
    }
}

static void BigToBE(const Big1024& in, uint8_t out[128]) {
    for (int i = 0; i < 128; i++) {
        out[127 - i] = (uint8_t)((in.w[i / 4] >> ((i % 4) * 8)) & 0xFF);
    }
}

static int BigCmp(const uint32_t* a, const uint32_t* b, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (a[i] != b[i]) return (a[i] > b[i]) ? 1 : -1;
    }
    return 0;
}

static void BigSub(uint32_t* a, const uint32_t* b, int n) {
    uint64_t borrow = 0;
    for (int i = 0; i < n; i++) {
        uint64_t diff = (uint64_t)a[i] - (uint64_t)b[i] - borrow;
        a[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1;
    }
}

static void BigMulMod(Big1024& res, const Big1024& a, const Big1024& b, const Big1024& mod) {
    uint32_t prod[64] = {};
    for (int i = 0; i < 32; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < 32; j++) {
            uint64_t cur = (uint64_t)prod[i + j] + (uint64_t)a.w[i] * (uint64_t)b.w[j] + carry;
            prod[i + j] = (uint32_t)cur;
            carry = cur >> 32;
        }
        prod[i + 32] = (uint32_t)carry;
    }
    uint32_t rem[33] = {};
    uint32_t mod33[33] = {};
    memcpy(mod33, mod.w, 32 * sizeof(uint32_t));
    for (int bit = 2047; bit >= 0; bit--) {
        uint32_t carry = (prod[bit / 32] >> (bit % 32)) & 1;
        for (int k = 0; k < 33; k++) {
            uint32_t nextCarry = rem[k] >> 31;
            rem[k] = (rem[k] << 1) | carry;
            carry = nextCarry;
        }
        if (BigCmp(rem, mod33, 33) >= 0) {
            BigSub(rem, mod33, 33);
        }
    }
    memcpy(res.w, rem, 32 * sizeof(uint32_t));
}

static void ModPow1024(uint8_t outBE[128], const uint8_t* baseBE, size_t baseLen, const uint8_t* expBE, size_t expLen) {
    Big1024 mod, base, acc;
    BigFromBE(mod, IVDRM_DH_P, 128);
    BigFromBE(base, baseBE, baseLen);
    memset(acc.w, 0, sizeof(acc.w));
    acc.w[0] = 1;
    for (size_t i = 0; i < expLen; i++) {
        for (int b = 7; b >= 0; b--) {
            BigMulMod(acc, acc, acc, mod);
            if ((expBE[i] >> b) & 1) {
                BigMulMod(acc, acc, base, mod);
            }
        }
    }
    BigToBE(acc, outBE);
}

static bool Aes128Ecb(const uint8_t key[16], const uint8_t in[16], uint8_t out[16], bool encrypt) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_ECB, sizeof(BCRYPT_CHAIN_MODE_ECB), 0);
    DWORD objLen = 0, cbData = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cbData, 0);
    std::vector<uint8_t> keyObj(objLen);
    NTSTATUS st = BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), objLen, (PUCHAR)key, 16, 0);
    bool ok = false;
    if (st == 0) {
        ULONG outLen = 0;
        if (encrypt) {
            ok = (BCryptEncrypt(hKey, (PUCHAR)in, 16, nullptr, nullptr, 0, out, 16, &outLen, 0) == 0 && outLen == 16);
        } else {
            ok = (BCryptDecrypt(hKey, (PUCHAR)in, 16, nullptr, nullptr, 0, out, 16, &outLen, 0) == 0 && outLen == 16);
        }
        BCryptDestroyKey(hKey);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

struct AesCbcCtx {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<uint8_t> keyObj;
    ~AesCbcCtx() { Reset(); if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0); }
    void Reset() { if (hKey) { BCryptDestroyKey(hKey); hKey = nullptr; } }
    bool SetKey(const uint8_t key[16]) {
        Reset();
        if (!hAlg) {
            if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
            BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
            DWORD objLen = 0, cbData = 0;
            BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cbData, 0);
            keyObj.resize(objLen);
        }
        return BCryptGenerateSymmetricKey(hAlg, &hKey, keyObj.data(), (ULONG)keyObj.size(), (PUCHAR)key, 16, 0) == 0;
    }
    bool Decrypt(const uint8_t iv[16], const uint8_t* in, uint8_t* out, size_t len) {
        if (!hKey) return false;
        uint8_t ivCopy[16];
        memcpy(ivCopy, iv, 16);
        ULONG outLen = 0;
        return BCryptDecrypt(hKey, (PUCHAR)in, (ULONG)len, nullptr, ivCopy, 16, out, (ULONG)len, &outLen, 0) == 0 && outLen == len;
    }
};

// ============================================================================
// BonDriver_GVMVPVZ_T Implementation
// ============================================================================
class CBonDriverGVMVPVZ : public IBonDriver2 {
public:
    CBonDriverGVMVPVZ() {
        InitializeCriticalSection(&m_cmdCs);
        InitializeCriticalSection(&m_bufCs);
        m_hStreamEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_hWdStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        wchar_t dbgBuf[16] = {};
        m_debug = (GetEnvironmentVariableW(L"GVMVPVZ_DEBUG", dbgBuf, 16) > 0 && dbgBuf[0] != L'0');
        for (int i = 0; i < NUM_CHANNELS; i++) {
            swprintf_s(m_chNames[i], L"%dch", 13 + i);
        }
    }

    ~CBonDriverGVMVPVZ() {
        CloseTuner();
        if (m_hWdStopEvent) CloseHandle(m_hWdStopEvent);
        if (m_hStreamEvent) CloseHandle(m_hStreamEvent);
        DeleteCriticalSection(&m_bufCs);
        DeleteCriticalSection(&m_cmdCs);
    }

    // --- IBonDriver ---
    const BOOL OpenTuner(void) override {
        if (m_bOpen) return TRUE;
        if (!g_usb.EnsureLoaded()) return FALSE;

        uint16_t bcd = 0;
        struct usb_device* dev = FindVZ(&bcd);
        if (!dev) return FALSE;

        if (bcd != 0x0100) {
            if (!BootFirmware()) return FALSE;
            dev = FindVZ(&bcd);
            if (!dev || bcd != 0x0100) return FALSE;
        }

        m_hCtrl = g_usb.usb_open(dev);
        if (!m_hCtrl) return FALSE;
        g_usb.usb_set_configuration(m_hCtrl, 1);
        if (g_usb.usb_claim_interface(m_hCtrl, 0) < 0) {
            g_usb.usb_close(m_hCtrl);
            m_hCtrl = nullptr;
            return FALSE;
        }

        // Drain stale EP81 packets
        {
            uint8_t drain[512];
            for (int i = 0; i < 64; i++) {
                if (g_usb.usb_interrupt_read(m_hCtrl, 0x81, (char*)drain, sizeof(drain), 50) <= 0) break;
            }
        }

        m_hStream = g_usb.usb_open(dev);
        if (!m_hStream || g_usb.usb_claim_interface(m_hStream, 2) < 0) {
            CloseTunerInternal();
            return FALSE;
        }

        // Recover gracefully ONLY if a previous process exited while Unit8 RUN was active
        {
            std::vector<uint8_t> junk(32768);
            int staleBytes = 0;
            for (int k = 0; k < 4; k++) {
                int r = g_usb.usb_bulk_read(m_hStream, 0x83, (char*)junk.data(), (int)junk.size(), 50);
                if (r > 0) staleBytes += r; else break;
            }
            if (staleBytes > 0) {
                if (m_debug) { fprintf(stderr, "[VZ] Recovering stale EP83 (%d B)\n", staleBytes); fflush(stderr); }
                uint8_t s2 = 2, s1 = 1, s0 = 0;
                SendCommand11(8, 1, 3, &s2, 1);
                SendCommand11(8, 1, 3, &s1, 1);
                SendCommand11(8, 1, 3, &s0, 1);
                for (int k = 0; k < 4; k++) {
                    if (g_usb.usb_bulk_read(m_hStream, 0x83, (char*)junk.data(), (int)junk.size(), 30) <= 0) break;
                }
            }
        }

        // GPIO reset + SONYLJxxx demod/tuner full init + B-CAS INT ONLY on cold boot (demod reg 0x0F != 0x34).
        // Re-sending B-CAS INT (u=9 cs=34) on a warm reopen after hardware CAS streaming stalls unit=9!
        uint8_t demodCheck = ReadDemod(0x0F);
        if (m_debug) { fprintf(stderr, "[VZ] Cold-check demod reg0F=0x%02X\n", demodCheck); fflush(stderr); }
        if (demodCheck != 0x34) {
            if (m_debug) { fprintf(stderr, "[VZ] InitHardware (cold boot)...\n"); fflush(stderr); }
            if (!InitHardware()) {
                CloseTunerInternal();
                return FALSE;
            }
            if (m_debug) { fprintf(stderr, "[VZ] InitInternalBcas (cold boot)...\n"); fflush(stderr); }
            InitInternalBcas();
        } else {
            uint32_t st = 0;
            SendCommand11(9, 1, 31, &st, 4);
            if (m_debug) { fprintf(stderr, "[VZ] Warm reopen: skip GPIO/INT (B-CAS status=%u)\n", st); fflush(stderr); }
        }

        // Prepare Unit8 & u=5/u=4 tuner state (paused, ready to tune)
        if (m_debug) { fprintf(stderr, "[VZ] PrepareStreamAndTuner...\n"); fflush(stderr); }
        PrepareStreamAndTuner();

        // IVDRM mutual authentication + WatchDog background thread
        if (m_debug) { fprintf(stderr, "[VZ] RunIvdrmAuth...\n"); fflush(stderr); }
        m_activeSid = 0;
        m_ivdrmOk = RunIvdrmAuth(0, m_sessionKey);
        if (!m_ivdrmOk) {
            m_activeSid = 1;
            m_ivdrmOk = RunIvdrmAuth(1, m_sessionKey);
        }
        if (!m_ivdrmOk) {
            if (m_debug) { fprintf(stderr, "[VZ] RunIvdrmAuth FAILED\n"); fflush(stderr); }
            CloseTunerInternal();
            return FALSE;
        }

        m_wdCounter = 1;
        RunWatchDogInitAndKick();
        InterlockedExchange(&m_wdStop, 0);
        if (m_hWdStopEvent) ResetEvent(m_hWdStopEvent);
        m_hWdThread = CreateThread(nullptr, 0, WatchDogThreadProc, this, 0, nullptr);
        if (m_debug) { fprintf(stderr, "[VZ] OpenTuner OK\n"); fflush(stderr); }

        m_bOpen = true;
        m_curSpace = 0;
        m_curChannel = 0xFFFFFFFF;
        m_lastSignalDb = 0.0f;
        m_lastSignalTick = 0;
        return TRUE;
    }

    void CloseTuner(void) override {
        if (!m_bOpen && !m_hCtrl && !m_hStream) return;
        StopStreaming();
        CloseTunerInternal();
        PurgeTsStream();
        m_bOpen = false;
        m_curChannel = 0xFFFFFFFF;
    }

    const BOOL SetChannel(const BYTE bCh) override {
        return SetChannel(0, (DWORD)bCh);
    }

    const float GetSignalLevel(void) override {
        if (!m_bOpen || m_curChannel == 0xFFFFFFFF) return 0.0f;
        ULONGLONG now = GetTickCount64();
        if (now - m_lastSignalTick < 500) return m_lastSignalDb;
        m_lastSignalTick = now;

        uint8_t s80 = ReadDemod(0x80);
        uint8_t sb0 = ReadDemod(0xB0);
        if ((s80 & 0x3F) != 0 || (sb0 & 0x0F) < 8) {
            m_lastSignalDb = 0.0f;
            return 0.0f;
        }
        // Read 3-byte CN monitor from demod reg 0x8B (matches PhilMAS64 GetIsdbtSignalStrength)
        uint8_t cn[3] = {};
        if (I2CReadRegs(5, 0x8B, cn, 3)) {
            uint32_t raw = ((uint32_t)cn[0] << 16) | ((uint32_t)cn[1] << 8) | cn[2];
            // Approximate dB mapping for SONY demod CN register (typical good reception ~24-30 dB)
            if (raw > 0) {
                float db = 32.0f - (float)raw / 2000.0f;
                if (db < 5.0f) db = 5.0f;
                if (db > 35.0f) db = 35.0f;
                m_lastSignalDb = db;
                return m_lastSignalDb;
            }
        }
        m_lastSignalDb = 25.0f;
        return m_lastSignalDb;
    }

    const DWORD WaitTsStream(const DWORD dwTimeOut = 0) override {
        if (!m_bOpen) return WAIT_ABANDONED;
        DWORD r = WaitForSingleObject(m_hStreamEvent, dwTimeOut);
        if (r == WAIT_OBJECT_0) return WAIT_OBJECT_0;
        if (r == WAIT_TIMEOUT) return WAIT_TIMEOUT;
        return WAIT_FAILED;
    }

    const DWORD GetReadyCount(void) override {
        EnterCriticalSection(&m_bufCs);
        DWORD cnt = (DWORD)m_tsQueue.size();
        LeaveCriticalSection(&m_bufCs);
        return cnt;
    }

    const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain) override {
        BYTE* pSrc = nullptr;
        if (!GetTsStream(&pSrc, pdwSize, pdwRemain)) return FALSE;
        if (pDst && pSrc && pdwSize && *pdwSize > 0) {
            memcpy(pDst, pSrc, *pdwSize);
        }
        return TRUE;
    }

    const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain) override {
        if (!ppDst || !pdwSize) return FALSE;
        EnterCriticalSection(&m_bufCs);
        if (m_tsQueue.empty()) {
            *ppDst = nullptr;
            *pdwSize = 0;
            if (pdwRemain) *pdwRemain = 0;
            ResetEvent(m_hStreamEvent);
            LeaveCriticalSection(&m_bufCs);
            return FALSE;
        }
        m_lastChunk = std::move(m_tsQueue.front());
        m_tsQueue.pop_front();
        DWORD rem = (DWORD)m_tsQueue.size();
        if (rem == 0) ResetEvent(m_hStreamEvent);
        LeaveCriticalSection(&m_bufCs);

        *ppDst = m_lastChunk.data();
        *pdwSize = (DWORD)m_lastChunk.size();
        if (pdwRemain) *pdwRemain = rem;
        return TRUE;
    }

    void PurgeTsStream(void) override {
        EnterCriticalSection(&m_bufCs);
        m_tsQueue.clear();
        ResetEvent(m_hStreamEvent);
        LeaveCriticalSection(&m_bufCs);
    }

    void Release(void) override {
        delete this;
    }

    // --- IBonDriver2 ---
    LPCTSTR GetTunerName(void) override {
        return L"I-O DATA GV-MVP/VZ (ISDB-T)";
    }

    const BOOL IsTunerOpening(void) override {
        return m_bOpen ? TRUE : FALSE;
    }

    LPCTSTR EnumTuningSpace(const DWORD dwSpace) override {
        return (dwSpace == 0) ? L"UHF" : nullptr;
    }

    LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel) override {
        if (dwSpace != 0 || dwChannel >= NUM_CHANNELS) return nullptr;
        return m_chNames[dwChannel];
    }

    const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel) override {
        if (!m_bOpen || dwSpace != 0 || dwChannel >= NUM_CHANNELS) return FALSE;

        // On in-process channel switch, run the exact same clean teardown + warm re-acquire sequence
        // proven across separate process restarts (StopStreaming while EP83 drains -> CloseSession -> Prepare -> Auth -> WatchDog).
        if (m_streaming) {
            if (m_debug) { fprintf(stderr, "[VZ] In-process switch: teardown & warm re-acquire...\n"); fflush(stderr); }
            StopStreaming();
            if (m_hWdThread) {
                InterlockedExchange(&m_wdStop, 1);
                if (m_hWdStopEvent) SetEvent(m_hWdStopEvent);
                WaitForSingleObject(m_hWdThread, 3000);
                CloseHandle(m_hWdThread);
                m_hWdThread = nullptr;
            }
            if (m_ivdrmOk && m_hCtrl) {
                uint8_t closeBuf[9] = {};
                *(uint32_t*)(closeBuf + 1) = m_activeSid;
                SendCommand11(9, 1, 5, closeBuf, 9);
                m_ivdrmOk = false;
            }
            PrepareStreamAndTuner();
            m_ivdrmOk = RunIvdrmAuth(m_activeSid, m_sessionKey);
            if (!m_ivdrmOk) {
                if (m_debug) { fprintf(stderr, "[VZ] Re-auth failed on switch\n"); fflush(stderr); }
                return FALSE;
            }
            m_wdCounter = 1;
            RunWatchDogInitAndKick();
            InterlockedExchange(&m_wdStop, 0);
            if (m_hWdStopEvent) ResetEvent(m_hWdStopEvent);
            m_hWdThread = CreateThread(nullptr, 0, WatchDogThreadProc, this, 0, nullptr);
        }
        PurgeTsStream();

        // Exact SonyLJxxx ISDB-T PLL formula from PhilMAS64 @ 0xce7b7..0xce874
        uint32_t khz = 473143u + (uint32_t)dwChannel * 6000u;
        uint32_t pll = (uint32_t)(((uint64_t)khz * 64ULL + 500ULL) / 1000ULL);
        WriteDemod(0x25, 0x00);
        WriteDemod(0x23, 0x4D);
        WriteTuner5(0x11, 0x00);
        WriteTuner5(0x13, 0x15);
        WriteTuner5(0x14, (uint8_t)(pll & 0xFF));
        WriteTuner5(0x15, (uint8_t)((pll >> 8) & 0xFF));
        WriteTuner5(0x11, 0x02);
        WriteDemod(0x01, 0x40);
        WriteDemod(0x23, 0x4C);

        // Wait for ISDB-T demod lock (up to 2.5s)
        bool locked = false;
        uint8_t last80 = 0xFF, lastB0 = 0x00;
        for (int p = 0; p < 25; p++) {
            Sleep(100);
            last80 = ReadDemod(0x80);
            lastB0 = ReadDemod(0xB0);
            if ((last80 & 0x3F) == 0 && (lastB0 & 0x0F) >= 8) {
                locked = true;
                break;
            }
        }
        if (m_debug) { fprintf(stderr, "[VZ] Tune ch=%lu (%u kHz) locked=%d reg80=0x%02X regB0=0x%02X\n", dwChannel, khz, locked ? 1 : 0, last80, lastB0); fflush(stderr); }

        // Start EP83 async reader BEFORE Unit8 RUN so DMA never stalls
        InterlockedExchange(&m_readerStop, 0);
        m_hReaderThread = CreateThread(nullptr, 0, ReaderThreadProc, this, 0, nullptr);
        Sleep(40);

        uint8_t runVal = 3;
        bool runOk = SendCommand11(8, 1, 3, &runVal, 1);
        if (m_debug) { fprintf(stderr, "[VZ] Unit8 RUN ok=%d\n", runOk ? 1 : 0); fflush(stderr); }
        if (!runOk) {
            InterlockedExchange(&m_readerStop, 1);
            if (m_hReaderThread) {
                WaitForSingleObject(m_hReaderThread, 3000);
                CloseHandle(m_hReaderThread);
                m_hReaderThread = nullptr;
            }
            return FALSE;
        }
        m_streaming = true;
        m_curSpace = dwSpace;
        m_curChannel = dwChannel;
        m_lastSignalTick = 0;

        // Allow ~350ms for NXP7164F hardware B-CAS to lock onto the new channel's ECM, then purge pre-ECM packets
        if (locked) {
            Sleep(350);
            PurgeTsStream();
        }
        return TRUE;
    }

    const DWORD GetCurSpace(void) override { return m_curSpace; }
    const DWORD GetCurChannel(void) override { return m_curChannel; }

private:
    static constexpr int NUM_CHANNELS = 50; // UHF 13ch..62ch
    static constexpr size_t MAX_QUEUE_CHUNKS = 512;

    CRITICAL_SECTION m_cmdCs;
    CRITICAL_SECTION m_bufCs;
    HANDLE m_hStreamEvent = nullptr;

    usb_dev_handle* m_hCtrl = nullptr;
    usb_dev_handle* m_hStream = nullptr;
    bool m_bOpen = false;
    bool m_streaming = false;
    bool m_ivdrmOk = false;
    bool m_debug = false;
    uint32_t m_activeSid = 0;
    uint8_t m_sessionKey[16] = {};
    uint64_t m_wdCounter = 1;
    uint8_t m_cmdTag = 0;
    uint32_t m_bcasEventSeq = 0;

    HANDLE m_hWdThread = nullptr;
    HANDLE m_hWdStopEvent = nullptr;
    volatile LONG m_wdStop = 0;

    HANDLE m_hReaderThread = nullptr;
    volatile LONG m_readerStop = 0;

    DWORD m_curSpace = 0;
    DWORD m_curChannel = 0xFFFFFFFF;
    float m_lastSignalDb = 0.0f;
    ULONGLONG m_lastSignalTick = 0;

    wchar_t m_chNames[NUM_CHANNELS][16] = {};
    std::deque<std::vector<uint8_t>> m_tsQueue;
    std::vector<uint8_t> m_lastChunk;

    static struct usb_device* FindVZ(uint16_t* bcdOut = nullptr) {
        g_usb.usb_find_busses();
        g_usb.usb_find_devices();
        for (struct usb_bus* b = g_usb.usb_get_busses(); b; b = b->next) {
            for (struct usb_device* d = b->devices; d; d = d->next) {
                if (d->descriptor.idVendor == 0x04BB && d->descriptor.idProduct == 0x052B) {
                    if (bcdOut) *bcdOut = d->descriptor.bcdDevice;
                    return d;
                }
            }
        }
        return nullptr;
    }

    static bool ResolveFirmwarePath(std::wstring& outPath) {
        wchar_t envPath[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"GVMVPVZ_FW_PATH", envPath, MAX_PATH) > 0 &&
            GetFileAttributesW(envPath) != INVALID_FILE_ATTRIBUTES) {
            outPath = envPath;
            return true;
        }
        wchar_t dllPath[MAX_PATH] = {};
        if (GetModuleFileNameW(g_hModule, dllPath, MAX_PATH) > 0) {
            wchar_t* slash = wcsrchr(dllPath, L'\\');
            if (slash) {
                wcscpy(slash + 1, L"NXP7164F_052B04BB.bin");
                if (GetFileAttributesW(dllPath) != INVALID_FILE_ATTRIBUTES) {
                    outPath = dllPath;
                    return true;
                }
            }
        }
        const wchar_t* fallback = L"C:\\DTV\\BonDriver\\NXP7164F_052B04BB.bin";
        if (GetFileAttributesW(fallback) != INVALID_FILE_ATTRIBUTES) {
            outPath = fallback;
            return true;
        }
        return false;
    }

    static bool UploadDfuStage(usb_dev_handle* h, const uint8_t* img, size_t len) {
        g_usb.usb_set_configuration(h, 1);
        if (g_usb.usb_claim_interface(h, 0) < 0) return false;
        if (g_usb.usb_clear_halt) g_usb.usb_clear_halt(h, 0x03);

        uint8_t st0[6] = {};
        int r = g_usb.usb_control_msg(h, 0xA1, 3, 0, 0, (char*)st0, 6, 2000);
        if (r == 6 && st0[4] == 10) {
            g_usb.usb_control_msg(h, 0x21, 4, 0, 0, nullptr, 0, 2000);
            r = g_usb.usb_control_msg(h, 0xA1, 3, 0, 0, (char*)st0, 6, 2000);
        }
        if (r == 6 && st0[4] != 2) {
            g_usb.usb_control_msg(h, 0x21, 6, 0, 0, nullptr, 0, 2000);
        }
        g_usb.usb_control_msg(h, 0x21, 6, 0, 0, nullptr, 0, 2000);
        g_usb.usb_control_msg(h, 0xA1, 3, 0, 0, (char*)st0, 6, 2000);

        size_t pos = 0;
        uint16_t block = 1;
        uint8_t status[6];
        for (; pos < len; block++) {
            size_t chunk = (len - pos > 4096) ? 4096 : (len - pos);
            uint8_t pad[4096] = {};
            const uint8_t* ptr = img + pos;
            if (chunk < 4096) {
                memcpy(pad, ptr, chunk);
                ptr = pad;
            }
            uint16_t hs_len = 4096;
            if (g_usb.usb_control_msg(h, 0x21, 1, block, 0, (char*)&hs_len, 2, 2000) < 0) {
                g_usb.usb_release_interface(h, 0);
                return false;
            }
            if (g_usb.usb_bulk_write(h, 0x03, (const char*)ptr, 4096, 5000) < 0) {
                g_usb.usb_release_interface(h, 0);
                return false;
            }
            r = g_usb.usb_control_msg(h, 0xA1, 3, 0, 0, (char*)status, 6, 10000);
            if (r < 0) { g_usb.usb_release_interface(h, 0); return false; }
            while (r == 6 && status[4] == 4) {
                DWORD waitMs = status[1] | (status[2] << 8) | (status[3] << 16);
                Sleep(waitMs > 0 ? waitMs : 5);
                r = g_usb.usb_control_msg(h, 0xA1, 3, 0, 0, (char*)status, 6, 10000);
            }
            if (r == 6 && status[4] != 5) {
                g_usb.usb_release_interface(h, 0);
                return false;
            }
            pos += chunk;
        }
        g_usb.usb_control_msg(h, 0x21, 1, block, 0, nullptr, 0, 2000);
        g_usb.usb_bulk_write(h, 0x03, nullptr, 0, 2000);
        g_usb.usb_control_msg(h, 0xA1, 3, 0, 0, (char*)status, 6, 2000);
        g_usb.usb_release_interface(h, 0);
        g_usb.usb_reset_ex(h, 1);
        return true;
    }

    static bool BootFirmware() {
        std::wstring fwPath;
        if (!ResolveFirmwarePath(fwPath)) return false;
        FILE* f = _wfopen(fwPath.c_str(), L"rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        size_t fsz = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsz < 0x20000) { fclose(f); return false; }
        std::vector<uint8_t> fw(fsz);
        fread(fw.data(), 1, fsz, f);
        fclose(f);

        uint32_t len1 = (*(uint32_t*)(fw.data() + 0x10) + *(uint32_t*)(fw.data() + 0x14) + 1) << 4;
        uint32_t len2 = (*(uint32_t*)(fw.data() + 0x1FD80) + *(uint32_t*)(fw.data() + 0x1FD84) + 1) << 4;

        for (int attempt = 0; attempt < 4; attempt++) {
            uint16_t bcd = 0;
            struct usb_device* dev = FindVZ(&bcd);
            if (!dev) { Sleep(500); continue; }
            usb_dev_handle* h = g_usb.usb_open(dev);
            if (!h) { Sleep(500); continue; }
            struct usb_device_descriptor hwDesc = {};
            if (g_usb.usb_control_msg(h, 0x80, 6, 0x0100, 0, (char*)&hwDesc, 18, 2000) == 18) {
                bcd = hwDesc.bcdDevice;
            }
            if (bcd == 0x0100) {
                g_usb.usb_close(h);
                return true;
            }
            char iprod[64] = {};
            if (g_usb.usb_get_string_simple) {
                g_usb.usb_get_string_simple(h, 0x20, iprod, sizeof(iprod));
            }
            if (strncmp(iprod, "01", 2) == 0) {
                UploadDfuStage(h, fw.data() + 0x1FD80, len2);
                g_usb.usb_close(h);
                Sleep(4000);
            } else {
                UploadDfuStage(h, fw.data() + 0x10, len1);
                g_usb.usb_close(h);
                Sleep(2000);
            }
        }
        uint16_t bcd = 0;
        return FindVZ(&bcd) != nullptr && bcd == 0x0100;
    }

    bool SendCommand11(uint8_t unit, uint32_t bRequest, uint16_t cs, void* buf, uint16_t size) {
        if (!m_hCtrl) return false;
        EnterCriticalSection(&m_cmdCs);
        uint8_t curTag = m_cmdTag++;
        uint16_t sent = 0;
        while (true) {
            uint16_t chunk = (size - sent > 256) ? 256 : (size - sent);
            uint8_t pkt[11 + 256] = {};
            pkt[0] = unit;
            pkt[1] = 2 | ((sent + chunk < size) ? 0x10 : 0);
            *(uint16_t*)(pkt + 2) = chunk;
            *(uint32_t*)(pkt + 4) = bRequest;
            *(uint16_t*)(pkt + 8) = cs;
            pkt[10] = curTag;
            if (chunk > 0 && buf) memcpy(pkt + 11, (uint8_t*)buf + sent, chunk);
            if (g_usb.usb_bulk_write(m_hCtrl, 0x01, (const char*)pkt, 11 + chunk, 2000) < 0) {
                if (m_debug) { fprintf(stderr, "[VZ] CMD EP1-WR FAIL u=%u bReq=0x%X cs=%u\n", unit, bRequest, cs); fflush(stderr); }
                LeaveCriticalSection(&m_cmdCs);
                return false;
            }
            sent += chunk;
            if (sent >= size) break;
        }
        uint16_t offset = 0;
        int skips = 0;
        while (true) {
            uint8_t resp[512] = {};
            int rlen = g_usb.usb_interrupt_read(m_hCtrl, 0x81, (char*)resp, sizeof(resp), 2000);
            if (rlen < 11) {
                if (m_debug) { fprintf(stderr, "[VZ] CMD EP81-RD FAIL u=%u bReq=0x%X cs=%u rlen=%d\n", unit, bRequest, cs, rlen); fflush(stderr); }
                LeaveCriticalSection(&m_cmdCs);
                return false;
            }
            if ((resp[0] != unit || *(uint16_t*)(resp + 8) != cs) && skips < 8) {
                skips++;
                continue;
            }
            uint8_t flags = resp[1];
            uint16_t rsize = *(uint16_t*)(resp + 2);
            if (flags & 0x01) {
                if (m_debug) {
                    uint32_t err = (rlen >= 15) ? *(uint32_t*)(resp + 11) : 0;
                    fprintf(stderr, "[VZ] CMD ERR u=%u bReq=0x%X cs=%u err=0x%08X\n", unit, bRequest, cs, err);
                    fflush(stderr);
                }
                LeaveCriticalSection(&m_cmdCs);
                return false;
            }
            if (buf && offset + rsize <= size && rlen >= 11 + rsize) {
                memcpy((uint8_t*)buf + offset, resp + 11, rsize);
            }
            offset += rsize;
            if (!(flags & 0x10)) break;
        }
        LeaveCriticalSection(&m_cmdCs);
        return true;
    }

    bool GPIOWritePin(uint32_t pin, uint8_t val) {
        uint8_t buf[5] = {};
        *(uint32_t*)buf = pin;
        buf[4] = val;
        return SendCommand11(2, 1, 1, buf, 5);
    }

    bool I2CWrite(uint8_t unit, uint8_t reg, const uint8_t* data, uint32_t len) {
        uint8_t buf[256] = {};
        *(uint32_t*)(buf + 0) = 1;
        *(uint32_t*)(buf + 4) = len;
        buf[8] = reg;
        if (len > 0 && data) memcpy(buf + 9, data, len);
        return SendCommand11(unit, 1, 0, buf, 256);
    }

    bool I2CReadRegs(uint8_t unit, uint8_t reg, uint8_t* out, uint32_t len) {
        uint8_t buf[256] = {};
        *(uint32_t*)(buf + 0) = 1;
        *(uint32_t*)(buf + 4) = len;
        buf[8] = reg;
        if (!SendCommand11(unit, 0x81, 0, buf, 256)) return false;
        if (out && len > 0 && len <= 240) memcpy(out, buf + 9, len);
        return true;
    }

    void WriteDemod(uint8_t reg, uint8_t val) { I2CWrite(5, reg, &val, 1); }
    uint8_t ReadDemod(uint8_t reg) { uint8_t v = 0; I2CReadRegs(5, reg, &v, 1); return v; }
    void WriteTuner5(uint8_t reg, uint8_t val) { uint8_t d[3] = { 0xC0, reg, val }; I2CWrite(5, 0xFE, d, 3); }
    void WriteUnit4_2(uint8_t reg, uint8_t val) { I2CWrite(4, reg, &val, 1); }
    void WriteUnit4_Tuner(const uint8_t* data, int len) { I2CWrite(4, data[0], data + 1, len - 1); }

    bool InitHardware() {
        if (!GPIOWritePin(0, 0)) return false;
        Sleep(15);
        GPIOWritePin(0, 1);
        Sleep(800);
        GPIOWritePin(0x16, 1);
        Sleep(50);

        const uint8_t init1[][2] = {
            {0x01,0x00},{0x02,0x00},{0x03,0x00},{0x04,0x02},{0x05,0x00},{0x06,0x80},{0x07,0x00},{0x08,0x00},
            {0x0c,0x00},{0x0f,0x34},{0x11,0x27},{0x12,0x0c},{0x13,0x33},{0x14,0x20},{0x15,0x40},{0x16,0x00},
            {0x17,0x70},{0x18,0x31},{0x19,0x13},{0x1a,0x31},{0x1b,0x13},{0x1c,0x38},{0x1d,0xa8},{0x1e,0xaa},
            {0x1f,0xa8},{0x20,0x00},{0x21,0xff},{0x22,0x80},{0x23,0x4c},{0x24,0x4c},{0x25,0x00},{0x26,0x00},
            {0x27,0x0c},{0x28,0x60},{0x29,0x6b},{0x2a,0x40},{0x2b,0x40},{0x2c,0x5a},{0x2d,0x05},{0x2e,0xff},
            {0x2f,0x00},{0x30,0x20},{0x31,0x0d},{0x32,0x21},{0x34,0x0f},{0x38,0x00},{0x39,0xaa},{0x3a,0x2d},
            {0x3b,0x21},{0x3c,0x3f},{0x3d,0x10},{0x3e,0x08},{0x3f,0x0c},{0x40,0x0c},{0x41,0x00},{0x42,0x00},
            {0x43,0x4f},{0x44,0xff},{0x46,0x20},{0x47,0x00},{0x48,0x90},{0x49,0xe6},{0x4a,0x02},{0x4b,0x80},
            {0x4c,0x00},{0x4f,0x05},{0x50,0x00},{0x51,0x68},{0x52,0x20},{0x54,0x57},{0x55,0xf1},{0x56,0x20},
            {0x57,0x70},{0x5c,0x50},{0x5d,0x00},{0x5f,0x80},{0x70,0x38},{0x71,0x00},{0x72,0x00},{0x75,0x02},
            {0x76,0x00},{0x77,0x01},{0x7c,0x00},{0x7d,0xd2},{0xba,0x00},{0xbb,0x00},{0xbc,0x00},{0xc7,0x00},
            {0xe4,0xf4},{0xec,0x00}
        };
        for (auto& kv : init1) WriteDemod(kv[0], kv[1]);
        const uint8_t init2[][2] = {
            {0xff,0xff},{0x2c,0x44},{0x4d,0x40},{0x7f,0x02},{0x9a,0x52},{0x48,0x5a},{0x76,0x1a},{0x6a,0x48},
            {0x64,0x28},{0x66,0xe6},{0x35,0x11},{0x7e,0x01},{0x0b,0x99},{0x0c,0x00},{0x10,0x00},{0x12,0xca},
            {0x16,0x90},{0x32,0x36},{0xd8,0x18},{0x00,0x3f},{0x02,0x00},{0x03,0x98},{0x04,0x00}
        };
        for (auto& kv : init2) WriteTuner5(kv[0], kv[1]);
        return true;
    }

    bool RunIvdrmAuth(uint32_t sessionId, uint8_t outSessionKey[16]) {
        for (uint32_t sid : { 0u, 1u }) {
            uint8_t closeBuf[9] = {};
            *(uint32_t*)(closeBuf + 1) = sid;
            SendCommand11(9, 1, 5, closeBuf, 9);
        }
        uint8_t z = 0;
        SendCommand11(9, 1, 0, &z, 1);
        uint32_t initResp = 0;
        SendCommand11(9, 0x81, 1, &initResp, 4);

        uint8_t openBuf[9] = {};
        *(uint32_t*)(openBuf + 1) = sessionId;
        if (!SendCommand11(9, 1, 2, openBuf, 9) || *(uint32_t*)(openBuf + 5) != 0) return false;

        uint8_t expA[16] = {};
        BCryptGenRandom(nullptr, expA, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        expA[0] |= 0x01;
        uint8_t gByte = 2;
        uint8_t pubA[128] = {}, pubB[128] = {}, sharedS[128] = {};
        ModPow1024(pubA, &gByte, 1, expA, 16);

        uint8_t msg[0x10c] = {};
        *(uint32_t*)(msg + 0) = 0x80;
        memcpy(msg + 4, pubA, 128);
        *(uint32_t*)(msg + 0x104) = sessionId;
        if (!SendCommand11(9, 1, 3, msg, 0x10c) || *(uint32_t*)(msg + 0x108) != 0) return false;
        if (!SendCommand11(9, 0x81, 3, msg, 0x10c) || *(uint32_t*)(msg + 0x108) != 0) return false;
        memcpy(pubB, msg + 4, 128);

        ModPow1024(sharedS, pubB, 128, expA, 16);
        memcpy(outSessionKey, sharedS, 16);

        const uint8_t authPlain[16] = {'A','u','t','h','o','r','i','z','a','t','i','o','n',' ','O','K'};
        uint8_t authEnc[16] = {}, authDec[16] = {};
        if (!Aes128Ecb(pubB, authPlain, authEnc, true)) return false;

        memset(msg, 0, sizeof(msg));
        *(uint32_t*)(msg + 0) = 0x10;
        memcpy(msg + 4, authEnc, 16);
        *(uint32_t*)(msg + 0x104) = sessionId;
        if (!SendCommand11(9, 1, 4, msg, 0x10c) || *(uint32_t*)(msg + 0x108) != 0) return false;
        if (!SendCommand11(9, 0x81, 4, msg, 0x10c) || *(uint32_t*)(msg + 0x108) != 0) return false;

        Aes128Ecb(pubA, msg + 4, authDec, false);
        return memcmp(authDec, authPlain, 16) == 0;
    }

    void KickWatchDogOnce() {
        m_wdCounter++;
        const struct { uint32_t id; uint32_t magic; } clients[] = {
            { 1, 0x4a73f812 }, { 2, 0xf94c36dc }, { 4, 0x71c0e12d }
        };
        bool allOk = true;
        for (auto& c : clients) {
            uint8_t plain[16] = {}, enc[16] = {}, pkt[20] = {};
            *(uint64_t*)(plain + 0) = m_wdCounter;
            *(uint32_t*)(plain + 8) = c.id;
            *(uint32_t*)(plain + 12) = c.magic;
            Aes128Ecb(m_sessionKey, plain, enc, true);
            memcpy(pkt + 4, enc, 16);
            if (!SendCommand11(9, 1, 7, pkt, 20)) allOk = false;
        }
        if (m_debug && (!allOk || (m_wdCounter % 30 == 0))) {
            uint32_t st = 0xFFFFFFFF;
            SendCommand11(9, 0x81, 8, &st, 4);
            fprintf(stderr, "[VZ] WD cnt=%llu ok=%d cs8=0x%08X\n", (unsigned long long)m_wdCounter, allOk ? 1 : 0, st);
            fflush(stderr);
        }
    }

    void RunWatchDogInitAndKick() {
        uint8_t wdInit[12] = {};
        *(uint32_t*)(wdInit + 0) = 0x00000007;
        *(uint64_t*)(wdInit + 4) = m_wdCounter;
        SendCommand11(9, 1, 6, wdInit, 12);
        KickWatchDogOnce();
    }

    static DWORD WINAPI WatchDogThreadProc(LPVOID param) {
        CBonDriverGVMVPVZ* self = (CBonDriverGVMVPVZ*)param;
        while (!InterlockedCompareExchange(&self->m_wdStop, 0, 0)) {
            if (WaitForSingleObject(self->m_hWdStopEvent, 2000) == WAIT_OBJECT_0) break;
            if (InterlockedCompareExchange(&self->m_wdStop, 0, 0)) break;
            self->KickWatchDogOnce();
        }
        return 0;
    }

    void InitInternalBcas() {
        uint32_t st = 0;
        if (!SendCommand11(9, 1, 31, &st, 4)) return;
        if (m_debug) { fprintf(stderr, "[VZ] B-CAS initial status=%u\n", st); fflush(stderr); }
        uint8_t info[262] = {};
        info[8] = 1; info[16] = 8; info[17] = 1; info[24] = 1;
        SendCommand11(9, 1, 30, info, 262);
        if (st == 2) return; // Card already initialized & ready

        // Drain any leftover B-CAS events
        for (int d = 0; d < 4; d++) {
            uint8_t ev[540] = {};
            if (!SendCommand11(9, 0x81, 32, ev, 540)) break;
            if (*(uint32_t*)(ev + 4) != 0) break;
            uint8_t comp[12] = {};
            *(uint32_t*)(comp + 4) = ++m_bcasEventSeq;
            SendCommand11(9, 1, 33, comp, 12);
        }

        // Send ARIB INT (0x30) to put internal B-CAS into ready state (status=2)
        uint8_t req[272] = {};
        *(uint32_t*)(req + 4) = 0x30;
        req[8] = 0x30;
        if (!SendCommand11(9, 1, 34, req, 272)) return;

        Sleep(60);
        for (int tries = 0; tries < 20; tries++) {
            uint8_t ev[540] = {};
            if (SendCommand11(9, 0x81, 32, ev, 540) && *(uint32_t*)(ev + 4) == 0) {
                uint8_t comp[12] = {};
                *(uint32_t*)(comp + 4) = ++m_bcasEventSeq;
                SendCommand11(9, 1, 33, comp, 12);
                uint8_t ev2[540] = {};
                SendCommand11(9, 0x81, 32, ev2, 540);
                break;
            }
            Sleep(30);
        }
    }

    void PrepareStreamAndTuner() {
        uint8_t probeBuf[4] = { 0x00, 0x00, 0x01, 0x01 };
        uint8_t stopVal = 0, acqVal = 1, pauseVal = 2;
        SendCommand11(8, 1, 3, &acqVal, 1);
        SendCommand11(8, 1, 3, &stopVal, 1);
        SendCommand11(8, 1, 1, probeBuf, 4);
        SendCommand11(8, 1, 2, probeBuf, 4);
        SendCommand11(8, 1, 3, &acqVal, 1);
        SendCommand11(8, 1, 3, &pauseVal, 1);

        uint8_t u9buf[8] = {};
        SendCommand11(9, 1, 22, u9buf, 8);

        WriteDemod(0x0f, 0x34);
        WriteDemod(0x1c, 0x28);
        WriteDemod(0x1e, 0xa2);
        WriteDemod(0x1f, 0x08);

        WriteUnit4_2(0x1c, 0xaa);
        WriteUnit4_2(0x1e, 0x20);
        WriteUnit4_2(0x1f, 0xaa);
        { uint8_t d[] = {0xfe,0xc0,0x04,0x40}; WriteUnit4_Tuner(d, 4); }
        WriteUnit4_2(0x17, 0x01);
        { uint8_t d[] = {0xfe,0xc0,0x0e,0x2f}; WriteUnit4_Tuner(d, 4); }
        { uint8_t d[] = {0xfe,0xc0,0x16,0x00}; WriteUnit4_Tuner(d, 4); }
        { uint8_t d[] = {0xfe,0xc0,0x02,0x0c,0xf6}; WriteUnit4_Tuner(d, 5); }
        { uint8_t d[] = {0xfe,0xc0,0x19,0x40}; WriteUnit4_Tuner(d, 4); }
        { uint8_t d[] = {0xfe,0xc0,0x20,0x08}; WriteUnit4_Tuner(d, 4); }

        const uint8_t tinit[][2] = {
            {0xff,0xff},{0x2c,0x44},{0x4d,0x40},{0x7f,0x02},{0x9a,0x52},
            {0x48,0x5a},{0x76,0x1a},{0x6a,0x48},{0x64,0x28},{0x66,0xe6},
            {0x35,0x11},{0x7e,0x01},{0x0b,0x99},{0x0c,0x00},{0x10,0x00},
            {0x12,0xca},{0x16,0x90},{0x32,0x36},{0xd8,0x18},{0x05,0x01}
        };
        for (auto& kv : tinit) WriteTuner5(kv[0], kv[1]);
        WriteDemod(0x03, 0x00);
    }

    void PushTsChunk(std::vector<uint8_t>&& chunk) {
        if (chunk.empty()) return;
        EnterCriticalSection(&m_bufCs);
        while (m_tsQueue.size() >= MAX_QUEUE_CHUNKS) {
            m_tsQueue.pop_front();
        }
        m_tsQueue.push_back(std::move(chunk));
        SetEvent(m_hStreamEvent);
        LeaveCriticalSection(&m_bufCs);
    }

    static DWORD WINAPI ReaderThreadProc(LPVOID param) {
        CBonDriverGVMVPVZ* self = (CBonDriverGVMVPVZ*)param;
        const int NUM_URB = 8;
        const int URB_SZ = 32768;
        void* ctx[NUM_URB] = {};
        uint8_t* tsBuf[NUM_URB] = {};
        for (int i = 0; i < NUM_URB; i++) {
            tsBuf[i] = (uint8_t*)malloc(URB_SZ);
            if (tsBuf[i] && self->m_hStream) {
                g_usb.usb_bulk_setup_async(self->m_hStream, &ctx[i], 0x83);
                if (ctx[i]) g_usb.usb_submit_async(ctx[i], (char*)tsBuf[i], URB_SZ);
            }
        }

        AesCbcCtx sessAes, tsAes;
        sessAes.SetKey(self->m_sessionKey);
        uint8_t tsIv[16] = {}, zeroIv[16] = {};
        bool hasTsKey = false;
        std::vector<uint8_t> carry;
        carry.reserve(URB_SZ * 2);
        std::vector<uint8_t> outChunk;
        outChunk.reserve(URB_SZ);

        int cur = 0;
        bool loggedFirst = false;
        while (!InterlockedCompareExchange(&self->m_readerStop, 0, 0)) {
            if (!ctx[cur]) { Sleep(10); cur = (cur + 1) % NUM_URB; continue; }
            int rr = g_usb.usb_reap_async(ctx[cur], 50);
            if (rr > 0) {
                if (!loggedFirst && self->m_debug) {
                    loggedFirst = true;
                    fprintf(stderr, "[VZ] EP83 first read %d B\n", rr);
                    fflush(stderr);
                }
                carry.insert(carry.end(), tsBuf[cur], tsBuf[cur] + rr);
                g_usb.usb_submit_async(ctx[cur], (char*)tsBuf[cur], URB_SZ);

                outChunk.clear();
                size_t pos = 0;
                while (pos + 188 <= carry.size()) {
                    if (carry[pos] != 0x47) { pos++; continue; }
                    uint8_t* pkt = &carry[pos];
                    uint8_t tsc = (pkt[3] >> 6) & 3;
                    uint16_t pid = ((pkt[1] & 0x1F) << 8) | pkt[2];
                    uint8_t afc = pkt[3] & 0x30;

                    if ((tsc == 1 || tsc == 2) && pid == 0x0000 && afc == 0x20) {
                        uint8_t dec[176] = {};
                        if (sessAes.Decrypt(zeroIv, pkt + 4, dec, 176) &&
                            memcmp(dec + 0x20, "FORIVFROMNXP", 12) == 0) {
                            memcpy(tsIv, dec + 0x00, 16);
                            tsAes.SetKey(dec + 0x10);
                            hasTsKey = true;
                            if (self->m_debug) {
                                fprintf(stderr, "[VZ] TS key rotated (tsc=%u)\n", tsc);
                                fflush(stderr);
                            }
                        }
                    } else if (tsc == 3 && hasTsKey) {
                        tsAes.Decrypt(tsIv, pkt + 4, pkt + 4, 176);
                        pkt[3] &= 0x3F;
                        if (pid != 0x1FFF) {
                            outChunk.insert(outChunk.end(), pkt, pkt + 188);
                        }
                    } else if (tsc == 0 && pid != 0x1FFF) {
                        outChunk.insert(outChunk.end(), pkt, pkt + 188);
                    }
                    pos += 188;
                }
                if (pos > 0) carry.erase(carry.begin(), carry.begin() + pos);
                if (!outChunk.empty()) {
                    std::vector<uint8_t> toSend;
                    toSend.swap(outChunk);
                    outChunk.reserve(URB_SZ);
                    self->PushTsChunk(std::move(toSend));
                }
            } else if (rr == -116) {
                // timeout: URB still pending
            } else {
                g_usb.usb_submit_async(ctx[cur], (char*)tsBuf[cur], URB_SZ);
            }
            cur = (cur + 1) % NUM_URB;
        }

        for (int i = 0; i < NUM_URB; i++) {
            if (ctx[i]) {
                if (g_usb.usb_cancel_async) g_usb.usb_cancel_async(ctx[i]);
                g_usb.usb_reap_async(ctx[i], 50);
                g_usb.usb_free_async(&ctx[i]);
            }
            if (tsBuf[i]) free(tsBuf[i]);
        }
        return 0;
    }

    void StopStreaming() {
        if (m_streaming) {
            // Stop Unit8 WHILE reader thread is still draining EP83 so EP81 never stalls
            uint8_t pauseVal = 2, acqVal = 1, stopVal = 0;
            SendCommand11(8, 1, 3, &pauseVal, 1);
            SendCommand11(8, 1, 3, &acqVal, 1);
            SendCommand11(8, 1, 3, &stopVal, 1);
            m_streaming = false;
        }
        if (m_hReaderThread) {
            InterlockedExchange(&m_readerStop, 1);
            WaitForSingleObject(m_hReaderThread, 3000);
            CloseHandle(m_hReaderThread);
            m_hReaderThread = nullptr;
        }
    }

    void CloseTunerInternal() {
        if (m_hWdThread) {
            InterlockedExchange(&m_wdStop, 1);
            if (m_hWdStopEvent) SetEvent(m_hWdStopEvent);
            WaitForSingleObject(m_hWdThread, 3000);
            CloseHandle(m_hWdThread);
            m_hWdThread = nullptr;
        }
        if (m_ivdrmOk && m_hCtrl) {
            if (m_debug) {
                uint32_t st = 0; SendCommand11(9, 1, 31, &st, 4);
                fprintf(stderr, "[VZ] CloseTuner before CloseSession: B-CAS status=%u\n", st); fflush(stderr);
            }
            uint8_t closeBuf[9] = {};
            *(uint32_t*)(closeBuf + 1) = m_activeSid;
            SendCommand11(9, 1, 5, closeBuf, 9);
            if (m_debug) {
                uint32_t st = 0; SendCommand11(9, 1, 31, &st, 4);
                fprintf(stderr, "[VZ] CloseTuner after CloseSession(%u): dwRes=0x%08X B-CAS status=%u\n",
                        m_activeSid, *(uint32_t*)(closeBuf + 5), st); fflush(stderr);
            }
            m_ivdrmOk = false;
        }
        if (m_hStream) {
            g_usb.usb_release_interface(m_hStream, 2);
            g_usb.usb_close(m_hStream);
            m_hStream = nullptr;
        }
        if (m_hCtrl) {
            g_usb.usb_release_interface(m_hCtrl, 0);
            g_usb.usb_close(m_hCtrl);
            m_hCtrl = nullptr;
        }
    }
};

} // anonymous namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) IBonDriver* CreateBonDriver() {
    return new CBonDriverGVMVPVZ();
}
