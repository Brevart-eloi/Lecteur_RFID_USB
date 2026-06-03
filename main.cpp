#ifndef UNICODE
#define UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "ufr_loader.h"
#include "card_reader.h"
#include "api_client.h"

// Identifiants des controles de la fenetre.
enum {
    ID_IP = 1001, ID_PORT, ID_USER, ID_PASS, ID_LOGIN,
    ID_LIST, ID_OWNER, ID_ADD, ID_TOGGLE, ID_DELETE, ID_REFRESH, ID_STATUS
};

static ApiClient          g_api;
static std::vector<Badge> g_badges;
static HWND g_hList, g_hStatus, g_hIp, g_hPort, g_hUser, g_hPass, g_hOwner;

// Le lecteur n'est ouvert qu'au premier scan 
static UfrLoader g_loader;
static bool      g_readerLoaded = false;

// Recupere le texte 
static std::wstring getText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring w(len, 0);
    GetWindowTextW(h, &w[0], len + 1);
    return w;
}
static void setStatus(const std::wstring& s) { SetWindowTextW(g_hStatus, s.c_str()); }

// Charge la DLL et ouvre le lecteur a la premiere utilisation.
static bool ensureReader(std::wstring& err) {
    if (g_readerLoaded) return true;
    if (!g_loader.load("uFCoder-x86.dll")) {
        err = L"uFCoder-x86.dll introuvable";
        return false;
    }
    if (g_loader.ReaderOpen() != UFR_OK) {
        err = L"Lecteur uFR non detecte";
        return false;
    }
    g_readerLoaded = true;
    return true;
}

static int selectedIndex() { return ListView_GetNextItem(g_hList, -1, LVNI_SELECTED); }

static bool doLogin() {
    g_api.host = getText(g_hIp);
    if (g_api.host.empty()) g_api.host = L"172.29.19.193";
    g_api.port = _wtoi(getText(g_hPort).c_str());
    if (g_api.port <= 0) g_api.port = 80;

    std::string err;
    if (!g_api.login(ApiClient::narrow(getText(g_hUser)),
                     ApiClient::narrow(getText(g_hPass)), err)) {
        setStatus(L"Echec login : " + ApiClient::widen(err));
        return false;
    }
    if (g_api.role != "admin")
        setStatus(L"Connecte en tant que '" + ApiClient::widen(g_api.role) +
                  L"un ajout/suppression exige un compte admin");
    else
        setStatus(L"Connecte en admin");
    return true;
}

static void refreshBadges() {
    if (!g_api.isLogged()) { setStatus(L"Connectez-vous d'abord"); return; }
    std::string err;
    if (!g_api.listBadges(g_badges, err)) { setStatus(ApiClient::widen(err)); return; }

    ListView_DeleteAllItems(g_hList);
    for (size_t i = 0; i < g_badges.size(); i++) {
        const Badge& b = g_badges[i];
        std::wstring wid = ApiClient::widen(b.id);
        LVITEMW it = {}; it.mask = LVIF_TEXT; it.iItem = (int)i;
        it.pszText = (LPWSTR)wid.c_str();
        ListView_InsertItem(g_hList, &it);
        std::wstring wuid = ApiClient::widen(b.uid), wown = ApiClient::widen(b.owner);
        ListView_SetItemText(g_hList, (int)i, 1, (LPWSTR)wuid.c_str());
        ListView_SetItemText(g_hList, (int)i, 2, (LPWSTR)wown.c_str());
        ListView_SetItemText(g_hList, (int)i, 3, (LPWSTR)(b.enabled ? L"Actif" : L"Desactive"));
    }
    setStatus(std::to_wstring(g_badges.size()) + L" badge");
}

static void addBadge() {
    if (!g_api.isLogged()) { setStatus(L"Connectez-vous d'abord"); return; }

    std::wstring owner = getText(g_hOwner);
    if (owner.empty()) {
        setStatus(L"Saisissez d'abord le nom du proprietaire");
        SetFocus(g_hOwner);
        return;
    }

    std::wstring rerr;
    if (!ensureReader(rerr)) { setStatus(rerr); return; }

    setStatus(L"Presentez la carte sur le lecteur");
    UpdateWindow(g_hStatus);

    CardReader reader(g_loader);
    std::string uid, type;
    if (!reader.readUidOnce(uid, type, 8000)) {
        setStatus(L"Aucune carte detectee, Reessayez");
        return;
    }

    // /!\ On retire le dernier octet de l'UID (les 2 derniers caracteres hex)
    // Le uFR lit les 4 octets complets, mais le lecteur de porte (Wiegand-26,
    // 24 bits) n'en garde que 3. Sans ca, un badge ajoute ici ne serait pas
    // reconnu par la porte. On passe aussi en minuscules pour coller au format
    // deja present en base (ex : "6220eb").
    if (uid.size() >= 2) uid = uid.substr(0, uid.size() - 2);
    for (char& c : uid) if (c >= 'A' && c <= 'F') c += ('a' - 'A');

    std::string err;
    if (g_api.addBadge(uid, ApiClient::narrow(owner), err)) {
        setStatus(L"Badge ajoute : " + ApiClient::widen(uid) + L" (" + owner + L")");
        SetWindowTextW(g_hOwner, L"");   // pret pour le badge suivant
        refreshBadges();
    } else {
        setStatus(L"UID " + ApiClient::widen(uid) + L" - " + ApiClient::widen(err));
    }
}

