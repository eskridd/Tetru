#ifdef _WIN32
    #ifndef PDC_WIDE
        #define PDC_WIDE
    #endif
    #ifndef PDC_FORCE_UTF8
        #define PDC_FORCE_UTF8
    #endif
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_SECURE_NO_WARNINGS
    #include <windows.h>
    #include <io.h>
    #ifdef MOUSE_MOVED
        #undef MOUSE_MOVED
    #endif
    #if defined(__has_include)
        #if __has_include(<curses.h>)
            #include <curses.h>
        #elif __has_include(<pdcurses.h>)
            #include <pdcurses.h>
        #elif __has_include(<pdcurses/curses.h>)
            #include <pdcurses/curses.h>
        #elif __has_include(<ncursesw/curses.h>)
            #include <ncursesw/curses.h>
        #elif __has_include(<ncurses/ncurses.h>)
            #include <ncurses/ncurses.h>
        #elif __has_include(<ncurses.h>)
            #include <ncurses.h>
        #else
            #include <curses.h>
        #endif
    #else
        #include <curses.h>
    #endif
#else
    #include <ncurses.h>
    #include <unistd.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

#define TETRU_VERSION "v0.0.3.1 TeBeta"
#define BOARD_W 10
#define BOARD_H 20
#define MAX_PARTICLES 160
#define MENU_STARS 50
#define STATS_FILE "tetru_stats.dat"
#define STATS_MAGIC 0x5445545255535441ULL
#define STATS_VERSION 1
#define HMAC_KEY 0x9e3779b97f4a7c15ULL

static const int BLOCKS[7][4][4][4] = {
    {
        {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
        {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
        {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}
    },
    {
        {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}
    },
    {
        {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}},
        {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    {
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}
    },
    {
        {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}},
        {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
        {{1,0,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    {
        {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}
    },
    {
        {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
        {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}},
        {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
        {{0,1,0,0},{1,1,0,0},{1,0,0,0},{0,0,0,0}}
    }
};

typedef enum {
    STATE_MENU,
    STATE_DIFF_MENU,
    STATE_STATS_VIEW,
    STATE_PLAYING,
    STATE_PAUSED
} GameState;

typedef enum {
    MODE_CLASSIC,
    MODE_SPRINT,
    MODE_SURVIVAL,
    MODE_VS_BOT
} GameMode;

typedef enum {
    DIFF_BEGINNER,
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD,
    DIFF_IMPOSSIBLE
} AiDifficulty;

typedef struct {
    int32_t total_games;
    int32_t classic_high_score;
    int32_t classic_max_lines;
    int32_t sprint_games;
    int32_t sprint_wins;
    double sprint_best_time;
    int32_t survival_high_score;
    int32_t vs_games;
    int32_t vs_wins;
    int32_t vs_losses;
    int32_t total_lines_cleared;
    int32_t total_tetrus;
} StatsData;

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t reserved;
    StatsData data;
    uint64_t checksum;
} StatsFileHeader;

typedef struct {
    int type;
    int rot;
    int x;
    int y;
} Piece;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int life;
    int max_life;
    int color;
    char ch;
} Particle;

typedef struct {
    int board[BOARD_H][BOARD_W];
    Piece current;
    int next_piece;
    int hold_piece;
    bool can_hold;
    int score;
    int lines;
    int level;
    int combo;
    int bag[7];
    int bag_index;
    int pending_garbage;
    double last_drop_time;
    double lock_timer;
    bool is_locking;
    bool is_ai;
    AiDifficulty ai_diff;
    double ai_move_time;
    int target_x;
    int target_rot;
    bool target_calculated;
    Particle particles[MAX_PARTICLES];
    int shake_frames;
    int flash_lines[4];
    int flash_count;
    int flash_timer;
    char banner_text[32];
    int banner_timer;
    int banner_color;
    double mode_start_time;
    double survival_timer;
    bool won;
    bool game_over;
    bool stats_recorded;
} PlayerState;

typedef struct {
    int x;
    int y;
    int speed;
    int color;
    char ch;
} MenuStar;

static GameState game_state = STATE_MENU;
static GameMode game_mode = MODE_CLASSIC;
static AiDifficulty current_diff = DIFF_MEDIUM;
static int menu_selection = 0;
static int diff_selection = 2;
static PlayerState p1;
static PlayerState p2;
static StatsData global_stats;
static MenuStar stars[MENU_STARS];
static int global_tick = 0;
static volatile sig_atomic_t running = 1;

#ifndef _WIN32
static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}
#endif

static uint64_t compute_checksum(const StatsData *d) {
    const uint8_t *bytes = (const uint8_t *)d;
    uint64_t hash = HMAC_KEY;
    for (size_t i = 0; i < sizeof(StatsData); i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= 0x100000001b3ULL;
        hash = (hash << 13) | (hash >> (64 - 13));
    }
    return hash;
}

static void sanitize_stats(StatsData *d) {
    if (d->total_games < 0 || d->total_games > 10000000) d->total_games = 0;
    if (d->classic_high_score < 0 || d->classic_high_score > 500000000) d->classic_high_score = 0;
    if (d->classic_max_lines < 0 || d->classic_max_lines > 100000) d->classic_max_lines = 0;
    if (d->sprint_games < 0 || d->sprint_games > 1000000) d->sprint_games = 0;
    if (d->sprint_wins < 0 || d->sprint_wins > d->sprint_games) d->sprint_wins = 0;
    if (d->sprint_best_time < 5.0 || d->sprint_best_time > 9999.0) d->sprint_best_time = 9999.0;
    if (d->survival_high_score < 0 || d->survival_high_score > 500000000) d->survival_high_score = 0;
    if (d->vs_games < 0 || d->vs_games > 1000000) d->vs_games = 0;
    if (d->vs_wins < 0 || d->vs_wins > d->vs_games) d->vs_wins = 0;
    if (d->vs_losses < 0 || d->vs_losses > d->vs_games) d->vs_losses = 0;
    if (d->total_lines_cleared < 0 || d->total_lines_cleared > 100000000) d->total_lines_cleared = 0;
    if (d->total_tetrus < 0 || d->total_tetrus > d->total_lines_cleared) d->total_tetrus = 0;
}

static void load_stats(void) {
    memset(&global_stats, 0, sizeof(global_stats));
    global_stats.sprint_best_time = 9999.0;

    int fd = open(STATS_FILE, O_RDONLY);
    if (fd < 0) return;

    StatsFileHeader header;
    ssize_t bytes_read = read(fd, &header, sizeof(header));
    close(fd);

    if (bytes_read == (ssize_t)sizeof(header)) {
        if (header.magic == STATS_MAGIC && header.version == STATS_VERSION) {
            uint64_t expected = compute_checksum(&header.data);
            if (header.checksum == expected) {
                global_stats = header.data;
                sanitize_stats(&global_stats);
                return;
            }
        }
    }

    memset(&global_stats, 0, sizeof(global_stats));
    global_stats.sprint_best_time = 9999.0;
}

static void save_stats(void) {
    sanitize_stats(&global_stats);
    StatsFileHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = STATS_MAGIC;
    header.version = STATS_VERSION;
    header.data = global_stats;
    header.checksum = compute_checksum(&global_stats);

    int fd = open(STATS_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        ssize_t written = write(fd, &header, sizeof(header));
        (void)written;
        close(fd);
    }
}

static unsigned int secure_seed(void) {
    unsigned int seed = 0;
#ifndef _WIN32
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        if (read(fd, &seed, sizeof(seed)) != (ssize_t)sizeof(seed)) {
            seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
        }
        close(fd);
    } else {
        seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    }
#else
    seed = (unsigned int)time(NULL) ^ (unsigned int)GetCurrentProcessId();
#endif
    return seed;
}

static double get_time_sec(void) {
#ifndef _WIN32
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
#else
    static LARGE_INTEGER freq;
    static bool init = false;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = true;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#endif
}

static void init_menu_stars(int term_h, int term_w) {
    for (int i = 0; i < MENU_STARS; i++) {
        stars[i].x = rand() % (term_w > 0 ? term_w : 80);
        stars[i].y = rand() % (term_h > 0 ? term_h : 24);
        stars[i].speed = 1 + (rand() % 2);
        stars[i].color = 8 + (rand() % 7);
        stars[i].ch = ".+*o"[rand() % 4];
    }
}

static void update_menu_stars(int term_h, int term_w) {
    for (int i = 0; i < MENU_STARS; i++) {
        stars[i].y += stars[i].speed;
        if (stars[i].y >= term_h) {
            stars[i].y = 0;
            stars[i].x = rand() % (term_w > 0 ? term_w : 80);
        }
    }
}

static void draw_menu_stars(void) {
    for (int i = 0; i < MENU_STARS; i++) {
        attron(COLOR_PAIR(stars[i].color));
        mvaddch(stars[i].y, stars[i].x, stars[i].ch);
        attroff(COLOR_PAIR(stars[i].color));
    }
}

static int get_bag_piece(PlayerState *p) {
    if (p->bag_index >= 7) {
        for (int i = 0; i < 7; i++) {
            p->bag[i] = i;
        }
        for (int i = 6; i > 0; i--) {
            int j = rand() % (i + 1);
            int temp = p->bag[i];
            p->bag[i] = p->bag[j];
            p->bag[j] = temp;
        }
        p->bag_index = 0;
    }
    return p->bag[p->bag_index++];
}

static bool check_collision(const PlayerState *p, const Piece *piece, int off_x, int off_y, int rot) {
    if (piece->type < 0 || piece->type >= 7 || rot < 0 || rot >= 4) return true;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (BLOCKS[piece->type][rot][r][c]) {
                int nx = piece->x + c + off_x;
                int ny = piece->y + r + off_y;
                if (nx < 0 || nx >= BOARD_W || ny >= BOARD_H) return true;
                if (ny >= 0 && p->board[ny][nx] != 0) return true;
            }
        }
    }
    return false;
}

