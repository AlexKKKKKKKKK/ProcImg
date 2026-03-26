#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Estrutura para armazenar os dados de análise da imagem
typedef struct {
    int histogram[256];
    double mean;
    double stddev;
    int total_pixels;
} ImageStats;

// Converte a imagem para escala de cinza e a padroniza para um formato de 32 bits
SDL_Surface* convert_to_grayscale(SDL_Surface* original) {
    SDL_Surface* gray_surface = SDL_ConvertSurfaceFormat(original, SDL_PIXELFORMAT_ARGB8888);
    if (!gray_surface) return NULL;

    Uint32* pixels = (Uint32*)gray_surface->pixels;
    int pixel_count = gray_surface->w * gray_surface->h;

    int is_colored = 0; // Flag para verificar se já era cinza

    for (int i = 0; i < pixel_count; ++i) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], gray_surface->format, &r, &g, &b, &a);

        if (r != g || r != b) {
            is_colored = 1;
        }

        // Aplica a fórmula exigida no enunciado
        Uint8 y = (Uint8)(0.2125 * r + 0.7154 * g + 0.0721 * b);
        pixels[i] = SDL_MapRGBA(gray_surface->format, y, y, y, a);
    }

    if (is_colored) printf("Imagem colorida detectada. Convertida para escala de cinza.\n");
    else printf("A imagem já estava em escala de cinza.\n");

    return gray_surface;
}

// Calcula o histograma, média e desvio padrão
void calculate_stats(SDL_Surface* img, ImageStats* stats) {
    for (int i = 0; i < 256; i++) stats->histogram[i] = 0;
    stats->total_pixels = img->w * img->h;

    Uint32* pixels = (Uint32*)img->pixels;
    double sum = 0;

    for (int i = 0; i < stats->total_pixels; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], img->format, &r, &g, &b, &a);
        stats->histogram[r]++;
        sum += r;
    }

    stats->mean = sum / stats->total_pixels;

    double variance_sum = 0;
    for (int i = 0; i < 256; i++) {
        variance_sum += stats->histogram[i] * pow(i - stats->mean, 2);
    }
    stats->stddev = sqrt(variance_sum / stats->total_pixels);

    // Classificação
    printf("\n--- Analise do Histograma ---\n");
    printf("Media: %.2f -> Imagem %s\n", stats->mean,
           (stats->mean < 85) ? "Escura" : (stats->mean > 170) ? "Clara" : "Media");
    printf("Desvio Padrao: %.2f -> Contraste %s\n", stats->stddev,
           (stats->stddev < 40) ? "Baixo" : (stats->stddev > 80) ? "Alto" : "Medio");
}

// Equaliza o histograma retornando uma nova surface
SDL_Surface* equalize_histogram(SDL_Surface* original, ImageStats* stats) {
    SDL_Surface* eq_surface = SDL_ConvertSurfaceFormat(original, SDL_PIXELFORMAT_ARGB8888);
    int cdf[256] = {0};

    cdf[0] = stats->histogram[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i-1] + stats->histogram[i];
    }

    int cdf_min = 0;
    for (int i = 0; i < 256; i++) {
        if (cdf[i] > 0) { cdf_min = cdf[i]; break; }
    }

    Uint32* pixels = (Uint32*)eq_surface->pixels;
    for (int i = 0; i < stats->total_pixels; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], eq_surface->format, &r, &g, &b, &a);

        float h_v = (float)(cdf[r] - cdf_min) / (stats->total_pixels - cdf_min);
        Uint8 new_val = (Uint8)round(h_v * 255.0f);

        pixels[i] = SDL_MapRGBA(eq_surface->format, new_val, new_val, new_val, a);
    }
    return eq_surface;
}

