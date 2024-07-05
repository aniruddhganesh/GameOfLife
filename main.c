#include <stdbool.h>
#include <unistd.h>

#include <pthread.h>
#include <SDL2/SDL.h>

#define WIDTH  800
#define HEIGHT WIDTH

#define SIM_PIXEL_RES 16
#define SIM_PIXEL_PAD 2

#define UTICK_RATE 300 * 1000

SDL_Renderer *renderer;
SDL_Window *window;

static bool sim_continue = false;

struct {
    bool pixels[SIM_PIXEL_RES][SIM_PIXEL_RES];
} State;

void init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error Initializing SDL: %s\n", SDL_GetError());
        exit(1);
    }

    window = SDL_CreateWindow("Game of Life",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            WIDTH, HEIGHT,
            SDL_WINDOW_SHOWN);

    if (!window) {
        fprintf(stderr, "Error Creating Window: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Error Creating Renderer: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }
}

void draw_grid(void)
{
    static const int px_size = (WIDTH - (SIM_PIXEL_RES+1)*SIM_PIXEL_PAD)/ SIM_PIXEL_RES;
    static const int pad = (WIDTH - ((px_size+SIM_PIXEL_PAD)*SIM_PIXEL_RES))/2;

    SDL_Rect r;
    r.w = r.h = px_size;

    for (int i = 0; i < SIM_PIXEL_RES; i++) {
        for (int j = 0; j < SIM_PIXEL_RES; j++) {
            r.x = (px_size + SIM_PIXEL_PAD)*i + pad;
            r.y = (px_size + SIM_PIXEL_PAD)*j + pad;

            if (State.pixels[i][j]) {
                SDL_SetRenderDrawColor(renderer, 0x22, 0xF2, 0x22, 0xFF);
            } else {
                SDL_SetRenderDrawColor(renderer, 0x22, 0x22, 0x22, 0xFF);
            }

            SDL_RenderDrawRect(renderer, &r);
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

void set_clicked_cell(void)
{
    static const int px_size = (WIDTH - (SIM_PIXEL_RES+1)*SIM_PIXEL_PAD)/ SIM_PIXEL_RES;
    static const int pad = (WIDTH - ((px_size+SIM_PIXEL_PAD)*SIM_PIXEL_RES))/2;

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    int cell_x = (mx - pad)/(px_size + SIM_PIXEL_PAD);
    int cell_y = (my - pad)/(px_size + SIM_PIXEL_PAD);

    State.pixels[cell_x][cell_y] = true;
}


int num_live_neighbours(int cell_x, int cell_y)
{
    int live_neighbour = 0;

    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            if (i == 0 && j == 0) 
                continue; // That's Us!

            int x = cell_x + i;
            int y = cell_y + j;

            if (x >= 0 && x < SIM_PIXEL_RES && y >= 0 && y < SIM_PIXEL_RES) {
                if (State.pixels[x][y]) {
                    ++live_neighbour;
                }
            }
        }
    }

    return live_neighbour;
}

// Underpopulation:
// Any live cell with fewer than two live neighbours dies, 
// as if by underpopulation.
bool cell_death_by_underpopulation(int cell_x, int cell_y)
{
    return num_live_neighbours(cell_x, cell_y) < 2;
}

// Overpopulation:
// Any live cell with more than three live neighbours dies, 
// as if by overpopulation.
int cell_death_by_overpopulation(int cell_x, int cell_y)
{
    return num_live_neighbours(cell_x, cell_y) > 3;
}

// Reproduction:
// Any dead cell with exactly three live neighbours becomes 
// a live cell, as if by reproduction.
int cell_creation(int cell_x, int cell_y)
{
    bool k = (num_live_neighbours(cell_x, cell_y) == 3);
    return k;
}

// Lives on:  # IMPLIED RULE #
// Any live cell with two or three live neighbours lives on 
// to the next generation.
void sim_next_state(void) {
    bool next_state[SIM_PIXEL_RES][SIM_PIXEL_RES] = {0};

    for (size_t i = 0; i < SIM_PIXEL_RES; i++) {
        for (size_t j = 0; j < SIM_PIXEL_RES; j++) {
            if (State.pixels[i][j]) {
                next_state[i][j] = !(cell_death_by_underpopulation(i, j) 
                        || cell_death_by_overpopulation(i, j));
            } else {
                next_state[i][j] = cell_creation(i, j);
            }
        }
    }

    for (size_t i = 0; i < SIM_PIXEL_RES; i++) {
        for (size_t j = 0; j < SIM_PIXEL_RES; j++) {
            State.pixels[i][j] = next_state[i][j];
        }
    }
}

void* run_sim(void *sim_continue)
{
    bool *s = sim_continue;
    while (1) {
        if (*s)
            sim_next_state();
        usleep(UTICK_RATE);
    }
    return NULL;
}

int main(void)
{
    init();

    SDL_Event ev;
    bool quit = false;
    
    pthread_t simulation;
    pthread_create(&simulation, NULL, &run_sim, &sim_continue);

    while (!quit) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_KEYDOWN:
                    sim_continue = true;
                    const char *keys = SDL_GetKeyboardState(NULL);

                    if (keys[SDL_SCANCODE_SPACE]) sim_continue = !sim_continue;
                    if (keys[SDL_SCANCODE_S] && !sim_continue)
                        sim_next_state();

                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!sim_continue)
                        set_clicked_cell();
                    break;
                case SDL_QUIT: 
                    quit = true; 
                    break;
            }
        }

        draw_grid();

        SDL_RenderPresent(renderer);

        SDL_SetRenderDrawColor(renderer, 0x0, 0x0, 0x0, 0xFF);
        SDL_RenderClear(renderer);

        SDL_Delay(16);
    }

    pthread_cancel(simulation);
    pthread_exit(NULL);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
