// RE1HD_ItemBoxAnywhere - Resident Evil HD Remaster (Steam, 32 bits) - ASI.
// Abre o BAU DE ITENS de qualquer lugar: aperte a tecla (padrao I) ou a combinacao de botoes do controle (padrao Start + Back).
//
// Como funciona (descoberto com as sondas): a tela do bau e a tela de inventario sao a MESMA tela (uBhdPrimUI_SubScr);
// so muda o "modo": o inventario normal abre com modo 4 e o bau com modo 5. Quando voce aperta a tecla do mod, ele liga
// um aviso; o proprio jogo, no trecho em que ja trata o botao de inventario (so quando o jogo deixa abrir o inventario),
// recebe o aviso e abre a tela com modo 5 (bau) em vez de 4. Ou seja: onde o jogo nao deixa abrir o inventario
// (cutscene, porta, combate travado...) o mod tambem nao abre.
//
// Compilar (MSVC x86): cl /nologo /LD /O2 /MT /EHsc /D_CRT_SECURE_NO_WARNINGS RE1HD_ItemBoxAnywhere.cpp /Fe:RE1HD_ItemBoxAnywhere.asi /link user32.lib kernel32.lib
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cctype>

// ---------------- configuracao ----------------
static bool   g_enable   = true;
static int    g_vk       = 'I';          // tecla do teclado (0 = desligada)
static DWORD  g_padMask  = 0x0010 | 0x0020;   // Start + Back (mascara XInput); 0 = desligada
static int    g_padLT = 0, g_padRT = 0;  // gatilhos como parte da combinacao
static int    g_timeout  = 800;          // ms que o aviso espera o jogo aceitar
static int    g_cooldown = 600;          // ms minimos entre dois pedidos
static bool   g_sound    = true;         // toca o som de abrir bau
static bool   g_log      = false;
static char   g_logPath[MAX_PATH];
static char   g_iniPath[MAX_PATH];

static void Note(const char* fmt, ...)
{
    if (!g_log) return;
    FILE* f = fopen(g_logPath, "a");
    if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a);
    fputc('\n', f); fclose(f);
}

// ---------------- nomes de teclas / botoes ----------------
static int ParseKey(const char* s)
{
    char t[64]; int n = 0;
    for (const char* p = s; *p && n < 63; ++p) if (!isspace((unsigned char)*p)) t[n++] = (char)toupper((unsigned char)*p);
    t[n] = 0;
    if (!n || !strcmp(t, "0") || !strcmp(t, "NONE") || !strcmp(t, "OFF")) return 0;
    if (n == 1 && ((t[0] >= 'A' && t[0] <= 'Z') || (t[0] >= '0' && t[0] <= '9'))) return t[0];
    if (t[0] == 'F' && n <= 3 && isdigit((unsigned char)t[1])) { int k = atoi(t + 1); if (k >= 1 && k <= 24) return VK_F1 + k - 1; }
    struct N { const char* n; int vk; };
    static const N names[] = {
        {"TAB",VK_TAB},{"ENTER",VK_RETURN},{"SPACE",VK_SPACE},{"ESC",VK_ESCAPE},{"ESCAPE",VK_ESCAPE},{"INSERT",VK_INSERT},{"DELETE",VK_DELETE},
        {"HOME",VK_HOME},{"END",VK_END},{"PAGEUP",VK_PRIOR},{"PAGEDOWN",VK_NEXT},{"BACKSPACE",VK_BACK},
        {"UP",VK_UP},{"DOWN",VK_DOWN},{"LEFT",VK_LEFT},{"RIGHT",VK_RIGHT},
        {"SHIFT",VK_SHIFT},{"CTRL",VK_CONTROL},{"ALT",VK_MENU},{"CAPSLOCK",VK_CAPITAL},
        {"NUMPAD0",VK_NUMPAD0},{"NUMPAD1",VK_NUMPAD1},{"NUMPAD2",VK_NUMPAD2},{"NUMPAD3",VK_NUMPAD3},{"NUMPAD4",VK_NUMPAD4},
        {"NUMPAD5",VK_NUMPAD5},{"NUMPAD6",VK_NUMPAD6},{"NUMPAD7",VK_NUMPAD7},{"NUMPAD8",VK_NUMPAD8},{"NUMPAD9",VK_NUMPAD9},
        {"NUMPADPLUS",VK_ADD},{"NUMPADMINUS",VK_SUBTRACT},{"NUMPADMULTIPLY",VK_MULTIPLY},{"NUMPADDIVIDE",VK_DIVIDE},
        {"MOUSE4",VK_XBUTTON1},{"MOUSE5",VK_XBUTTON2},{"MOUSE3",VK_MBUTTON},
    };
    for (const N& e : names) if (!strcmp(t, e.n)) return e.vk;
    if (t[0] == '0' && t[1] == 'X') return (int)strtol(t, nullptr, 16);
    int v = atoi(t); if (v > 0 && v < 255) return v;
    return -1;
}

