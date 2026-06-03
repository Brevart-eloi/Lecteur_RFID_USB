#pragma once
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <array>
#include <thread>
#include <chrono>
#include <windows.h>
#include "ufr_loader.h"
#include "db_client.h"

static std::string bytesToHex(const uint8_t* data, size_t len)
{
    std::ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        if (i > 0) oss << ' ';
        oss << std::hex << std::uppercase
            << std::setw(2) << std::setfill('0')
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

static std::string cardTypeName(uint8_t t)
{
    switch (t) {
    case 0x08: return "MIFARE Classic 1K";
    case 0x18: return "MIFARE Classic 4K";
    case 0x44: return "MIFARE Ultralight / NTAG";
    case 0x20: return "MIFARE DESFire";
    default:
        std::ostringstream o;
        o << "Inconnu (0x" << std::hex << std::uppercase
            << std::setw(2) << std::setfill('0') << (int)t << ')';
        return o.str();
    }
}

// ---------------------------------------------------------------
// Classe principale
// ---------------------------------------------------------------
class CardReader
{
    UfrLoader& m_loader;

public:
    explicit CardReader(UfrLoader& loader) : m_loader(loader) {}

    void printReaderInfo()
    {
        uint32_t readerType = 0, serialNum = 0;
        uint8_t  fwMaj = 0, fwMin = 0, hwMaj = 0, hwMin = 0;

        m_loader.GetReaderType(&readerType);
        m_loader.GetReaderSerialNumber(&serialNum);
        m_loader.GetReaderFirmwareVersion(&fwMaj, &fwMin);
        m_loader.GetReaderHardwareVersion(&hwMaj, &hwMin);

        std::cout << "  Type lecteur  : 0x" << std::hex << std::uppercase << readerType << "\n"
            << "  N serie       : 0x" << std::hex << std::uppercase << serialNum << "\n"
            << "  Firmware      : v" << std::dec << (int)fwMaj << '.' << (int)fwMin << "\n"
            << "  Hardware      : v" << std::dec << (int)hwMaj << '.' << (int)hwMin << "\n";
    }

    // Attend qu'une carte soit presentee (jusqu'a timeoutMs) et renvoie son UID
    // en hexa, d'un bloc (ex: "A1B2C3D4"). Utilise pour l'ajout depuis la GUI.
    // Renvoie false si rien n'est lu a temps.
    bool readUidOnce(std::string& uidHex, std::string& typeName, int timeoutMs = 8000)
    {
        const int stepMs = 200;
        for (int elapsed = 0; elapsed <= timeoutMs; elapsed += stepMs) {
            uint8_t cardType = 0, uid[10] = { 0 }, uidLen = 0;
            UFR_STATUS status = m_loader.GetCardIdEx(&cardType, uid, &uidLen);
            if (status == UFR_OK && uidLen > 0) {
                std::ostringstream oss;
                for (uint8_t i = 0; i < uidLen; i++)
                    oss << std::hex << std::uppercase
                        << std::setw(2) << std::setfill('0') << (int)uid[i];
                uidHex   = oss.str();
                typeName = cardTypeName(cardType);
                if (m_loader.ReaderUISignal) m_loader.ReaderUISignal(1, 1);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        }
        return false;
    }

    void readLoop()
    {
        uint8_t lastUid[10] = { 0 };
        uint8_t lastUidLen = 0;

        while (true)
        {
            uint8_t cardType = 0;
            uint8_t uid[10] = { 0 };
            uint8_t uidLen = 0;

            UFR_STATUS status = m_loader.GetCardIdEx(&cardType, uid, &uidLen);

            if (status == UFR_OK)
            {
                bool isNew = (uidLen != lastUidLen)
                    || (memcmp(uid, lastUid, uidLen) != 0);

                if (isNew)
                {
                    memcpy(lastUid, uid, uidLen);
                    lastUidLen = uidLen;

                    std::cout << "Carte detectee :\n";
                    std::cout << "Type : " << cardTypeName(cardType) << "\n";
                    std::cout << "UID  : " << bytesToHex(uid, uidLen)
                        << " (" << (int)uidLen << " octets)\n";
                    std::cout << "----------------------------------------------------------\n\n";

                    m_loader.ReaderUISignal(1, 1);
                }
            }
            else if (status == UFR_NO_CARD)
            {
                if (lastUidLen != 0) {
                    std::cout << "La carte a ete retiree.\n\n";
                    lastUidLen = 0;
                    memset(lastUid, 0, sizeof(lastUid));
                }
            }
            else
            {
                std::cerr << "[WARN] GetCardIdEx() -> 0x"
                    << std::hex << std::uppercase << status << "\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }
};