static void spawn_player_piece(PlayerState *p) {
    p->current.type = p->next_piece;
    p->current.rot = 0;
    p->current.x = BOARD_W / 2 - 2;
    p->current.y = 0;
    p->next_piece = get_bag_piece(p);
    p->can_hold = true;
    p->is_locking = false;
    p->lock_timer = 0.0;
    p->target_calculated = false;

    if (check_collision(p, &p->current, 0, 0, p->current.rot)) {
        p->game_over = true;
    }
}

static void init_player(PlayerState *p, bool is_ai, AiDifficulty diff) {
    memset(p->board, 0, sizeof(p->board));
    p->score = 0;
    p->lines = 0;
    p->level = 1;
    p->combo = 0;
    p->hold_piece = -1;
    p->can_hold = true;
    p->bag_index = 7;
    p->pending_garbage = 0;
    p->last_drop_time = get_time_sec();
    p->lock_timer = 0.0;
    p->is_locking = false;
    p->is_ai = is_ai;
    p->ai_diff = diff;
    p->ai_move_time = get_time_sec();
    p->target_calculated = false;
    p->shake_frames = 0;
    p->flash_count = 0;
    p->flash_timer = 0;
    p->banner_timer = 0;
    p->banner_text[0] = '\0';
    p->mode_start_time = get_time_sec();
    p->survival_timer = get_time_sec();
    p->won = false;
    p->game_over = false;
    p->stats_recorded = false;
    memset(p->particles, 0, sizeof(p->particles));
    p->next_piece = get_bag_piece(p);
    spawn_player_piece(p);
}

static void add_particle(PlayerState *p, float x, float y, int color) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (p->particles[i].life <= 0) {
            p->particles[i].x = x;
            p->particles[i].y = y;
            p->particles[i].vx = ((rand() % 100) - 50) / 30.0f;
            p->particles[i].vy = -((rand() % 60) + 15) / 30.0f;
            p->particles[i].life = 12 + (rand() % 10);
            p->particles[i].max_life = p->particles[i].life;
            p->particles[i].color = color;
            p->particles[i].ch = "*+^."[rand() % 4];
            break;
        }
    }
}

static void spawn_line_particles(PlayerState *p, int row, int color) {
    for (int c = 0; c < BOARD_W; c++) {
        for (int k = 0; k < 3; k++) {
            add_particle(p, (float)(c * 2 + 1), (float)(row + 1), color);
        }
    }
}

static void update_particles(PlayerState *p) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (p->particles[i].life > 0) {
            p->particles[i].x += p->particles[i].vx;
            p->particles[i].y += p->particles[i].vy;
            p->particles[i].vy += 0.08f;
            p->particles[i].life--;
        }
    }
}

