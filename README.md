# Processamento de Imagem

Projeto de processamento de imagem em C usando a biblioteca SDL3 para análise e equalização de imagens. É usada a biblioteca SDL3 para criação de GUI interativa e SDL3_image para carregamento das imagens, converte a imagem para tons de cinza e classifica as imagens quanto a luminosidade usando a média dos pixels e o contraste com o desvio padrão, além de plotar um histograma para análise. Eu (Alex) fiz a primeira versão do código, mas não ficou bom, então o Thomas ajeitou ele, e ambos contribuimos  com README.

# Compilação

Rodamos o código no WSL2, instalando as bibliotecas SDL, SDL_image e SDL_ttf localmente via git clone, cmake -S . -B build e cmake --build build para compilar usamos:

gcc main3.c -o programa -lSDL3 -lSDL3_image -lSDL3_ttf -lm

## Tivemos erros de linking com a biblioteca SDL3_ttf

Para consertar usamos:

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
