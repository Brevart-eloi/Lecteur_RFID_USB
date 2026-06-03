#pragma once
// Dialogue avec le proxy node.js : on se connecte (login JWT), puis on gere les
// badges via les routes /rfid. Pour memoire, les routes utilisees :
//     POST   /login      -> { token, role }
//     GET    /rfid       -> { badges: [ {id, uid, owner, enabled, ...} ] }
//     POST   /rfid       -> ajout         (reserve admin)
//     PATCH  /rfid/:id   -> { enabled }   (reserve admin)
//     DELETE /rfid/:id                    (reserve admin)
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#pragma comment(lib, "winhttp.lib")

struct Badge {
    std::string id;
    std::string uid;
    std::string owner;
    bool        enabled = true;
};

class ApiClient
{
public:
    std::wstring host = L"172.29.19.193";
    int          port = 80;
    std::string  token;        // JWT (UTF-8) renseigne apres login()
    std::string  role;

    bool isLogged() const { return !token.empty(); }

    // Se connecte et garde le token JWT. Renvoie false (+ err) en cas d'echec.
    bool login(const std::string& user, const std::string& pass, std::string& err) {
        std::string body = "{\"username\":\"" + jsonEscape(user) +
                           "\",\"password\":\"" + jsonEscape(pass) + "\"}";
        int st = 0;
        std::string r = request(L"POST", L"/login", body, false, &st);
        if (st == 0) { err = "Connexion impossible au proxy."; return false; }
        std::string t = jsonStr(r, "token");
        if (t.empty()) { err = jsonStr(r, "message"); if (err.empty()) err = "Login refuse."; return false; }
        token = t;
        role  = jsonStr(r, "role");
        return true;
    }

    bool listBadges(std::vector<Badge>& out, std::string& err) {
        int st = 0;
        std::string r = request(L"GET", L"/rfid", "", true, &st);
        if (st != 200) { err = httpErr("GET /rfid", st, r); return false; }
        parseBadges(r, out);
        return true;
    }

    bool addBadge(const std::string& uid, const std::string& owner, std::string& err) {
        std::string body = "{\"uid\":\"" + jsonEscape(uid) +
                          "\",\"owner\":\"" + jsonEscape(owner) + "\",\"enabled\":true}";
        int st = 0;
        std::string r = request(L"POST", L"/rfid", body, true, &st);
        if (st == 200) return true;
        if (st == 409) { err = "Ce badge existe deja."; return false; }
        err = httpErr("POST /rfid", st, r);
        return false;
    }

    bool setEnabled(const std::string& idOrUid, bool enabled, std::string& err) {
        std::string body = std::string("{\"enabled\":") + (enabled ? "true" : "false") + "}";
        int st = 0;
        std::string r = request(L"PATCH", L"/rfid/" + widen(idOrUid), body, true, &st);
        if (st == 200) return true;
        err = httpErr("PATCH /rfid", st, r);
        return false;
    }

    bool deleteBadge(const std::string& idOrUid, std::string& err) {
        int st = 0;
        std::string r = request(L"DELETE", L"/rfid/" + widen(idOrUid), "", true, &st);
        if (st == 200) return true;
        err = httpErr("DELETE /rfid", st, r);
        return false;
    }

