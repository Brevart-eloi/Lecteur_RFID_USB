#pragma once
#include <windows.h>
#include <wininet.h>
#include <string>
#include <iostream>

#pragma comment(lib, "wininet.lib")

class DbClient
{
public:
    static bool sendUidToServer(const std::string& uid)
    {
        
        std::string server = "172.29.19.193"; 
        int port = 3002;                      
        std::string endpoint = "/rfid/scan";  

        // Construction du JSON
        std::string jsonPayload = "{\"uid\": \"" + uid + "\"}";
        HINTERNET hInternet = InternetOpenA("uFR_Proxy_Client", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) 
            return false;

        HINTERNET hConnect = InternetConnectA(hInternet, server.c_str(), port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            std::cerr << "Impossible de joindre le proxy Node.js sur " << server << ":" << port << "\n";
            InternetCloseHandle(hInternet);
            return false;
        }

        const char* acceptTypes[] = { "application/json", NULL };
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", endpoint.c_str(), NULL, NULL, acceptTypes, INTERNET_FLAG_RELOAD, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return false;
        }

        std::string headers = "Content-Type: application/json\r\n";
        bool success = HttpSendRequestA(hRequest, headers.c_str(), headers.length(), (LPVOID)jsonPayload.c_str(), jsonPayload.length());

        if (success) {
            std::cout << "UID envoye au proxy avec succes.\n";
        }
        else {
            std::cerr << "Echec de l'envoi. Code : " << GetLastError() << "\n";
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

        return success;
    }
};