static int get_ghost_y_p(const PlayerState *p) {
    int ghost_y = p->current.y;
    while (!check_collision(p, &p->current, 0, ghost_y - p->current.y + 1, p->current.rot)) {
        ghost_y++;
    }
    return ghost_y;
}

static bool try_rotate_p(PlayerState *p, int next_rot) {
    static const int kicks[5][2] = {
        {0, 0},
        {-1, 0},
        {1, 0},
        {0, -1},
        {-2, 0}
    };
    for (int i = 0; i < 5; i++) {
        if (!check_collision(p, &p->current, kicks[i][0], kicks[i][1], next_rot)) {
            p->current.x += kicks[i][0];
            p->current.y += kicks[i][1];
            p->current.rot = next_rot;
            return true;
        }
    }
    return false;
}

static void apply_garbage(PlayerState *p) {
    if (p->pending_garbage <= 0) return;
    int g = p->pending_garbage;
    if (g > 6) g = 6;
    p->pending_garbage -= g;

    int hole = rand() % BOARD_W;
    for (int r = 0; r < BOARD_H - g; r++) {
        for (int c = 0; c < BOARD_W; c++) {
            p->board[r][c] = p->board[r + g][c];
        }
    }
    for (int r = BOARD_H - g; r < BOARD_H; r++) {
        for (int c = 0; c < BOARD_W; c++) {
            p->board[r][c] = (c == hole) ? 0 : 8;
        }
    }
    p->shake_frames = 4;
}

static void show_banner(PlayerState *p, const char *msg, int color) {
    strncpy(p->banner_text, msg, sizeof(p->banner_text) - 1);
    p->banner_text[sizeof(p->banner_text) - 1] = '\0';
    p->banner_timer = 20;
    p->banner_color = color;
}

static int clear_lines_p(PlayerState *p) {
    int cleared = 0;
    p->flash_count = 0;
    for (int r = BOARD_H - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < BOARD_W; c++) {
            if (!p->board[r][c]) {
                full = false;
                break;
            }
        }
        if (full) {
            if (p->flash_count < 4) {
                p->flash_lines[p->flash_count++] = r;
            }
            spawn_line_particles(p, r, p->board[r][0]);
            cleared++;
            for (int k = r; k > 0; k--) {
                for (int c = 0; c < BOARD_W; c++) {
                    p->board[k][c] = p->board[k - 1][c];
                }
            }
            for (int c = 0; c < BOARD_W; c++) {
                p->board[0][c] = 0;
            }
            r++;
        }
    }
    if (cleared > 0) {
        static const int pts[5] = {0, 100, 300, 600, 1000};
        p->combo++;
        int combo_bonus = (p->combo > 1) ? (p->combo * 50) : 0;
        p->score += pts[cleared] * p->level + combo_bonus;
        p->lines += cleared;
        p->level = 1 + p->lines / 10;
        p->shake_frames = (cleared >= 4) ? 6 : (cleared > 1 ? 3 : 1);
        p->flash_timer = 3;

        if (!p->is_ai) {
            global_stats.total_lines_cleared += cleared;
            if (cleared == 4) {
                global_stats.total_tetrus++;
            }
            save_stats();
        }

        if (cleared == 4) {
            show_banner(p, "TETRU QUAD!", 16);
        } else if (cleared == 3) {
            show_banner(p, "TRIPLE!", 14);
        } else if (cleared == 2) {
            show_banner(p, "DOUBLE!", 11);
        } else if (p->combo > 2) {
            char cbuf[32];
            snprintf(cbuf, sizeof(cbuf), "COMBO x%d!", p->combo);
            show_banner(p, cbuf, 12);
        }
    } else {
        p->combo = 0;
    }
    return cleared;
}

static void record_endgame_stats(void) {
    if (p1.stats_recorded) return;
    p1.stats_recorded = true;
    global_stats.total_games++;

    if (game_mode == MODE_CLASSIC) {
        if (p1.score > global_stats.classic_high_score) {
            global_stats.classic_high_score = p1.score;
        }
        if (p1.lines > global_stats.classic_max_lines) {
            global_stats.classic_max_lines = p1.lines;
        }
    } else if (game_mode == MODE_SPRINT) {
        global_stats.sprint_games++;
        if (p1.won) {
            global_stats.sprint_wins++;
            double time_taken = get_time_sec() - p1.mode_start_time;
            if (time_taken >= 5.0 && time_taken < global_stats.sprint_best_time) {
                global_stats.sprint_best_time = time_taken;
            }
        }
    } else if (game_mode == MODE_SURVIVAL) {
        if (p1.score > global_stats.survival_high_score) {
            global_stats.survival_high_score = p1.score;
        }
    } else if (game_mode == MODE_VS_BOT) {
        global_stats.vs_games++;
        if (p2.game_over && !p1.game_over) {
            global_stats.vs_wins++;
        } else {
            global_stats.vs_losses++;
        }
    }
    save_stats();
}

static void lock_piece_p(PlayerState *p, PlayerState *opponent) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (BLOCKS[p->current.type][p->current.rot][r][c]) {
                int nx = p->current.x + c;
                int ny = p->current.y + r;
                if (ny >= 0 && ny < BOARD_H && nx >= 0 && nx < BOARD_W) {
                    p->board[ny][nx] = p->current.type + 1;
                }
            }
        }
    }

    int lines = clear_lines_p(p);
    if (opponent && lines > 1) {
        int send = (lines == 2) ? 1 : ((lines == 3) ? 2 : 4);
        if (p->pending_garbage > 0) {
            int cancel = (send < p->pending_garbage) ? send : p->pending_garbage;
            p->pending_garbage -= cancel;
            send -= cancel;
        }
        if (send > 0) {
            opponent->pending_garbage += send;
        }
    }

    if (lines == 0) {
        apply_garbage(p);
    }

    if (game_mode == MODE_SPRINT && p->lines >= 40) {
        p->won = true;
        p->game_over = true;
        record_endgame_stats();
    }

    spawn_player_piece(p);
    if (p->game_over) {
        record_endgame_stats();
    }
}