    // Conversions UTF-8 <-> UTF-16 (la fenetre s'en sert aussi).
    static std::wstring widen(const std::string& s) {
        if (s.empty()) return L"";
        int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(n, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
        return w;
    }
    static std::string narrow(const std::wstring& w) {
        if (w.empty()) return "";
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
        return s;
    }

private:
    // Envoie une requete et renvoie le corps de la reponse.
    // status recoit le code HTTP (0 = le proxy n'a pas repondu).
    std::string request(const wchar_t* method, const std::wstring& path,
                        const std::string& body, bool auth, int* status) {
        if (status) *status = 0;
        std::string result;
        HINTERNET hS = WinHttpOpen(L"RFID-Manager/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hS) return result;
        HINTERNET hC = WinHttpConnect(hS, host.c_str(), (INTERNET_PORT)port, 0);
        if (!hC) { WinHttpCloseHandle(hS); return result; }
        HINTERNET hR = WinHttpOpenRequest(hC, method, path.c_str(), nullptr,
                                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hR) { WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return result; }

        std::wstring headers = L"Content-Type: application/json\r\n";
        if (auth && !token.empty())
            headers += L"Authorization: Bearer " + widen(token) + L"\r\n";

        BOOL ok = WinHttpSendRequest(hR, headers.c_str(), (DWORD)-1,
                                     (LPVOID)body.data(), (DWORD)body.size(),
                                     (DWORD)body.size(), 0);
        if (ok) ok = WinHttpReceiveResponse(hR, nullptr);
        if (ok) {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(hR, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
            if (status) *status = (int)code;
            DWORD avail = 0;
            do {
                avail = 0;
                if (!WinHttpQueryDataAvailable(hR, &avail) || !avail) break;
                std::string buf(avail, 0);
                DWORD read = 0;
                WinHttpReadData(hR, &buf[0], avail, &read);
                result.append(buf.data(), read);
            } while (avail > 0);
        }
        WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
        return result;
    }

    static std::string httpErr(const char* what, int st, const std::string& body) {
        std::string e = std::string(what) + " : HTTP " + std::to_string(st);
        std::string msg = jsonStr(body, "error");
        if (msg.empty()) msg = jsonStr(body, "message");
        if (!msg.empty()) e += " - " + msg;
        return e;
    }

    // Echappe les guillemets et backslash avant de glisser une valeur dans du JSON.
    static std::string jsonEscape(const std::string& s) {
        std::string o;
        for (char c : s) {
            if (c == '"' || c == '\\') o += '\\';
            o += c;
        }
        return o;
    }

    // Les reponses du proxy sont simples, donc on evite une lib JSON : ces deux
    // helpers recuperent a la main une valeur texte ou numerique d'une cle.
    static std::string jsonStr(const std::string& j, const std::string& key, size_t from = 0) {
        std::string pat = "\"" + key + "\"";
        size_t k = j.find(pat, from);
        if (k == std::string::npos) return "";
        size_t c = j.find(':', k + pat.size());
        if (c == std::string::npos) return "";
        size_t i = c + 1;
        while (i < j.size() && (j[i] == ' ' || j[i] == '\t')) i++;
        if (i >= j.size() || j[i] != '"') return "";
        i++;
        std::string out;
        while (i < j.size() && j[i] != '"') {
            if (j[i] == '\\' && i + 1 < j.size()) i++;
            out += j[i++];
        }
        return out;
    }
    static std::string jsonNum(const std::string& j, const std::string& key, size_t from = 0) {
        std::string pat = "\"" + key + "\"";
        size_t k = j.find(pat, from);
        if (k == std::string::npos) return "";
        size_t c = j.find(':', k + pat.size());
        if (c == std::string::npos) return "";
        size_t i = c + 1;
        while (i < j.size() && (j[i] == ' ' || j[i] == '\t')) i++;
        std::string out;
        while (i < j.size() && (isalnum((unsigned char)j[i]) || j[i] == '.' || j[i] == '-'))
            out += j[i++];
        return out;
    }

    // Parcourt le tableau "badges" et reconstruit un Badge par objet rencontre.
    static void parseBadges(const std::string& resp, std::vector<Badge>& out) {
        out.clear();
        size_t arr = resp.find("\"badges\"");
        size_t p = (arr == std::string::npos) ? resp.find('[') : resp.find('[', arr);
        if (p == std::string::npos) return;
        int depth = 0; size_t objStart = std::string::npos;
        for (size_t i = p; i < resp.size(); i++) {
            char ch = resp[i];
            if (ch == '{') { if (depth == 0) objStart = i; depth++; }
            else if (ch == '}') {
                if (--depth == 0 && objStart != std::string::npos) {
                    std::string obj = resp.substr(objStart, i - objStart + 1);
                    Badge b;
                    b.id    = jsonNum(obj, "id");
                    b.uid   = jsonStr(obj, "uid");
                    b.owner = jsonStr(obj, "owner");
                    std::string en = jsonNum(obj, "enabled");
                    b.enabled = (en == "1" || en == "true");
                    if (!b.id.empty() || !b.uid.empty()) out.push_back(b);
                    objStart = std::string::npos;
                }
            }
            else if (ch == ']' && depth == 0) break;
        }
    }
};
