cmake_minimum_required(VERSION 3.16)
project(Lecteur_RFID_USB CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Sources ------------------------------------------------------------------
add_executable(Lecteur_RFID_USB
    main.cpp
    # card_reader.h et ufr_loader.h sont header-only, pas besoin de les lister ici
)

# --- Windows : on lie uniquement les DLL systeme (la uFCoder.dll est chargee
#     dynamiquement avec LoadLibrary, pas besoin de .lib)  --------------------
if(WIN32)
    target_link_libraries(Lecteur_RFID_USB PRIVATE )
endif()

# --- Copier la DLL uFCoder dans le dossier de build automatiquement ----------
# Decommentez et adaptez le chemin si vous avez le SDK installe :
# set(UFCODER_DLL "${CMAKE_SOURCE_DIR}/sdk/uFCoder-x86.dll")
# add_custom_command(TARGET Lecteur_RFID_USB POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${UFCODER_DLL}"
#         "$<TARGET_FILE_DIR:Lecteur_RFID_USB>/uFCoder-x86.dll"
# )

message(STATUS "=== uFR Reader build configure ===")
message(STATUS "N'oubliez pas de copier uFCoder-x86.dll ou uFCoder-x64.dll")
message(STATUS "dans le meme dossier que l'executable.")
