# uFR Classic - Lecteur NFC en C++ (Windows)

## Structure du projet

```
ufr_reader/
-- main.cpp           point d'entree
-- ufr_loader.h       chargement dynamique de uFCoder.dll
-- card_reader.h      detection et lecture de carte
-- CMakeLists.txt     build avec CMake
-- README.md
```

---

## Prerequis

### 1. SDK Digital Logic (uFCoder)
Telechargez le SDK gratuit sur le site officiel :
 https://www.d-logic.com/nfc-rfid-reader-sdk/

Choisissez **"Windows C/C++ SDK"**.  
Vous aurez besoin du fichier :
- `uFCoder-x86.dll` (pour un build 32-bit)
- ou `uFCoder-x64.dll` (pour un build 64-bit)

### 2. Drivers FTDI
Le uFR Classic communique via FTDI USB.  
Telechargez et installez : https://ftdichip.com/drivers/d2xx-drivers/

### 3. Outils de build
- **CMake**  3.16 : https://cmake.org/download/
- **MinGW-w64** (GCC pour Windows) ou **Visual Studio 2019+**

---

## Compilation

### Avec CMake + MinGW
```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### Avec CMake + Visual Studio
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A Win32   # pour 32-bit (uFCoder-x86)
cmake --build . --config Release
```

---

## Lancement

1. Copiez `uFCoder-x86.dll` dans le meme dossier que `ufr_reader.exe`
2. Branchez le uFR Classic
3. Lancez l'executable :
```
ufr_reader.exe
```
4. Approchez une carte NFC  les informations s'affichent dans la console

---

## Exemple de sortie

```
=== uFR Classic Reader - C++ Demo ===

[OK] DLL chargee avec succes.
[OK] Lecteur ouvert.
  Type lecteur  : 0xD1180022
  N serie      : 0x12345678
  Firmware      : v5.0
  Hardware      : v1.5

--- Approchez une carte NFC (Ctrl+C pour quitter) ---

- Carte detectee -----------------------------
|  Type    : MIFARE Classic 1K
|  UID     : 0xA1B2C3D4
|  Bloc 00 : A1 B2 C3 D4 44 08 04 00 62 63 64 65 66 67 68 69
|  Bloc 01 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
-----------------------------------------------
```

---

## Adapter la cle MIFARE

Si vos cartes ont une cle personnalisee, modifiez dans `card_reader.h` :

```cpp
uint8_t defaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // cle par defaut
// Exemples d'autres cles courantes :
// uint8_t defaultKey[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}; // MAD key
// uint8_t defaultKey[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}; // NDEF key
```

---

## Architecture : pourquoi LoadLibrary ?

Le projet charge `uFCoder.dll` **dynamiquement** (sans `.lib`) via `LoadLibrary` / `GetProcAddress`.  
Avantages :
- Pas besoin du `.lib` de l'editeur de liens
- Compatible 32-bit et 64-bit selon la DLL copiee
- Meilleure gestion des erreurs si la DLL est absente