static void hard_drop_p(PlayerState *p, PlayerState *opponent) {
    int drop_dist = 0;
    while (!check_collision(p, &p->current, 0, 1, p->current.rot)) {
        p->current.y++;
        drop_dist++;
    }
    p->score += drop_dist * 2;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (BLOCKS[p->current.type][p->current.rot][r][c]) {
                add_particle(p, (float)((p->current.x + c) * 2 + 1), (float)(p->current.y + r + 1), p->current.type + 1);
            }
        }
    }
    lock_piece_p(p, opponent);
    p->last_drop_time = get_time_sec();
}

static void hold_piece_p(PlayerState *p) {
    if (!p->can_hold) return;
    if (p->hold_piece == -1) {
        p->hold_piece = p->current.type;
        spawn_player_piece(p);
    } else {
        int temp = p->hold_piece;
        p->hold_piece = p->current.type;
        p->current.type = temp;
        p->current.rot = 0;
        p->current.x = BOARD_W / 2 - 2;
        p->current.y = 0;
        p->is_locking = false;
        p->lock_timer = 0.0;
        p->target_calculated = false;
    }
    p->can_hold = false;
}

static int evaluate_board(const int b[BOARD_H][BOARD_W]) {
    int aggregate_height = 0;
    int complete_lines = 0;
    int holes = 0;
    int bumpiness = 0;
    int col_heights[BOARD_W];

    for (int c = 0; c < BOARD_W; c++) {
        col_heights[c] = 0;
        for (int r = 0; r < BOARD_H; r++) {
            if (b[r][c] != 0) {
                col_heights[c] = BOARD_H - r;
                break;
            }
        }
        aggregate_height += col_heights[c];
    }

    for (int r = 0; r < BOARD_H; r++) {
        bool full = true;
        for (int c = 0; c < BOARD_W; c++) {
            if (b[r][c] == 0) {
                full = false;
                break;
            }
        }
        if (full) complete_lines++;
    }

    for (int c = 0; c < BOARD_W; c++) {
        bool block_found = false;
        for (int r = 0; r < BOARD_H; r++) {
            if (b[r][c] != 0) {
                block_found = true;
            } else if (block_found) {
                holes++;
            }
        }
    }

    for (int c = 0; c < BOARD_W - 1; c++) {
        bumpiness += abs(col_heights[c] - col_heights[c + 1]);
    }

    return -51 * aggregate_height + 76 * complete_lines * 10 - 36 * holes * 10 - 18 * bumpiness;
}

static void find_best_ai_move(PlayerState *p) {
    int best_score = -999999;
    int best_x = p->current.x;
    int best_rot = 0;

    int mistake_chance = 0;
    switch (p->ai_diff) {
        case DIFF_BEGINNER: mistake_chance = 50; break;
        case DIFF_EASY: mistake_chance = 30; break;
        case DIFF_MEDIUM: mistake_chance = 12; break;
        case DIFF_HARD: mistake_chance = 3; break;
        case DIFF_IMPOSSIBLE: mistake_chance = 0; break;
    }

    if (mistake_chance > 0 && (rand() % 100) < mistake_chance) {
        p->target_rot = rand() % 4;
        p->target_x = rand() % (BOARD_W - 2);
        p->target_calculated = true;
        return;
    }

    for (int rot = 0; rot < 4; rot++) {
        for (int x = -3; x < BOARD_W + 3; x++) {
            Piece test_p = {p->current.type, rot, x, 0};
            if (check_collision(p, &test_p, 0, 0, rot)) continue;

            int y = 0;
            while (!check_collision(p, &test_p, 0, y - test_p.y + 1, rot)) {
                y++;
            }
            test_p.y = y;

            int sim_board[BOARD_H][BOARD_W];
            memcpy(sim_board, p->board, sizeof(p->board));

            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    if (BLOCKS[test_p.type][rot][r][c]) {
                        int nx = test_p.x + c;
                        int ny = test_p.y + r;
                        if (ny >= 0 && ny < BOARD_H && nx >= 0 && nx < BOARD_W) {
                            sim_board[ny][nx] = test_p.type + 1;
                        }
                    }
                }
            }

            int score_val = evaluate_board(sim_board);
            if (score_val > best_score) {
                best_score = score_val;
                best_x = x;
                best_rot = rot;
            }
        }
    }

    p->target_x = best_x;
    p->target_rot = best_rot;
    p->target_calculated = true;
}

static void update_ai_player(PlayerState *ai, PlayerState *human, double current_time) {
    if (ai->game_over) return;
    if (!ai->target_calculated) {
        find_best_ai_move(ai);
    }

    double speed = 0.08;
    bool direct_hard_drop = true;

    switch (ai->ai_diff) {
        case DIFF_BEGINNER:
            speed = 0.30;
            direct_hard_drop = false;
            break;
        case DIFF_EASY:
            speed = 0.20;
            direct_hard_drop = false;
            break;
        case DIFF_MEDIUM:
            speed = 0.11;
            direct_hard_drop = true;
            break;
        case DIFF_HARD:
            speed = 0.05;
            direct_hard_drop = true;
            break;
        case DIFF_IMPOSSIBLE:
            speed = 0.015;
            direct_hard_drop = true;
            break;
    }

    if (current_time - ai->ai_move_time >= speed) {
        ai->ai_move_time = current_time;

        if (ai->current.rot != ai->target_rot) {
            int next_rot = (ai->current.rot + 1) % 4;
            try_rotate_p(ai, next_rot);
        } else if (ai->current.x < ai->target_x) {
            if (!check_collision(ai, &ai->current, 1, 0, ai->current.rot)) {
                ai->current.x++;
            }
        } else if (ai->current.x > ai->target_x) {
            if (!check_collision(ai, &ai->current, -1, 0, ai->current.rot)) {
                ai->current.x--;
            }
        } else {
            if (direct_hard_drop) {
                hard_drop_p(ai, human);
            } else {
                if (!check_collision(ai, &ai->current, 0, 1, ai->current.rot)) {
                    ai->current.y++;
                } else {
                    lock_piece_p(ai, human);
                }
            }
        }
    }
}

