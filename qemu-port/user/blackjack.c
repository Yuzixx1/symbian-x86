/*
 * blackjack.c -- BLACKJACK, a genuine interactive end-user game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A single 52-card deck is shuffled from a fixed-seed LCG (deterministic, so
 * every hand is reproducible).  Cards are drawn alternately to player and
 * dealer; Aces count as 1 or 11 and face cards as 10.  Commands are read from
 * PortCon through read_line(): new, hit, stand, board, selftest, quit.  The
 * built-in selftest plays one full scripted hand (new / hit / stand) that ends
 * in a WIN: the player reaches 17, the dealer draws to a bust.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*blackjack_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&blackjack_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* Card encoding: 0..51, rank = card % 13 (0=A .. 12=K), suit = card / 13. */
#define HSIZE 13

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned add_text(char* out, unsigned n, const char* s)
{
    while (*s) out[n++] = *s++;
    return n;
}

static unsigned add_uint(char* out, unsigned n, unsigned v)
{
    char d[12];
    int k = 0;
    if (!v) d[k++] = '0';
    while (v) { d[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) out[n++] = d[--k];
    return n;
}

/* Short card label: A,2..9,10,J,Q,K followed by C/D/H/S. */
static unsigned add_card(char* out, unsigned n, int card)
{
    int r = card % HSIZE;
    if      (r == 0)  out[n++] = 'A';
    else if (r <= 8)  out[n++] = (char)('1' + r);   /* 1 -> '2' ... 8 -> '9' */
    else if (r == 9)  { out[n++] = '1'; out[n++] = '0'; }
    else if (r == 10) out[n++] = 'J';
    else if (r == 11) out[n++] = 'Q';
    else              out[n++] = 'K';
    out[n++] = "CDHS"[card / HSIZE];
    return n;
}

/* ------------------------------------------------------------ game engine */
static unsigned char g_deck[52];
static int g_dpos;
static unsigned char g_player[12], g_dealer[12];
static int g_pn, g_dn;
static int g_over;     /* 0 = playing, 1 = hand finished */
static int g_result;   /* 1 = BUST, 2 = WIN, 3 = LOSE, 4 = PUSH */

/* Best total, reducing an Ace from 11 to 1 while the hand is over 21. */
static int hand_value(const unsigned char* hand, int n)
{
    int sum = 0, aces = 0, i;
    for (i = 0; i < n; ++i)
        {
        int r = hand[i] % HSIZE;
        if (r == 0)      { sum += 11; ++aces; }
        else if (r >= 10) sum += 10;
        else              sum += r + 1;
        }
    while (sum > 21 && aces) { sum -= 10; --aces; }
    return sum;
}

/* Fixed-seed LCG Fisher-Yates shuffle: seed 0xDEAD gives the scripted hand
 * 2H/5C vs 4D/KC, hit JH -> 17, dealer draws 10C -> 24 (bust) -> WIN. */
static void shuffle_deck(void)
{
    unsigned seed = 0xDEADu;
    int i;
    for (i = 0; i < 52; ++i) g_deck[i] = (unsigned char)i;
    for (i = 51; i > 0; --i)
        {
        unsigned j;
        unsigned char t;
        seed = seed * 1103515245u + 12345u;   /* wraps mod 2^32 */
        j = ((seed >> 16) & 0x7fffu) % (unsigned)(i + 1);
        t = g_deck[i]; g_deck[i] = g_deck[j]; g_deck[j] = t;
        }
    g_dpos = 0;
}

static int draw(void)
{
    if (g_dpos >= 52) g_dpos = 0;   /* reshuffle safety: should never happen */
    return g_deck[g_dpos++];
}

/* ------------------------------------------------------------ markers */
static void marker_player(void)
{
    char text[96];
    unsigned n = 0;
    int i;
    n = add_text(text, n, "BJ: player [");
    for (i = 0; i < g_pn; ++i)
        { if (i) text[n++] = ' '; n = add_card(text, n, g_player[i]); }
    n = add_text(text, n, "] total=");
    n = add_uint(text, n, (unsigned)hand_value(g_player, g_pn));
    text[n] = 0;
    marker(text);
}

/* One dealer card visible, the hole card hidden. */
static void marker_dealer_show(void)
{
    char text[48];
    unsigned n = 0;
    n = add_text(text, n, "BJ: dealer [");
    n = add_card(text, n, g_dealer[0]);
    n = add_text(text, n, ",?]");
    text[n] = 0;
    marker(text);
}

/* Full dealer reveal once the hand is over. */
static void marker_dealer_full(void)
{
    char text[96];
    unsigned n = 0;
    int i;
    n = add_text(text, n, "BJ: dealer [");
    for (i = 0; i < g_dn; ++i)
        { if (i) text[n++] = ' '; n = add_card(text, n, g_dealer[i]); }
    n = add_text(text, n, "] total=");
    n = add_uint(text, n, (unsigned)hand_value(g_dealer, g_dn));
    text[n] = 0;
    marker(text);
}

static void marker_hit(int card, int total)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "BJ: hit ");
    n = add_card(text, n, card);
    n = add_text(text, n, " total=");
    n = add_uint(text, n, (unsigned)total);
    text[n] = 0;
    marker(text);
}