static void ParsePad(const char* s)
{
    g_padMask = 0; g_padLT = g_padRT = 0;
    char buf[128]; strncpy(buf, s, 127); buf[127] = 0;
    for (char* tok = strtok(buf, "+, "); tok; tok = strtok(nullptr, "+, "))
    {
        char t[32]; int n = 0;
        for (const char* p = tok; *p && n < 31; ++p) t[n++] = (char)toupper((unsigned char)*p);
        t[n] = 0;
        if (!strcmp(t, "START")) g_padMask |= 0x0010;
        else if (!strcmp(t, "BACK") || !strcmp(t, "SELECT") || !strcmp(t, "VIEW")) g_padMask |= 0x0020;
        else if (!strcmp(t, "A")) g_padMask |= 0x1000;
        else if (!strcmp(t, "B")) g_padMask |= 0x2000;
        else if (!strcmp(t, "X")) g_padMask |= 0x4000;
        else if (!strcmp(t, "Y")) g_padMask |= 0x8000;
        else if (!strcmp(t, "LB") || !strcmp(t, "L1")) g_padMask |= 0x0100;
        else if (!strcmp(t, "RB") || !strcmp(t, "R1")) g_padMask |= 0x0200;
        else if (!strcmp(t, "LS") || !strcmp(t, "L3")) g_padMask |= 0x0040;
        else if (!strcmp(t, "RS") || !strcmp(t, "R3")) g_padMask |= 0x0080;
        else if (!strcmp(t, "LT") || !strcmp(t, "L2")) g_padLT = 1;
        else if (!strcmp(t, "RT") || !strcmp(t, "R2")) g_padRT = 1;
        else if (!strcmp(t, "DPADUP")) g_padMask |= 0x0001;
        else if (!strcmp(t, "DPADDOWN")) g_padMask |= 0x0002;
        else if (!strcmp(t, "DPADLEFT")) g_padMask |= 0x0004;
        else if (!strcmp(t, "DPADRIGHT")) g_padMask |= 0x0008;
        else if (!strcmp(t, "NONE") || !strcmp(t, "OFF") || !strcmp(t, "0")) { g_padMask = 0; g_padLT = g_padRT = 0; return; }
        else Note("PadCombo: botao desconhecido '%s' (ignorado).", tok);
    }
}

static void LoadIni()
{
    char b[128];
    g_enable = GetPrivateProfileIntA("ItemBox", "Enable", 1, g_iniPath) != 0;
    GetPrivateProfileStringA("ItemBox", "Key", "I", b, sizeof b, g_iniPath);
    g_vk = ParseKey(b); if (g_vk < 0) g_vk = 'I';
    GetPrivateProfileStringA("ItemBox", "PadCombo", "Start+Back", b, sizeof b, g_iniPath);
    ParsePad(b);
    g_timeout  = GetPrivateProfileIntA("ItemBox", "RequestTimeoutMs", 800, g_iniPath);
    g_cooldown = GetPrivateProfileIntA("ItemBox", "CooldownMs", 600, g_iniPath);
    g_sound    = GetPrivateProfileIntA("ItemBox", "PlaySound", 1, g_iniPath) != 0;
    g_log      = GetPrivateProfileIntA("ItemBox", "Log", 0, g_iniPath) != 0;
}

// ---------------- patch no jogo ----------------
static volatile uint8_t g_want  = 0;     // pedido de "abrir bau" pendente
static volatile uint8_t g_fired = 0;     // o jogo ja usou o pedido
static volatile uint8_t g_snd   = 1;

static const uintptr_t SITE_JNZ  = 0x48B0CF;   // jnz near 0x48B218  (botao de inventario apertado)
static const uintptr_t SITE_CALL = 0x48B244;   // call 0x4F6F60      (abre a tela de itens com o modo)
static const uintptr_t FN_OPEN   = 0x4F6F60;
static const uintptr_t FN_SOUND  = 0x4EE130;
static const uintptr_t OPEN_INV  = 0x48B218;
static const uintptr_t AFTER_JNZ = 0x48B0D5;