static void draw_clean_frame(int start_y, int start_x, int h, int w, const char *title, int border_color) {
    attron(COLOR_PAIR(border_color));
    mvaddstr(start_y, start_x, "+");
    mvaddstr(start_y, start_x + w - 1, "+");
    mvaddstr(start_y + h - 1, start_x, "+");
    mvaddstr(start_y + h - 1, start_x + w - 1, "+");
    for (int x = 1; x < w - 1; x++) {
        mvaddstr(start_y, start_x + x, "-");
        mvaddstr(start_y + h - 1, start_x + x, "-");
    }
    for (int y = 1; y < h - 1; y++) {
        mvaddstr(start_y + y, start_x, "|");
        mvaddstr(start_y + y, start_x + w - 1, "|");
    }
    attroff(COLOR_PAIR(border_color));

    if (title && (int)strlen(title) < w - 2) {
        int title_len = (int)strlen(title);
        int pos_x = start_x + (w - title_len) / 2;
        attron(A_BOLD | COLOR_PAIR(16));
        mvaddstr(start_y, pos_x, title);
        attroff(A_BOLD | COLOR_PAIR(16));
    }
}

static const char *get_diff_name(AiDifficulty diff) {
    switch (diff) {
        case DIFF_BEGINNER: return "BEGINNER";
        case DIFF_EASY: return "EASY";
        case DIFF_MEDIUM: return "MEDIUM";
        case DIFF_HARD: return "HARD";
        case DIFF_IMPOSSIBLE: return "IMPOSSIBLE";
    }
    return "UNKNOWN";
}

static void render_player(const PlayerState *p, int start_y, int start_x, const char *name) {
    int off_y = (p->shake_frames > 0) ? (p->shake_frames % 2 == 0 ? 1 : -1) : 0;
    int cur_y = start_y + off_y;
    int cur_x = start_x;

    int win_h = BOARD_H + 2;
    int win_w = BOARD_W * 2 + 2;
    int hold_w = 12;
    int side_w = 14;

    draw_clean_frame(cur_y + 4, cur_x, 8, hold_w, "HOLD", 15);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (p->hold_piece >= 0 && p->hold_piece < 7 && BLOCKS[p->hold_piece][0][r][c]) {
                attron(COLOR_PAIR(p->hold_piece + 1));
                mvaddstr(cur_y + 6 + r, cur_x + 2 + c * 2, "[]");
                attroff(COLOR_PAIR(p->hold_piece + 1));
            }
        }
    }

    draw_clean_frame(cur_y, cur_x + hold_w + 1, win_h, win_w, name, 15);
    int board_origin_y = cur_y + 1;
    int board_origin_x = cur_x + hold_w + 2;

    for (int r = 0; r < BOARD_H; r++) {
        for (int c = 0; c < BOARD_W; c++) {
            if (p->board[r][c] > 0) {
                int col = p->board[r][c];
                if (p->flash_timer > 0) {
                    for (int f = 0; f < p->flash_count; f++) {
                        if (p->flash_lines[f] == r) {
                            col = 3;
                            break;
                        }
                    }
                }
                attron(COLOR_PAIR(col));
                mvaddstr(board_origin_y + r, board_origin_x + c * 2, "[]");
                attroff(COLOR_PAIR(col));
            }
        }
    }

    if (!p->game_over) {
        int ghost_y = get_ghost_y_p(p);
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (BLOCKS[p->current.type][p->current.rot][r][c]) {
                    int gx = p->current.x + c;
                    int gy = ghost_y + r;
                    if (gy >= 0 && gy < BOARD_H && gx >= 0 && gx < BOARD_W && !p->board[gy][gx]) {
                        attron(COLOR_PAIR(p->current.type + 8));
                        mvaddstr(board_origin_y + gy, board_origin_x + gx * 2, "::");
                        attroff(COLOR_PAIR(p->current.type + 8));
                    }
                }
            }
        }

        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (BLOCKS[p->current.type][p->current.rot][r][c]) {
                    int px = p->current.x + c;
                    int py = p->current.y + r;
                    if (py >= 0 && py < BOARD_H && px >= 0 && px < BOARD_W) {
                        attron(COLOR_PAIR(p->current.type + 1));
                        mvaddstr(board_origin_y + py, board_origin_x + px * 2, "[]");
                        attroff(COLOR_PAIR(p->current.type + 1));
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (p->particles[i].life > 0) {
            int px = (int)p->particles[i].x;
            int py = (int)p->particles[i].y;
            if (py >= 0 && py < BOARD_H && px >= 0 && px < BOARD_W * 2) {
                attron(COLOR_PAIR(p->particles[i].color) | A_BOLD);
                mvaddch(board_origin_y + py, board_origin_x + px, p->particles[i].ch);
                attroff(COLOR_PAIR(p->particles[i].color) | A_BOLD);
            }
        }
    }

    if (p->banner_timer > 0) {
        attron(COLOR_PAIR(p->banner_color) | A_BOLD);
        int len = (int)strlen(p->banner_text);
        int bx = board_origin_x + (BOARD_W * 2 - len) / 2;
        mvaddstr(board_origin_y + 8, bx, p->banner_text);
        attroff(COLOR_PAIR(p->banner_color) | A_BOLD);
    }

    int side_origin_x = cur_x + hold_w + 1 + win_w + 1;
    draw_clean_frame(cur_y, side_origin_x, win_h, side_w, "STATS", 15);

    attron(A_BOLD);
    mvaddstr(cur_y + 1, side_origin_x + 5, "NEXT");
    attroff(A_BOLD);
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (p->next_piece >= 0 && p->next_piece < 7 && BLOCKS[p->next_piece][0][r][c]) {
                attron(COLOR_PAIR(p->next_piece + 1));
                mvaddstr(cur_y + 3 + r, side_origin_x + 3 + c * 2, "[]");
                attroff(COLOR_PAIR(p->next_piece + 1));
            }
        }
    }

    if (p->is_ai) {
        attron(A_BOLD);
        mvaddstr(cur_y + 8, side_origin_x + 2, "BOT DIFF");
        attroff(A_BOLD);
        attron(COLOR_PAIR(16));
        mvprintw(cur_y + 9, side_origin_x + 2, "%s", get_diff_name(p->ai_diff));
        attroff(COLOR_PAIR(16));

        attron(A_BOLD);
        mvaddstr(cur_y + 11, side_origin_x + 2, "LINES");
        attroff(A_BOLD);
        mvprintw(cur_y + 12, side_origin_x + 2, "%d", p->lines);
    } else {
        attron(A_BOLD);
        mvaddstr(cur_y + 8, side_origin_x + 2, "SCORE");
        attroff(A_BOLD);
        mvprintw(cur_y + 9, side_origin_x + 2, "%d", p->score);

        attron(A_BOLD);
        mvaddstr(cur_y + 11, side_origin_x + 2, "LINES");
        attroff(A_BOLD);
        mvprintw(cur_y + 12, side_origin_x + 2, "%d", p->lines);

        attron(A_BOLD);
        mvaddstr(cur_y + 14, side_origin_x + 2, "LEVEL");
        attroff(A_BOLD);
        mvprintw(cur_y + 15, side_origin_x + 2, "%d", p->level);
    }

    if (game_mode == MODE_SPRINT && !p->is_ai) {
        double elapsed = (p->won || p->game_over) ? (get_time_sec() - p->mode_start_time) : (get_time_sec() - p->mode_start_time);
        attron(A_BOLD | COLOR_PAIR(16));
        mvprintw(cur_y + 17, side_origin_x + 2, "TIME: %.1fs", elapsed);
        attroff(A_BOLD | COLOR_PAIR(16));
    } else if (p->pending_garbage > 0) {
        attron(COLOR_PAIR(7) | A_BOLD);
        mvprintw(cur_y + 17, side_origin_x + 2, "INCOMING: %d", p->pending_garbage);
        attroff(COLOR_PAIR(7) | A_BOLD);
    }

    if (p->game_over) {
        if (p->won) {
            attron(COLOR_PAIR(12) | A_BOLD);
            mvaddstr(board_origin_y + BOARD_H / 2 - 1, board_origin_x + (BOARD_W * 2 - 10) / 2, " VICTORY! ");
            attroff(COLOR_PAIR(12) | A_BOLD);
        } else {
            attron(COLOR_PAIR(17) | A_BOLD);
            mvaddstr(board_origin_y + BOARD_H / 2 - 1, board_origin_x + (BOARD_W * 2 - 11) / 2, " GAME OVER ");
            attroff(COLOR_PAIR(17) | A_BOLD);
        }
        mvaddstr(board_origin_y + BOARD_H / 2 + 1, board_origin_x + (BOARD_W * 2 - 11) / 2, " R: Restart");
    }
}

