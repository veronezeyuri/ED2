#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO_DADOS "dados.bin"
#define FIM_LISTA -1

typedef struct
{
    char cod[5];
    char nome[61];
    char artista[51];
    char genero[21];
} Faixa;

// Inicializa o arquivo com o cabeçalho se ele não existir
void inicializar_arquivo()
{
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    if (!f)
    {
        f = fopen(ARQUIVO_DADOS, "wb");
        int head = FIM_LISTA;
        fwrite(&head, sizeof(int), 1, f); // Offset para primeiro elemento
        fclose(f);
    }
    else
    {
        fclose(f);
    }
}

// 1. Inserção com Best-Fit
void inserir(Faixa fx)
{
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");
    char payload[150];
    sprintf(payload, "%s|%s|%s|%s", fx.cod, fx.nome, fx.artista, fx.genero);
    int req_size = strlen(payload) + 1; // +1 para o \0

    int head;
    fseek(f, 0, SEEK_SET);
    fread(&head, sizeof(int), 1, f);

    int best_off = -1, prev_best = -1, min_diff = 9999999;
    int curr_off = head, prev_off = -1;

    // Busca o best-fit na lista de espaços disponíveis
    while (curr_off != FIM_LISTA)
    {
        int size, next;
        char m;
        fseek(f, curr_off, SEEK_SET);
        fread(&size, sizeof(int), 1, f);
        fread(&m, sizeof(char), 1, f);
        fread(&next, sizeof(int), 1, f);

        if (size >= req_size)
        {
            if (size - req_size < min_diff)
            {
                min_diff = size - req_size;
                best_off = curr_off;
                prev_best = prev_off;
            }
        }
        prev_off = curr_off;
        curr_off = next;
    }

    if (best_off != -1)
    {
        // Encontrou espaço: remove da lista encadeada
        int next_best;
        fseek(f, best_off + sizeof(int) + sizeof(char), SEEK_SET);
        fread(&next_best, sizeof(int), 1, f);

        if (prev_best == -1)
        {
            fseek(f, 0, SEEK_SET);
            fwrite(&next_best, sizeof(int), 1, f);
        }
        else
        {
            fseek(f, prev_best + sizeof(int) + sizeof(char), SEEK_SET);
            fwrite(&next_best, sizeof(int), 1, f);
        }
        // Sobrescreve dados aproveitando a fragmentação interna
        fseek(f, best_off + sizeof(int), SEEK_SET);
        fwrite(payload, sizeof(char), req_size, f);
    }
    else
    {
        // Não encontrou espaço: insere no final do arquivo
        fseek(f, 0, SEEK_END);
        fwrite(&req_size, sizeof(int), 1, f);
        fwrite(payload, sizeof(char), req_size, f);
    }
    fclose(f);
}

// 2. Remoção
void remover(const char *cod_alvo)
{
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");
    int head, curr = sizeof(int);
    fseek(f, 0, SEEK_SET);
    fread(&head, sizeof(int), 1, f);

    fseek(f, 0, SEEK_END);
    int end = ftell(f);

    while (curr < end)
    {
        int size;
        fseek(f, curr, SEEK_SET);
        fread(&size, sizeof(int), 1, f);
        char *buf = malloc(size);
        fread(buf, sizeof(char), size, f);

        if (buf[0] != '@')
        {
            if (strncmp(buf, cod_alvo, 4) == 0)
            {
                // Marca como removido e aponta próximo para o fim da lista
                fseek(f, curr + sizeof(int), SEEK_SET);
                char marker = '@';
                int next_off = FIM_LISTA;
                fwrite(&marker, sizeof(char), 1, f);
                fwrite(&next_off, sizeof(int), 1, f);

                // Adiciona o novo espaço sempre no final da lista
                if (head == FIM_LISTA)
                {
                    fseek(f, 0, SEEK_SET);
                    fwrite(&curr, sizeof(int), 1, f);
                }
                else
                {
                    int tail_off = head;
                    int next_tail;
                    while (1)
                    {
                        fseek(f, tail_off + sizeof(int) + sizeof(char), SEEK_SET);
                        fread(&next_tail, sizeof(int), 1, f);
                        if (next_tail == FIM_LISTA)
                            break;
                        tail_off = next_tail;
                    }
                    fseek(f, tail_off + sizeof(int) + sizeof(char), SEEK_SET);
                    fwrite(&curr, sizeof(int), 1, f);
                }
                free(buf);
                break;
            }
        }
        free(buf);
        curr += sizeof(int) + size;
    }
    fclose(f);
}

// 3. Compactação
void compactar()
{
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    FILE *tmp = fopen("temp.bin", "wb");

    int empty_head = FIM_LISTA;
    fwrite(&empty_head, sizeof(int), 1, tmp);

    int curr = sizeof(int);
    fseek(f, 0, SEEK_END);
    int end = ftell(f);

    while (curr < end)
    {
        int size;
        fseek(f, curr, SEEK_SET);
        fread(&size, sizeof(int), 1, f);
        char *buf = malloc(size);
        fread(buf, sizeof(char), size, f);

        // Se válido, limpa também a fragmentação interna escrevendo o tamanho exato
        if (buf[0] != '@')
        {
            int real_size = strlen(buf) + 1;
            fwrite(&real_size, sizeof(int), 1, tmp);
            fwrite(buf, sizeof(char), real_size, tmp);
        }
        free(buf);
        curr += sizeof(int) + size;
    }

    fclose(f);
    fclose(tmp);
    remove(ARQUIVO_DADOS);
    rename("temp.bin", ARQUIVO_DADOS);
}

// 4. Dump do Arquivo
void dump()
{
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    if (!f)
        return;

    int head;
    fread(&head, sizeof(int), 1, f);
    printf("--- DUMP (HEAD Offset: %d) ---\n", head);

    int curr = sizeof(int);
    fseek(f, 0, SEEK_END);
    int end = ftell(f);

    while (curr < end)
    {
        int size;
        fseek(f, curr, SEEK_SET);
        fread(&size, sizeof(int), 1, f);
        char *buf = malloc(size);
        fread(buf, sizeof(char), size, f);

        if (buf[0] == '@')
        {
            int next;
            memcpy(&next, buf + 1, sizeof(int));
            printf("[Off: %4d] Tamanho: %3d | REMOVIDO | Prox: %d\n", curr, size, next);
        }
        else
        {
            printf("[Off: %4d] Tamanho: %3d | DADOS: %s\n", curr, size, buf);
        }
        free(buf);
        curr += sizeof(int) + size;
    }
    printf("------------------------------\n");
    fclose(f);
}

// 5. Carrega Arquivos (Esqueleto para Testes em Memória)
void carregar_arquivos_teste()
{
    // Implementação dependente da estrutura dos arquivos "insere.bin" e "remove.bin"
    // Sugere-se ler tudo para um vetor de struct e percorrê-lo aos poucos.
}

int main()
{
    inicializar_arquivo();

    // Teste Simples
    Faixa f1 = {"7042", "Aguas de Marco", "Elis Regina", "MPB"};
    Faixa f2 = {"1234", "Bohemian Rhapsody", "Queen", "Rock"};
    Faixa f3 = {"9999", "Shape of You", "Ed Sheeran", "Pop"};

    inserir(f1);
    inserir(f2);
    inserir(f3);

    remover("1234");
    dump();

    Faixa f4 = {"5555", "Halo", "Beyonce", "Pop"};
    inserir(f4); // Testará o best-fit no espaço do 1234
    dump();

    compactar();
    dump();

    return 0;
}