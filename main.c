//Alex Kodama 10417942
//Thomas Pinheiro 10418118
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// ESTRUTURAS
typedef struct {
    int bins[256];
    double media;
    double desvio_padrao;
    int total_pixels;
} Histograma;

// FUNÇÕES AUXILIARES DE CLASSIFICAÇÃO
const char* classificarLuminosidade(double media) {
    if (media < 85) return "Escura";
    if (media > 170) return "Clara";
    return "Media";
}

const char* classificarContraste(double desvio) {
    if (desvio < 40) return "Baixo";
    if (desvio > 80) return "Alto";
    return "Medio";
}

// PROCESSAMENTO DE IMAGEM
void converterParaCinza(SDL_Surface* surface) {
    SDL_LockSurface(surface);
    const SDL_PixelFormatDetails *formatDetails = SDL_GetPixelFormatDetails(surface->format);
    SDL_Palette *palette = SDL_GetSurfacePalette(surface);

    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            Uint32* pixel = (Uint32*)((Uint8*)surface->pixels + y * surface->pitch + x * formatDetails->bytes_per_pixel);
            Uint8 r, g, b, a;
            SDL_GetRGBA(*pixel, formatDetails, palette, &r, &g, &b, &a);

            if (!(r == g && g == b)) {
                Uint8 y_gray = (Uint8)(0.2125 * r + 0.7154 * g + 0.0721 * b);
                *pixel = SDL_MapRGBA(formatDetails, palette, y_gray, y_gray, y_gray, a);
            }
        }
    }
    SDL_UnlockSurface(surface);
}

Histograma calcularHistograma(SDL_Surface* surface) {
    Histograma hist = {0};
    hist.total_pixels = surface->w * surface->h;

    SDL_LockSurface(surface);
    Uint8* pixels = (Uint8*)surface->pixels;
    const SDL_PixelFormatDetails *formatDetails = SDL_GetPixelFormatDetails(surface->format);
    SDL_Palette *palette = SDL_GetSurfacePalette(surface);

    for (int i = 0; i < hist.total_pixels; i++) {
        Uint32 pixel;
        if(formatDetails->bytes_per_pixel == 4) {
             pixel = ((Uint32*)pixels)[i];
        } else if (formatDetails->bytes_per_pixel == 3) {
             pixel = pixels[i * 3] | (pixels[i * 3 + 1] << 8) | (pixels[i * 3 + 2] << 16);
        } else {
             pixel = ((Uint8*)pixels)[i];
        }

        Uint8 y_gray, g, b, a;
        SDL_GetRGBA(pixel, formatDetails, palette, &y_gray, &g, &b, &a);
        hist.bins[y_gray]++;
    }
    SDL_UnlockSurface(surface);

    double soma = 0;
    for (int i = 0; i < 256; i++) {
        soma += i * hist.bins[i];
    }
    hist.media = soma / hist.total_pixels;

    double soma_variancia = 0;
    for (int i = 0; i < 256; i++) {
        soma_variancia += hist.bins[i] * pow(i - hist.media, 2);
    }
    hist.desvio_padrao = sqrt(soma_variancia / hist.total_pixels);

    return hist;
}

void equalizarImagem(SDL_Surface* original, SDL_Surface* destino, Histograma* histOriginal) {
    int cdf[256] = {0};
    int min_cdf = 0;

    cdf[0] = histOriginal->bins[0];
    for (int i = 1; i < 256; i++) {
        cdf[i] = cdf[i-1] + histOriginal->bins[i];
    }
    for (int i = 0; i < 256; i++) {
        if (cdf[i] > 0) {
            min_cdf = cdf[i];
            break;
        }
    }

    Uint8 mapa[256];
    for (int i = 0; i < 256; i++) {
        mapa[i] = (Uint8)round(((double)(cdf[i] - min_cdf) / (histOriginal->total_pixels - min_cdf)) * 255.0);
    }

    SDL_LockSurface(original);
    SDL_LockSurface(destino);
    const SDL_PixelFormatDetails *formatDetails = SDL_GetPixelFormatDetails(original->format);
    SDL_Palette *palette = SDL_GetSurfacePalette(original);

    Uint32* pixOrig = (Uint32*)original->pixels;
    Uint32* pixDest = (Uint32*)destino->pixels;

    for (int i = 0; i < histOriginal->total_pixels; i++) {
        Uint8 y, g, b, a;
        SDL_GetRGBA(pixOrig[i], formatDetails, palette, &y, &g, &b, &a);
        Uint8 novo_y = mapa[y];
        pixDest[i] = SDL_MapRGBA(formatDetails, palette, novo_y, novo_y, novo_y, a);
    }
    SDL_UnlockSurface(destino);
    SDL_UnlockSurface(original);
}