static void render_stats_view(int term_h, int term_w) {
    erase();
    draw_menu_stars();

    int win_h = 22;
    int win_w = 58;
    int sy = (term_h - win_h) / 2;
    int sx = (term_w - win_w) / 2;
    if (sy < 0) sy = 0;
    if (sx < 0) sx = 0;

    int border_color = 8 + (global_tick / 8) % 7;
    draw_clean_frame(sy, sx, win_h, win_w, "PLAYER CAREER STATS", border_color);

    attron(COLOR_PAIR(16) | A_BOLD);
    mvaddstr(sy + 2, sx + (win_w - 24) / 2, "=== CAREER RECORDS ===");
    attroff(COLOR_PAIR(16) | A_BOLD);

    attron(COLOR_PAIR(15));
    mvprintw(sy + 4, sx + 5, "Total Games Played: %d", global_stats.total_games);
    mvprintw(sy + 5, sx + 5, "Total Lines Cleared: %d", global_stats.total_lines_cleared);
    mvprintw(sy + 6, sx + 5, "Total Tetru Quads (4-Lines): %d", global_stats.total_tetrus);

    mvaddstr(sy + 8, sx + 5, "--- Classic Mode ---");
    mvprintw(sy + 9, sx + 7, "High Score: %d", global_stats.classic_high_score);
    mvprintw(sy + 10, sx + 7, "Max Lines: %d", global_stats.classic_max_lines);

    mvaddstr(sy + 11, sx + 5, "--- 40-Line Sprint ---");
    if (global_stats.sprint_best_time < 9990.0) {
        mvprintw(sy + 12, sx + 7, "Best Time: %.2fs (Wins: %d/%d)", global_stats.sprint_best_time, global_stats.sprint_wins, global_stats.sprint_games);
    } else {
        mvprintw(sy + 12, sx + 7, "Best Time: None (Wins: %d/%d)", global_stats.sprint_wins, global_stats.sprint_games);
    }

    mvaddstr(sy + 13, sx + 5, "--- Survival Rush ---");
    mvprintw(sy + 14, sx + 7, "Survival High Score: %d", global_stats.survival_high_score);

    mvaddstr(sy + 15, sx + 5, "--- VS AI Battle ---");
    mvprintw(sy + 16, sx + 7, "Battles: %d | Wins: %d | Losses: %d", global_stats.vs_games, global_stats.vs_wins, global_stats.vs_losses);

    mvaddstr(sy + 18, sx + 5, "Press any key or Q to return to Main Menu");
    attroff(COLOR_PAIR(15));

    attron(COLOR_PAIR(16) | A_BOLD);
    mvaddstr(sy + 20, sx + (win_w - 18) / 2, "Made by Eskrid");
    attroff(COLOR_PAIR(16) | A_BOLD);

    refresh();
}

static void render_menu(int term_h, int term_w) {
    erase();
    draw_menu_stars();

    int menu_h = 25;
    int menu_w = 58;
    int sy = (term_h - menu_h) / 2;
    int sx = (term_w - menu_w) / 2;
    if (sy < 0) sy = 0;
    if (sx < 0) sx = 0;

    int border_color = 8 + (global_tick / 8) % 7;
    draw_clean_frame(sy, sx, menu_h, menu_w, "TETRU ARCADE", border_color);

    static const char *logo[4] = {
        " _____ _____ _____ ____  _   _ ",
        "|_   _| ____|_   _|  _ \\| | | |",
        "  | | |  _|   | | | |_) | | | |",
        "  |_| |_____| |_| |_| \\_\\_____/"
    };

    for (int i = 0; i < 4; i++) {
        int color = 8 + ((global_tick / 4 + i) % 7);
        attron(COLOR_PAIR(color) | A_BOLD);
        mvaddstr(sy + 2 + i, sx + (menu_w - 31) / 2, logo[i]);
        attroff(COLOR_PAIR(color) | A_BOLD);
    }

    attron(COLOR_PAIR(14));
    mvprintw(sy + 6, sx + (menu_w - (int)strlen(TETRU_VERSION)) / 2, "%s", TETRU_VERSION);
    attroff(COLOR_PAIR(14));

    const char *options[6] = {
        "1. Classic Mode (Endless)",
        "2. 40-Line Sprint (Speedrun)",
        "3. Survival Rush (Garbage Attack)",
        "4. VS Computer (AI Battle)",
        "5. Career Stats & Records",
        "6. Exit Game"
    };

    for (int i = 0; i < 6; i++) {
        if (menu_selection == i) {
            attron(COLOR_PAIR(i + 1) | A_BOLD | A_REVERSE);
            mvprintw(sy + 8 + i * 2, sx + 8, "  >> %-36s <<  ", options[i]);
            attroff(COLOR_PAIR(i + 1) | A_BOLD | A_REVERSE);
        } else {
            attron(COLOR_PAIR(10));
            mvprintw(sy + 8 + i * 2, sx + 10, "   [ %-34s ]   ", options[i]);
            attroff(COLOR_PAIR(10));
        }
    }

    attron(COLOR_PAIR(15));
    mvaddstr(sy + 21, sx + 6, "Controls: UP/DOWN/W/S | ENTER/SPACE: Select");
    attroff(COLOR_PAIR(15));

    attron(COLOR_PAIR(16) | A_BOLD);
    mvaddstr(sy + 23, sx + (menu_w - 18) / 2, "Made by Eskrid");
    attroff(COLOR_PAIR(16) | A_BOLD);

    refresh();
}