static void finish(int result)
{
    if (g_over) return;
    g_over = 1;
    g_result = result;
    if (result != 1) marker_dealer_full();   /* no reveal when player busts */
    if      (result == 1) marker("BJ: BUST");
    else if (result == 2) marker("BJ: WIN");
    else if (result == 3) marker("BJ: LOSE");
    else                  marker("BJ: PUSH");
}

static void check_natural(void)
{
    int pt = hand_value(g_player, g_pn);
    int dt = hand_value(g_dealer, g_dn);
    if (g_pn == 2 && pt == 21)
        finish(dt == 21 ? 4 : 2);   /* natural 21: push or win */
}

static void new_game(void)
{
    shuffle_deck();
    g_pn = 0; g_dn = 0; g_over = 0; g_result = 0;
    g_player[g_pn++] = (unsigned char)draw();   /* player */
    g_dealer[g_dn++] = (unsigned char)draw();   /* dealer */
    g_player[g_pn++] = (unsigned char)draw();   /* player */
    g_dealer[g_dn++] = (unsigned char)draw();   /* dealer */
    marker_player();
    marker_dealer_show();
    check_natural();
}

static void do_hit(void)
{
    int card, total;
    if (g_over) return;
    card = draw();
    g_player[g_pn++] = (unsigned char)card;
    total = hand_value(g_player, g_pn);
    marker_hit(card, total);
    if (total > 21) finish(1);
}

static void do_stand(void)
{
    int pt, dt;
    if (g_over) return;
    while (hand_value(g_dealer, g_dn) < 17 && g_dn < 12)
        g_dealer[g_dn++] = (unsigned char)draw();
    pt = hand_value(g_player, g_pn);
    dt = hand_value(g_dealer, g_dn);
    if (dt > 21 || pt > dt) finish(2);
    else if (pt < dt)       finish(3);
    else                    finish(4);
}

static void do_board(void)
{
    marker_player();
    if (g_over) marker_dealer_full();
    else        marker_dealer_show();
}

/* One full scripted hand: new, hit to 17, stand; dealer busts -> WIN. */
static void selftest(void)
{
    marker("BJ: SELFTEST");
    new_game();
    do_hit();
    do_stand();
    marker(g_result == 2 ? "BJ: SELFTEST OK" : "BJ: SELFTEST FAIL");
}

/* ------------------------------------------------------------ command loop */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = word[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

static int run_command(const char* input)
{
    const char* word;
    unsigned wl;
    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;

    if (word_is(word, wl, "new"))    { new_game(); return 0; }
    if (word_is(word, wl, "hit"))    { do_hit(); return 0; }
    if (word_is(word, wl, "stand"))  { do_stand(); return 0; }
    if (word_is(word, wl, "board"))  { do_board(); return 0; }
    if (word_is(word, wl, "selftest")) { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q")) return 1;
    marker("BJ: commands: new hit stand board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("BJ: console busy"); return 1; }

    marker("BJ: BLACKJACK");
    new_game();

    for (;;)
        {
        print("BJ> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;
        if (run_command(line)) break;      /* quit */
        }

    marker("BJ: DONE");
    con_release();
    return 0;
}