// FUNÇÃO PRINCIPAL
int main(int argc, char* argv[]) {
    // 1. Argumentos da linha de comando
    if (argc < 2) {
        printf("Uso: %s <caminho_da_imagem.ext>\n", argv[0]);
        return 1;
    }
    const char* imagePath = argv[1];

    // 2. Inicialização das bibliotecas
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Erro SDL: %s\n", SDL_GetError()); return 1;
    }
    if (TTF_Init() < 0) {
        printf("Erro TTF: %s\n", SDL_GetError());
    }

    // 3. Carregar e preparar a imagem
    SDL_Surface* imgSurface = IMG_Load(imagePath);
    if (!imgSurface) {
        printf("Erro ao carregar a imagem '%s'.\n", imagePath);
        SDL_Quit(); return 1;
    }

    // Garantir formato 32-bits para simplificar manipulação
    SDL_Surface* surfaceFormatada = SDL_ConvertSurface(imgSurface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(imgSurface);

    // Converte para cinza
    converterParaCinza(surfaceFormatada);

    // Cria um backup da imagem em cinza para a reversão
    SDL_Surface* surfaceBackupCinza = SDL_DuplicateSurface(surfaceFormatada);

    // 4. Criação das Janelas
    SDL_Window* mainWindow = SDL_CreateWindow("Proj1 - Imagem Principal", surfaceFormatada->w, surfaceFormatada->h, 0);
    SDL_Renderer* mainRenderer = SDL_CreateRenderer(mainWindow, NULL);
    SDL_SetWindowPosition(mainWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    int main_x, main_y;
    SDL_GetWindowPosition(mainWindow, &main_x, &main_y);
    SDL_Texture* imageTexture = SDL_CreateTextureFromSurface(mainRenderer, surfaceFormatada);

    SDL_Window* secWindow = SDL_CreateWindow("Proj1 - Ferramentas", 400, 600, 0);
    SDL_Renderer* secRenderer = SDL_CreateRenderer(secWindow, NULL);
    SDL_SetWindowPosition(secWindow, main_x + surfaceFormatada->w + 10, main_y);

    // 5. Configuração da Interface Secundária
    Histograma histAtual = calcularHistograma(surfaceFormatada);
    TTF_Font* font = TTF_OpenFont("fonte.ttf", 16); // COLOQUE UMA FONTE .TTF NA PASTA!
    if (!font) {
        printf("Aviso: Fonte 'fonte.ttf' nao encontrada. Estatisticas apenas no console.\n");
        printf("Luminosidade: %s | Contraste: %s\n", classificarLuminosidade(histAtual.media), classificarContraste(histAtual.desvio_padrao));
    }

    SDL_FRect btnRect = {50, 450, 300, 50};
    int btnHover = 0;
    int isEqualized = 0;
    int running = 1;
    SDL_Event event;

    // 6. Loop Principal
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.key.key == SDLK_ESCAPE) {
                running = 0;
            }

            // Tratamento de Teclado (Salvar Imagem)
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_S) {
                if (IMG_SavePNG(surfaceFormatada, "output_image.png") == 0) {
                    printf("Imagem salva em 'output_image.png'!\n");
                }
            }

            // Capturar mouse na janela secundária
            if (event.window.windowID == SDL_GetWindowID(secWindow)) {
                if (event.type == SDL_EVENT_MOUSE_MOTION) {
                    float mx = event.motion.x; float my = event.motion.y;
                    btnHover = (mx >= btnRect.x && mx <= btnRect.x + btnRect.w && my >= btnRect.y && my <= btnRect.y + btnRect.h);
                }

                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT && btnHover) {
                    if (!isEqualized) {
                        equalizarImagem(surfaceBackupCinza, surfaceFormatada, &histAtual);
                        isEqualized = 1;
                    } else {
                        SDL_BlitSurface(surfaceBackupCinza, NULL, surfaceFormatada, NULL);
                        isEqualized = 0;
                    }

                    // Atualiza Textura e Histograma
                    SDL_DestroyTexture(imageTexture);
                    imageTexture = SDL_CreateTextureFromSurface(mainRenderer, surfaceFormatada);
                    histAtual = calcularHistograma(surfaceFormatada);

                    if(!font) {
                         printf("Atualizado - Luminosidade: %s | Contraste: %s\n", classificarLuminosidade(histAtual.media), classificarContraste(histAtual.desvio_padrao));
                    }
                }
            }
        }

        // --- Renderização da Janela Principal ---
        SDL_RenderClear(mainRenderer);
        SDL_RenderTexture(mainRenderer, imageTexture, NULL, NULL);
        SDL_RenderPresent(mainRenderer);

        // --- Renderização da Janela Secundária ---
        SDL_SetRenderDrawColor(secRenderer, 40, 40, 40, 255); // Fundo escuro
        SDL_RenderClear(secRenderer);

        // Desenhar Histograma (Gráfico de Barras Simples)
        SDL_SetRenderDrawColor(secRenderer, 200, 200, 200, 255);
        int max_bin = 0;
        for(int i=0; i<256; i++) if(histAtual.bins[i] > max_bin) max_bin = histAtual.bins[i];

        for (int i = 0; i < 256; i++) {
            float alturaBarra = (float)histAtual.bins[i] / max_bin * 200.0f;
            SDL_FRect barra = { 72 + i, 300 - alturaBarra, 1, alturaBarra };
            SDL_RenderFillRect(secRenderer, &barra);
        }

        // Desenhar Botão
        if (btnHover) {
            if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) SDL_SetRenderDrawColor(secRenderer, 0, 0, 139, 255);
            else SDL_SetRenderDrawColor(secRenderer, 100, 149, 237, 255);
        } else {
            SDL_SetRenderDrawColor(secRenderer, 0, 0, 255, 255);
        }
        SDL_RenderFillRect(secRenderer, &btnRect);

        // Renderizar Textos (se a fonte foi carregada)
        if (font) {
            SDL_Color textColor = {255, 255, 255, 255};

            // Texto do botão
            const char* btnText = isEqualized ? "Ver original" : "Equalizar";
            SDL_Surface* textSurf = TTF_RenderText_Solid(font, btnText, 0, textColor);
            SDL_Texture* textTex = SDL_CreateTextureFromSurface(secRenderer, textSurf);
            SDL_FRect tRect = { btnRect.x + btnRect.w/2 - textSurf->w/2, btnRect.y + btnRect.h/2 - textSurf->h/2, textSurf->w, textSurf->h };
            SDL_RenderTexture(secRenderer, textTex, NULL, &tRect);
            SDL_DestroySurface(textSurf); SDL_DestroyTexture(textTex);

            // Texto das Estatísticas
            char statsText[100];
            snprintf(statsText, sizeof(statsText), "Lum: %s | Cont: %s", classificarLuminosidade(histAtual.media), classificarContraste(histAtual.desvio_padrao));
            SDL_Surface* statSurf = TTF_RenderText_Solid(font, statsText, 0, textColor);
            SDL_Texture* statTex = SDL_CreateTextureFromSurface(secRenderer, statSurf);
            SDL_FRect sRect = { 50, 350, statSurf->w, statSurf->h };
            SDL_RenderTexture(secRenderer, statTex, NULL, &sRect);
            SDL_DestroySurface(statSurf); SDL_DestroyTexture(statTex);
        }

        SDL_RenderPresent(secRenderer);
    }

    // 7. Limpeza
    if (font) TTF_CloseFont(font);
    SDL_DestroyTexture(imageTexture);
    SDL_DestroyRenderer(mainRenderer);
    SDL_DestroyRenderer(secRenderer);
    SDL_DestroyWindow(mainWindow);
    SDL_DestroyWindow(secWindow);
    SDL_DestroySurface(surfaceFormatada);
    SDL_DestroySurface(surfaceBackupCinza);
    TTF_Quit();
}