static void render_difficulty_menu(int term_h, int term_w) {
    erase();
    draw_menu_stars();

    int menu_h = 20;
    int menu_w = 56;
    int sy = (term_h - menu_h) / 2;
    int sx = (term_w - menu_w) / 2;
    if (sy < 0) sy = 0;
    if (sx < 0) sx = 0;

    int border_color = 8 + (global_tick / 8) % 7;
    draw_clean_frame(sy, sx, menu_h, menu_w, "SELECT AI DIFFICULTY", border_color);

    attron(COLOR_PAIR(16) | A_BOLD);
    mvaddstr(sy + 2, sx + (menu_w - 23) / 2, "=== SELECT AI LEVEL ===");
    attroff(COLOR_PAIR(16) | A_BOLD);

    const char *diff_options[5] = {
        "1. Beginner    (Relaxed)",
        "2. Easy        (Casual)",
        "3. Medium      (Standard)",
        "4. Hard        (Fast Attack)",
        "5. Impossible  (Supreme God)"
    };

    for (int i = 0; i < 5; i++) {
        if (diff_selection == i) {
            attron(COLOR_PAIR(i + 1) | A_BOLD | A_REVERSE);
            mvprintw(sy + 5 + i * 2, sx + 8, "  >> %-32s <<  ", diff_options[i]);
            attroff(COLOR_PAIR(i + 1) | A_BOLD | A_REVERSE);
        } else {
            attron(COLOR_PAIR(10));
            mvprintw(sy + 5 + i * 2, sx + 10, "   [ %-30s ]   ", diff_options[i]);
            attroff(COLOR_PAIR(10));
        }
    }

    attron(COLOR_PAIR(15));
    mvaddstr(sy + 16, sx + 6, "Controls: UP/DOWN to select | ENTER: Battle");
    mvaddstr(sy + 17, sx + 6, "Press Q to return to Main Menu");
    attroff(COLOR_PAIR(15));

    attron(COLOR_PAIR(16) | A_BOLD);
    mvaddstr(sy + 18, sx + (menu_w - 18) / 2, "Made by Eskrid");
    attroff(COLOR_PAIR(16) | A_BOLD);

    refresh();
}

static void start_game(GameMode mode, AiDifficulty diff) {
    game_mode = mode;
    current_diff = diff;
    init_player(&p1, false, DIFF_MEDIUM);
    if (mode == MODE_VS_BOT) {
        init_player(&p2, true, diff);
    }
    game_state = STATE_PLAYING;
}