struct Emit
{
    uint8_t* p; int n;
    void b(int v) { p[n++] = (uint8_t)v; }
    void d(uint32_t v) { memcpy(p + n, &v, 4); n += 4; }
    void jmp(uintptr_t to) { b(0xE9); d((uint32_t)(to - ((uintptr_t)p + n + 4))); }
    void call(uintptr_t to) { b(0xE8); d((uint32_t)(to - ((uintptr_t)p + n + 4))); }
};

static bool Match(uintptr_t a, const uint8_t* e, int len)
{
    __try { return memcmp((void*)a, e, len) == 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool Decrypted()
{
    static const uint8_t j[]  = { 0xA8, 0x01, 0x0F, 0x85, 0x43, 0x01, 0x00, 0x00 };   // 0x48B0CD
    static const uint8_t c[]  = { 0xE8, 0x17, 0xBD, 0x06, 0x00 };                      // 0x48B244
    static const uint8_t f[]  = { 0x8B, 0x49, 0x40, 0x85, 0xC9 };                      // 0x4F6F60
    return Match(SITE_JNZ - 2, j, sizeof j) && Match(SITE_CALL, c, sizeof c) && Match(FN_OPEN, f, sizeof f);
}

static bool Install()
{
    uint8_t* mem = (uint8_t*)VirtualAlloc(nullptr, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return false;
    g_snd = g_sound ? 1 : 0;

    // ---- stub 1: no lugar do "jnz near 0x48B218". Se ha pedido pendente, trata como botao de inventario apertado.
    Emit s1 = { mem, 0 };
    s1.b(0x80); s1.b(0x3D); s1.d((uint32_t)(uintptr_t)&g_want); s1.b(0x00);      // cmp byte [g_want],0
    s1.b(0x75); s1.b(0x09);                                                        // jne taken
    s1.b(0xA8); s1.b(0x01);                                                        // test al,1   (al ainda e o bit do botao)
    s1.b(0x75); s1.b(0x05);                                                        // jnz taken
    s1.jmp(AFTER_JNZ);                                                             // nao apertado: segue o fluxo normal
    s1.jmp(OPEN_INV);                                                              // taken

    // ---- stub 2: no lugar do "call 0x4F6F60". Se ha pedido pendente: modo 4 (inventario) -> 5 (bau).
    uint8_t* m2 = mem + 64;
    Emit s2 = { m2, 0 };
    s2.b(0x80); s2.b(0x3D); s2.d((uint32_t)(uintptr_t)&g_want); s2.b(0x00);      // cmp byte [g_want],0
    s2.b(0x74); const int jeOrig = s2.n; s2.b(0x00);                               // je original (preenchido depois)
    s2.b(0xC6); s2.b(0x05); s2.d((uint32_t)(uintptr_t)&g_want); s2.b(0x00);      // g_want = 0
    s2.b(0xC6); s2.b(0x05); s2.d((uint32_t)(uintptr_t)&g_fired); s2.b(0x01);     // g_fired = 1
    s2.b(0xC7); s2.b(0x44); s2.b(0x24); s2.b(0x04); s2.d(5);                       // mov dword [esp+4],5   (modo = bau)
    s2.b(0x80); s2.b(0x3D); s2.d((uint32_t)(uintptr_t)&g_snd); s2.b(0x00);       // cmp byte [g_snd],0
    s2.b(0x74); const int jeSnd = s2.n; s2.b(0x00);                                // je pula_som
    s2.b(0x6A); s2.b(0x07);                                                        // push 7
    s2.b(0x8B); s2.b(0x0D); s2.d(0x00CF2F28);                                      // mov ecx,[0xCF2F28]
    s2.call(FN_SOUND);                                                             // som de abrir bau (limpa a pilha sozinha: ret 4)
    s2.b(0x8B); s2.b(0x0D); s2.d(0x00CF30D8);                                      // mov ecx,[0xCF30D8]  (this da tela de itens)
    const int origAt = s2.n;
    m2[jeSnd] = (uint8_t)(origAt - (jeSnd + 1));
    m2[jeOrig] = (uint8_t)(origAt - (jeOrig + 1));
    s2.jmp(FN_OPEN);                                                               // original (a funcao termina com ret 0x18)
    FlushInstructionCache(GetCurrentProcess(), mem, 256);

    // ---- aplica
    DWORD old;
    if (!VirtualProtect((void*)SITE_JNZ, 6, PAGE_EXECUTE_READWRITE, &old)) return false;
    uint8_t* p = (uint8_t*)SITE_JNZ;
    p[0] = 0xE9; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)mem - (SITE_JNZ + 5)); p[5] = 0x90;
    VirtualProtect(p, 6, old, &old);
    if (!VirtualProtect((void*)SITE_CALL, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    p = (uint8_t*)SITE_CALL;
    p[0] = 0xE8; *(int32_t*)(p + 1) = (int32_t)((uintptr_t)m2 - (SITE_CALL + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)0x48B000, 0x300);
    return true;
}

// ---------------- entrada ----------------
typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT a, b, c, d; } XPAD;
typedef struct { DWORD dwPacketNumber; XPAD Gamepad; } XSTATE;
typedef DWORD (WINAPI *XGetState_t)(DWORD, XSTATE*);

static bool GameFocused()
{
    HWND w = GetForegroundWindow(); DWORD pid = 0;
    if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

static DWORD WINAPI Worker(LPVOID)
{
    // espera a protecao da Steam decifrar o codigo (ate 3 min)
    for (int i = 0; i < 1800 && !Decrypted(); ++i) Sleep(100);
    if (!Decrypted()) { Note("Codigo do jogo diferente do esperado: mod desativado, nada foi alterado."); return 0; }
    Sleep(1000);
    if (!Install()) { Note("Falha ao aplicar o patch."); return 0; }
    Note("[%lu] Patch aplicado. Tecla=%d  PadCombo=0x%04X LT=%d RT=%d", GetTickCount(), g_vk, g_padMask, g_padLT, g_padRT);

    XGetState_t xget = nullptr;
    const char* dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (const char* d : dlls) { HMODULE m = GetModuleHandleA(d); if (!m) m = LoadLibraryA(d); if (m) { xget = (XGetState_t)GetProcAddress(m, "XInputGetState"); if (xget) break; } }

    bool prevKey = false, prevPad = false;
    DWORD lastReq = 0, reqAt = 0;
    for (;;)
    {
        Sleep(5);
        const DWORD now = GetTickCount();
        bool key = false, pad = false;
        if (GameFocused())
        {
            if (g_vk > 0) key = (GetAsyncKeyState(g_vk) & 0x8000) != 0;
            if (xget && (g_padMask || g_padLT || g_padRT))
            {
                for (DWORD u = 0; u < 4 && !pad; ++u)
                {
                    XSTATE st; memset(&st, 0, sizeof st);
                    if (xget(u, &st) != 0) continue;
                    const bool btn = (st.Gamepad.wButtons & g_padMask) == g_padMask;
                    const bool lt = !g_padLT || st.Gamepad.bLeftTrigger > 150, rt = !g_padRT || st.Gamepad.bRightTrigger > 150;
                    pad = btn && lt && rt;
                }
            }
        }
        if (!g_enable) { prevKey = key; prevPad = pad; continue; }

        if (((key && !prevKey) || (pad && !prevPad)) && !g_want && now - lastReq > (DWORD)g_cooldown)
        {
            g_fired = 0; g_want = 1; lastReq = now; reqAt = now;
            Note("[%lu] pedido: abrir bau (%s)", now, key && !prevKey ? "teclado" : "controle");
        }
        prevKey = key; prevPad = pad;

        if (g_fired) { g_fired = 0; Note("[%lu] o jogo abriu a tela com modo 5 (bau) %lu ms depois do pedido.", now, now - reqAt); }
        if (g_want && now - reqAt > (DWORD)g_timeout)
        {
            g_want = 0;
            Note("[%lu] pedido expirou: o jogo nao aceitou abrir o inventario agora (cutscene/porta/combate?).", now);
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hm, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(hm);
    GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    char* d = strrchr(g_iniPath, '.'); if (d) strcpy(d, ".ini");
    strcpy(g_logPath, g_iniPath); d = strrchr(g_logPath, '.'); if (d) strcpy(d, ".log");
    LoadIni();
    if (g_log) { FILE* f = fopen(g_logPath, "w"); if (f) fclose(f); }
    Note("RE1HD_ItemBoxAnywhere carregado.");
    CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    return TRUE;
}
