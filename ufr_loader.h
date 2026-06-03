#pragma once
#include <windows.h>
#include <iostream>
#include <cstdint>


typedef uint32_t UFR_STATUS;

constexpr UFR_STATUS UFR_OK = 0x00;
constexpr UFR_STATUS UFR_NO_CARD = 0x08;

// Types de cartes
constexpr uint8_t CARD_MIFARE_CLASSIC_1K = 0x08;
constexpr uint8_t CARD_MIFARE_CLASSIC_4K = 0x18;
constexpr uint8_t CARD_MIFARE_ULTRALIGHT = 0x44;
constexpr uint8_t CARD_MIFARE_DESFIRE = 0x20;

// Type de cle
constexpr uint8_t MIFARE_KEY_A = 0x60;
constexpr uint8_t MIFARE_KEY_B = 0x61;

constexpr size_t MIFARE_BLOCK_SIZE = 16;


typedef UFR_STATUS(__stdcall* fn_ReaderOpen)();
typedef UFR_STATUS(__stdcall* fn_ReaderClose)();
typedef UFR_STATUS(__stdcall* fn_GetReaderType)(uint32_t*);
typedef UFR_STATUS(__stdcall* fn_GetReaderSerialNumber)(uint32_t*);
typedef UFR_STATUS(__stdcall* fn_GetCardId)(uint8_t*, uint32_t*);


typedef UFR_STATUS(__stdcall* fn_GetCardIdEx)(uint8_t*, uint8_t*, uint8_t*);

typedef UFR_STATUS(__stdcall* fn_GetReaderFirmwareVersion)(uint8_t*, uint8_t*);
typedef UFR_STATUS(__stdcall* fn_GetReaderHardwareVersion)(uint8_t*, uint8_t*);
typedef UFR_STATUS(__stdcall* fn_ReaderUISignal)(uint8_t, uint8_t);


typedef UFR_STATUS(__stdcall* fn_BlockRead_PK)(
    uint8_t*,   // data (out)
    uint8_t,    // block_addr (absolu)
    uint8_t,    // auth_mode
    uint8_t*    // key[6]
    );

typedef UFR_STATUS(__stdcall* fn_BlockWrite_PK)(
    uint8_t*,   // data (in)
    uint8_t,    // block_addr
    uint8_t,    // auth_mode
    uint8_t*    // key[6]
    );


// Classe de chargement

class UfrLoader
{
public:
    HMODULE hDll = nullptr;

    fn_ReaderOpen               ReaderOpen = nullptr;
    fn_ReaderClose              ReaderClose = nullptr;
    fn_GetReaderType            GetReaderType = nullptr;
    fn_GetReaderSerialNumber    GetReaderSerialNumber = nullptr;
    fn_GetCardId                GetCardId = nullptr;  
    fn_GetCardIdEx              GetCardIdEx = nullptr;  
    fn_GetReaderFirmwareVersion GetReaderFirmwareVersion = nullptr;
    fn_GetReaderHardwareVersion GetReaderHardwareVersion = nullptr;
    fn_ReaderUISignal           ReaderUISignal = nullptr;
    fn_BlockRead_PK             BlockRead_PK = nullptr;
    fn_BlockWrite_PK            BlockWrite_PK = nullptr;

    ~UfrLoader() {
        if (hDll) FreeLibrary(hDll);
    }

    bool load(const char* dllPath)
    {
        hDll = LoadLibraryA(dllPath);
        if (!hDll) return false;

#define LOAD_FN(name) \
    name = reinterpret_cast<fn_##name>(GetProcAddress(hDll, #name)); \
    if (!name) { \
        std::cerr << "[ERREUR] Fonction introuvable dans la DLL : " #name "\n"; \
        FreeLibrary(hDll); hDll = nullptr; return false; \
    }

        LOAD_FN(ReaderOpen)
            LOAD_FN(ReaderClose)
            LOAD_FN(GetReaderType)
            LOAD_FN(GetReaderSerialNumber)
            LOAD_FN(GetCardId)
            LOAD_FN(GetCardIdEx)
            LOAD_FN(GetReaderFirmwareVersion)
            LOAD_FN(GetReaderHardwareVersion)
            LOAD_FN(ReaderUISignal)
            LOAD_FN(BlockRead_PK)
            LOAD_FN(BlockWrite_PK)

#undef LOAD_FN
            return true;
    }
};