int main(int argc, char* argv[]) {
    // 1. Tratamento da linha de comando
    if (argc != 2) {
        printf("Uso: %s <caminho_da_imagem.ext>\n", argv[0]);
        return 1;
    }

    // Inicialização da SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Erro ao inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    // 2. Carregamento da imagem
    SDL_Surface* raw_image = IMG_Load(argv[1]);
    if (!raw_image) {
        printf("Erro ao carregar a imagem %s. Formato invalido ou arquivo nao encontrado. (%s)\n", argv[1], IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // Processamento Inicial
    SDL_Surface* gray_image = convert_to_grayscale(raw_image);
    SDL_DestroySurface(raw_image);

    ImageStats stats_gray, stats_eq;
    calculate_stats(gray_image, &stats_gray);
    SDL_Surface* eq_image = equalize_histogram(gray_image, &stats_gray);
    calculate_stats(eq_image, &stats_eq);

    // Variáveis de estado
    SDL_Surface* current_surface = gray_image;
    ImageStats* current_stats = &stats_gray;
    int is_equalized = 0;

    // 3. Setup das Janelas e Renderizadores
    SDL_Window* main_window = SDL_CreateWindow("Proj1 - Imagem", current_surface->w, current_surface->h, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer* main_renderer = SDL_CreateRenderer(main_window, NULL);

    // Centraliza a principal e pega a posição para colocar a secundária ao lado
    SDL_SetWindowPosition(main_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    int main_x, main_y;
    SDL_GetWindowPosition(main_window, &main_x, &main_y);

    int sec_w = 400, sec_h = 600;
    SDL_Window* sec_window = SDL_CreateWindow("Proj1 - Controles e Histograma", sec_w, sec_h, 0);
    SDL_Renderer* sec_renderer = SDL_CreateRenderer(sec_window, NULL);
    SDL_SetWindowPosition(sec_window, main_x + current_surface->w + 10, main_y);

    // Retângulos e texturas
    SDL_FRect btn_rect = {50, sec_h - 100, (float)sec_w - 100, 60};
    SDL_Texture* tex_gray = SDL_CreateTextureFromSurface(main_renderer, gray_image);
    SDL_Texture* tex_eq = SDL_CreateTextureFromSurface(main_renderer, eq_image);
    SDL_Texture* current_texture = tex_gray;

    int running = 1;
    SDL_Event event;
    int btn_state = 0; // 0: Neutro, 1: Hover, 2: Clicked

    while (running) {
        // --- EVENTOS ---
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            // 6. Salvar Imagem com 'S'
            else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_S) {
                if (IMG_SavePNG(current_surface, "output_image.png") == 0) {
                    printf("Imagem salva como 'output_image.png' com sucesso!\n");
                } else {
                    printf("Erro ao salvar imagem: %s\n", IMG_GetError());
                }
            }
            // Interação do Mouse no botão (Janela Secundária)
            else if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                float mx = event.motion.x;
                float my = event.motion.y;
                SDL_Window* focus = SDL_GetMouseFocus();

                if (focus == sec_window) {
                    int inside = (mx >= btn_rect.x && mx <= (btn_rect.x + btn_rect.w) &&
                                  my >= btn_rect.y && my <= (btn_rect.y + btn_rect.h));
                    if (!inside) {
                        btn_state = 0;
                    } else {
                        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                            btn_state = 2; // Clicado
                        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                            if (btn_state == 2) { // Soltou o clique dentro
                                // 5. Alternar Equalização
                                is_equalized = !is_equalized;
                                current_surface = is_equalized ? eq_image : gray_image;
                                current_texture = is_equalized ? tex_eq : tex_gray;
                                current_stats = is_equalized ? &stats_eq : &stats_gray;
                                printf("Modo: %s\n", is_equalized ? "Equalizado" : "Original (Cinza)");
                            }
                            btn_state = 1; // Volta pra hover
                        } else if (btn_state != 2) {
                            btn_state = 1; // Hover
                        }
                    }
                } else {
                    btn_state = 0;
                }
            }
        }

        // --- RENDERIZAR JANELA PRINCIPAL ---
        SDL_SetRenderDrawColor(main_renderer, 0, 0, 0, 255);
        SDL_RenderClear(main_renderer);
        SDL_RenderTexture(main_renderer, current_texture, NULL, NULL);
        SDL_RenderPresent(main_renderer);

        // --- RENDERIZAR JANELA SECUNDÁRIA ---
        SDL_SetRenderDrawColor(sec_renderer, 30, 30, 30, 255); // Fundo cinza escuro
        SDL_RenderClear(sec_renderer);

        // 4. Desenhar Histograma
        SDL_SetRenderDrawColor(sec_renderer, 255, 255, 255, 255);
        int max_val = 0;
        for (int i = 0; i < 256; i++) {
            if (current_stats->histogram[i] > max_val) max_val = current_stats->histogram[i];
        }

        float hist_w = 256.0f;
        float hist_h = 200.0f;
        float start_x = (sec_w - hist_w) / 2.0f;
        float start_y = 50.0f + hist_h;

        for (int i = 0; i < 256; i++) {
            float h = ((float)current_stats->histogram[i] / max_val) * hist_h;
            SDL_RenderLine(sec_renderer, start_x + i, start_y, start_x + i, start_y - h);
        }

        // 5. Desenhar Botão (Azul Neutro, Azul Claro Hover, Azul Escuro Clicado)
        if (btn_state == 0) SDL_SetRenderDrawColor(sec_renderer, 0, 100, 200, 255);      // Neutro
        else if (btn_state == 1) SDL_SetRenderDrawColor(sec_renderer, 50, 150, 255, 255); // Hover
        else SDL_SetRenderDrawColor(sec_renderer, 0, 50, 150, 255);                       // Clicado

        SDL_RenderFillRect(sec_renderer, &btn_rect);

        SDL_RenderPresent(sec_renderer);
        SDL_Delay(16); // ~60 FPS
    }

    // Cleanup
    SDL_DestroyTexture(tex_gray);
    SDL_DestroyTexture(tex_eq);
    SDL_DestroySurface(gray_image);
    SDL_DestroySurface(eq_image);
    SDL_DestroyRenderer(main_renderer);
    SDL_DestroyWindow(main_window);
    SDL_DestroyRenderer(sec_renderer);
    SDL_DestroyWindow(sec_window);
    SDL_Quit();

    return 0;
}