int main(void) {
#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif

    load_stats();

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_BLACK, COLOR_CYAN);
        init_pair(2, COLOR_BLACK, COLOR_BLUE);
        init_pair(3, COLOR_BLACK, COLOR_WHITE);
        init_pair(4, COLOR_BLACK, COLOR_YELLOW);
        init_pair(5, COLOR_BLACK, COLOR_GREEN);
        init_pair(6, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(7, COLOR_BLACK, COLOR_RED);
        init_pair(8, COLOR_CYAN, -1);
        init_pair(9, COLOR_BLUE, -1);
        init_pair(10, COLOR_WHITE, -1);
        init_pair(11, COLOR_YELLOW, -1);
        init_pair(12, COLOR_GREEN, -1);
        init_pair(13, COLOR_MAGENTA, -1);
        init_pair(14, COLOR_RED, -1);
        init_pair(15, COLOR_WHITE, -1);
        init_pair(16, COLOR_YELLOW, -1);
        init_pair(17, COLOR_RED, -1);
    }

    srand(secure_seed());

    int term_h, term_w;
    getmaxyx(stdscr, term_h, term_w);
    init_menu_stars(term_h, term_w);

    while (running) {
        getmaxyx(stdscr, term_h, term_w);
        global_tick++;

        int ch = getch();

        if (game_state == STATE_MENU) {
            update_menu_stars(term_h, term_w);

            if (ch == KEY_UP || ch == 'w' || ch == 'W') {
                menu_selection = (menu_selection + 5) % 6;
            } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
                menu_selection = (menu_selection + 1) % 6;
            } else if (ch == 10 || ch == 13 || ch == ' ') {
                if (menu_selection == 0) {
                    start_game(MODE_CLASSIC, DIFF_MEDIUM);
                } else if (menu_selection == 1) {
                    start_game(MODE_SPRINT, DIFF_MEDIUM);
                } else if (menu_selection == 2) {
                    start_game(MODE_SURVIVAL, DIFF_MEDIUM);
                } else if (menu_selection == 3) {
                    game_state = STATE_DIFF_MENU;
                } else if (menu_selection == 4) {
                    game_state = STATE_STATS_VIEW;
                } else {
                    break;
                }
            } else if (ch == 'q' || ch == 'Q') {
                break;
            }
            render_menu(term_h, term_w);
            napms(33);
            continue;
        }

        if (game_state == STATE_STATS_VIEW) {
            update_menu_stars(term_h, term_w);
            if (ch > 0) {
                game_state = STATE_MENU;
            }
            render_stats_view(term_h, term_w);
            napms(33);
            continue;
        }

        if (game_state == STATE_DIFF_MENU) {
            update_menu_stars(term_h, term_w);

            if (ch == KEY_UP || ch == 'w' || ch == 'W') {
                diff_selection = (diff_selection + 4) % 5;
            } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
                diff_selection = (diff_selection + 1) % 5;
            } else if (ch == 10 || ch == 13 || ch == ' ') {
                start_game(MODE_VS_BOT, (AiDifficulty)diff_selection);
            } else if (ch == 'q' || ch == 'Q') {
                game_state = STATE_MENU;
            }
            render_difficulty_menu(term_h, term_w);
            napms(33);
            continue;
        }

        int req_w = (game_mode == MODE_VS_BOT) ? 96 : 48;
        if (term_h < 23 || term_w < req_w) {
            erase();
            mvprintw(term_h / 2, (term_w - 24) > 0 ? (term_w - 24) / 2 : 0, "Terminal too small: %dx%d", term_w, term_h);
            mvprintw(term_h / 2 + 1, (term_w - 24) > 0 ? (term_w - 24) / 2 : 0, "Needs at least %dx23", req_w);
            refresh();
            napms(50);
            continue;
        }

        if (ch == 'q' || ch == 'Q') {
            record_endgame_stats();
            game_state = STATE_MENU;
            erase();
            refresh();
            continue;
        }

        if (ch == 'p' || ch == 'P') {
            if (game_state == STATE_PLAYING) game_state = STATE_PAUSED;
            else if (game_state == STATE_PAUSED) game_state = STATE_PLAYING;
        }

        if (ch == 'r' || ch == 'R') {
            record_endgame_stats();
            start_game(game_mode, current_diff);
            continue;
        }

        double cur_time = get_time_sec();

        if (game_state == STATE_PLAYING) {
            if (game_mode == MODE_SURVIVAL && !p1.game_over) {
                double surv_interval = 8.0 - (p1.level * 0.4);
                if (surv_interval < 2.5) surv_interval = 2.5;
                if (cur_time - p1.survival_timer >= surv_interval) {
                    p1.pending_garbage += 1;
                    apply_garbage(&p1);
                    p1.survival_timer = cur_time;
                }
            }

            if (!p1.game_over) {
                if (ch == KEY_LEFT || ch == 'a' || ch == 'A') {
                    if (!check_collision(&p1, &p1.current, -1, 0, p1.current.rot)) {
                        p1.current.x--;
                        if (p1.is_locking) p1.lock_timer = cur_time;
                    }
                } else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') {
                    if (!check_collision(&p1, &p1.current, 1, 0, p1.current.rot)) {
                        p1.current.x++;
                        if (p1.is_locking) p1.lock_timer = cur_time;
                    }
                } else if (ch == KEY_UP || ch == 'w' || ch == 'W') {
                    int next_rot = (p1.current.rot + 1) % 4;
                    if (try_rotate_p(&p1, next_rot)) {
                        if (p1.is_locking) p1.lock_timer = cur_time;
                    }
                } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
                    if (!check_collision(&p1, &p1.current, 0, 1, p1.current.rot)) {
                        p1.current.y++;
                        p1.score += 1;
                    }
                } else if (ch == ' ') {
                    hard_drop_p(&p1, (game_mode == MODE_VS_BOT) ? &p2 : NULL);
                } else if (ch == 'c' || ch == 'C') {
                    hold_piece_p(&p1);
                }

                double drop_interval = 0.8 - (p1.level - 1) * 0.07;
                if (drop_interval < 0.08) drop_interval = 0.08;

                if (cur_time - p1.last_drop_time >= drop_interval) {
                    if (!check_collision(&p1, &p1.current, 0, 1, p1.current.rot)) {
                        p1.current.y++;
                        p1.is_locking = false;
                    } else {
                        if (!p1.is_locking) {
                            p1.is_locking = true;
                            p1.lock_timer = cur_time;
                        }
                    }
                    p1.last_drop_time = cur_time;
                }

                if (p1.is_locking) {
                    if (check_collision(&p1, &p1.current, 0, 1, p1.current.rot)) {
                        if (cur_time - p1.lock_timer >= 0.45) {
                            lock_piece_p(&p1, (game_mode == MODE_VS_BOT) ? &p2 : NULL);
                        }
                    } else {
                        p1.is_locking = false;
                    }
                }
            }

            if (game_mode == MODE_VS_BOT && !p2.game_over) {
                update_ai_player(&p2, &p1, cur_time);
            }
        }

        update_particles(&p1);
        if (p1.shake_frames > 0) p1.shake_frames--;
        if (p1.flash_timer > 0) p1.flash_timer--;
        if (p1.banner_timer > 0) p1.banner_timer--;

        if (game_mode == MODE_VS_BOT) {
            update_particles(&p2);
            if (p2.shake_frames > 0) p2.shake_frames--;
            if (p2.flash_timer > 0) p2.flash_timer--;
            if (p2.banner_timer > 0) p2.banner_timer--;
        }

        erase();
        if (game_mode != MODE_VS_BOT) {
            int total_w = 12 + 22 + 14 + 2;
            int sx = (term_w - total_w) / 2;
            int sy = (term_h - 22) / 2;
            const char *mode_name = "TETRU CLASSIC";
            if (game_mode == MODE_SPRINT) mode_name = "40-LINE SPRINT";
            else if (game_mode == MODE_SURVIVAL) mode_name = "SURVIVAL RUSH";
            render_player(&p1, sy, sx, mode_name);
        } else {
            int p_w = 12 + 22 + 14 + 2;
            int gap = 2;
            int total_w = p_w * 2 + gap;
            int sx = (term_w - total_w) / 2;
            int sy = (term_h - 22) / 2;
            render_player(&p1, sy, sx, "YOU");
            render_player(&p2, sy, sx + p_w + gap, "AI BOT");
        }

        if (game_state == STATE_PAUSED) {
            attron(COLOR_PAIR(16) | A_BOLD);
            mvaddstr(term_h / 2, (term_w - 10) / 2, " [PAUSED] ");
            attroff(COLOR_PAIR(16) | A_BOLD);
        }

        refresh();
        napms(16);
    }

    save_stats();
    endwin();
    return 0;
}