static void toggleBadge() {
    int i = selectedIndex();
    if (i < 0 || i >= (int)g_badges.size()) { setStatus(L"Selectionnez un badge"); return; }
    Badge& b = g_badges[i];
    std::string err;
    std::string key = b.id.empty() ? b.uid : b.id;
    if (g_api.setEnabled(key, !b.enabled, err)) refreshBadges();
    else setStatus(ApiClient::widen(err));
}

static void deleteBadge(HWND parent) {
    int i = selectedIndex();
    if (i < 0 || i >= (int)g_badges.size()) { setStatus(L"Selectionnez un badge"); return; }
    Badge& b = g_badges[i];
    std::wstring msg = L"Supprimer le badge " + ApiClient::widen(b.uid) +
                       L" (" + ApiClient::widen(b.owner) + L") ?";
    if (MessageBoxW(parent, msg.c_str(), L"Confirmation", MB_YESNO | MB_ICONWARNING) != IDYES) return;
    std::string err;
    std::string key = b.id.empty() ? b.uid : b.id;
    if (g_api.deleteBadge(key, err)) { setStatus(L"Badge supprime"); refreshBadges(); }
    else setStatus(ApiClient::widen(err));
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        HINSTANCE hi = GetModuleHandleW(nullptr);
        auto lbl = [&](const wchar_t* t, int x, int y, int cx) {
            CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y, cx, 18, h, nullptr, hi, nullptr);
        };
        // ES_AUTOHSCROLL est important : sans lui, un champ mono-ligne refuse
        // les caracteres des qu'on atteint son bord (mot de passe tronque !).
        const DWORD edStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
        lbl(L"Proxy :", 10, 14, 45);
        g_hIp   = CreateWindowW(L"EDIT", L"172.29.19.193", edStyle, 55, 10, 120, 24, h, (HMENU)ID_IP, hi, nullptr);
        lbl(L":", 178, 14, 8);
        g_hPort = CreateWindowW(L"EDIT", L"80", edStyle | ES_NUMBER, 188, 10, 45, 24, h, (HMENU)ID_PORT, hi, nullptr);
        lbl(L"Login :", 245, 14, 42);
        g_hUser = CreateWindowW(L"EDIT", L"", edStyle, 290, 10, 100, 24, h, (HMENU)ID_USER, hi, nullptr);
        lbl(L"Mot de passe :", 400, 14, 88);
        g_hPass = CreateWindowW(L"EDIT", L"", edStyle | ES_PASSWORD, 488, 10, 100, 24, h, (HMENU)ID_PASS, hi, nullptr);
        CreateWindowW(L"BUTTON", L"Se connecter", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 598, 9, 110, 26, h, (HMENU)ID_LOGIN, hi, nullptr);

        g_hList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                10, 48, 700, 300, h, (HMENU)ID_LIST, hi, nullptr);
        ListView_SetExtendedListViewStyle(g_hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        auto col = [&](int i, const wchar_t* t, int cx) {
            LVCOLUMNW c = {}; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            c.pszText = (LPWSTR)t; c.cx = cx; c.iSubItem = i;
            ListView_InsertColumn(g_hList, i, &c);
        };
        col(0, L"ID", 50); col(1, L"UID", 210); col(2, L"Proprietaire", 320); col(3, L"Etat", 110);

        // Ajout d'un badge : nom du proprietaire + scan.
        lbl(L"Proprietaire :", 10, 360, 80);
        g_hOwner = CreateWindowW(L"EDIT", L"", edStyle, 92, 356, 230, 24, h, (HMENU)ID_OWNER, hi, nullptr);
        CreateWindowW(L"BUTTON", L"Ajouter (scanner)", WS_CHILD | WS_VISIBLE, 332, 354, 160, 28, h, (HMENU)ID_ADD, hi, nullptr);

        // Actions sur le badge selectionne dans la liste.
        CreateWindowW(L"BUTTON", L"Activer / Desactiver", WS_CHILD | WS_VISIBLE, 10,  392, 160, 28, h, (HMENU)ID_TOGGLE,  hi, nullptr);
        CreateWindowW(L"BUTTON", L"Supprimer",            WS_CHILD | WS_VISIBLE, 180, 392, 110, 28, h, (HMENU)ID_DELETE,  hi, nullptr);
        CreateWindowW(L"BUTTON", L"Rafraichir",           WS_CHILD | WS_VISIBLE, 300, 392, 110, 28, h, (HMENU)ID_REFRESH, hi, nullptr);

        g_hStatus = CreateWindowW(L"STATIC", L"Pret. Renseignez un compte admin puis 'Se connecter'.",
                                  WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 10, 430, 700, 20, h, (HMENU)ID_STATUS, hi, nullptr);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_LOGIN:   if (doLogin()) refreshBadges(); break;
        case ID_REFRESH: refreshBadges(); break;
        case ID_ADD:     addBadge(); break;
        case ID_TOGGLE:  toggleBadge(); break;
        case ID_DELETE:  deleteBadge(h); break;
        }
        return 0;
    case WM_DESTROY:
        if (g_readerLoaded && g_loader.ReaderClose) g_loader.ReaderClose();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int main() {
    // Masque la console (le projet reste en subsystem Console)
    HWND con = GetConsoleWindow();
    if (con) ShowWindow(con, SW_HIDE);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = L"RfidUserManager";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND h = CreateWindowW(L"RfidUserManager", L"Gestion des utilisateurs RFID",
                           WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 740, 500, nullptr, nullptr, hi, nullptr);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return 0;
}
