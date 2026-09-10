#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO_DADOS "dados.bin"
#define ARQUIVO_INSERE "insere.bin"
#define ARQUIVO_REMOVE "remove.bin"
#define ARQUIVO_ESTADO "estado.bin"
#define FIM_LISTA -1

typedef struct
{
    char cod[5];
    char nome[61];
    char artista[51];
    char genero[21];
} Faixa;

// Dados carregados em memória dos arquivos de teste
Faixa *faixas_insere = NULL;
int total_insere = 0;
int pos_insere = 0;

char (*codigos_remove)[5] = NULL;
int total_remove = 0;
int pos_remove = 0;

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

// Salva o estado atual (posições nos arquivos de entrada) em disco
void salvar_estado()
{
    FILE *f = fopen(ARQUIVO_ESTADO, "wb");
    if (f)
    {
        fwrite(&pos_insere, sizeof(int), 1, f);
        fwrite(&pos_remove, sizeof(int), 1, f);
        fclose(f);
    }
}

// Restaura o estado (posições) de uma execução anterior
void carregar_estado()
{
    FILE *f = fopen(ARQUIVO_ESTADO, "rb");
    if (f)
    {
        fread(&pos_insere, sizeof(int), 1, f);
        fread(&pos_remove, sizeof(int), 1, f);
        fclose(f);
    }
}

// 5. Carrega Arquivos de teste em memória
void carregar_arquivos_teste()
{
    // Carregar insere.bin (vetor de structs Faixa)
    FILE *f = fopen(ARQUIVO_INSERE, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long tam = ftell(f);
        total_insere = tam / sizeof(Faixa);
        fseek(f, 0, SEEK_SET);

        if (faixas_insere)
            free(faixas_insere);
        faixas_insere = (Faixa *)malloc(total_insere * sizeof(Faixa));
        fread(faixas_insere, sizeof(Faixa), total_insere, f);
        fclose(f);
        printf("Carregadas %d faixas de %s.\n", total_insere, ARQUIVO_INSERE);
    }
    else
    {
        printf("Arquivo %s nao encontrado.\n", ARQUIVO_INSERE);
    }

    // Carregar remove.bin (vetor de códigos de 5 bytes cada)
    f = fopen(ARQUIVO_REMOVE, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long tam = ftell(f);
        total_remove = tam / 5; // cada código: 4 chars + '\0'
        fseek(f, 0, SEEK_SET);

        if (codigos_remove)
            free(codigos_remove);
        codigos_remove = malloc(total_remove * 5);
        fread(codigos_remove, 5, total_remove, f);
        fclose(f);
        printf("Carregados %d codigos de %s.\n", total_remove, ARQUIVO_REMOVE);
    }
    else
    {
        printf("Arquivo %s nao encontrado.\n", ARQUIVO_REMOVE);
    }

    // Restaurar posições de uma execução anterior
    carregar_estado();
    printf("Posicao atual: insere=%d/%d, remove=%d/%d\n",
           pos_insere, total_insere, pos_remove, total_remove);
}

// Insere o próximo registro do arquivo insere.bin
void inserir_proximo()
{
    if (!faixas_insere)
    {
        printf("Arquivos nao carregados. Use opcao 5 primeiro.\n");
        return;
    }
    if (pos_insere >= total_insere)
    {
        printf("Todos os registros de insercao ja foram utilizados (%d/%d).\n",
               pos_insere, total_insere);
        return;
    }
    Faixa fx = faixas_insere[pos_insere];
    inserir(fx);
    printf("Inserida faixa [%d/%d]: %s|%s|%s|%s\n",
           pos_insere + 1, total_insere,
           fx.cod, fx.nome, fx.artista, fx.genero);
    pos_insere++;
    salvar_estado();
}

// Remove o próximo registro usando código do arquivo remove.bin
void remover_proximo()
{
    if (!codigos_remove)
    {
        printf("Arquivos nao carregados. Use opcao 5 primeiro.\n");
        return;
    }
    if (pos_remove >= total_remove)
    {
        printf("Todos os codigos de remocao ja foram utilizados (%d/%d).\n",
               pos_remove, total_remove);
        return;
    }
    printf("Removendo faixa [%d/%d] com codigo: %s\n",
           pos_remove + 1, total_remove, codigos_remove[pos_remove]);
    remover(codigos_remove[pos_remove]);
    pos_remove++;
    salvar_estado();
}

int main()
{
    inicializar_arquivo();

    int opcao;
    do
    {
        printf("\n=== Sistema de Streaming Musical ===\n");
        printf("1. Inserir faixa\n");
        printf("2. Remover faixa\n");
        printf("3. Compactar arquivo\n");
        printf("4. Dump do arquivo\n");
        printf("5. Carregar arquivos de teste\n");
        printf("0. Sair\n");
        printf("Opcao: ");
        scanf("%d", &opcao);

        switch (opcao)
        {
        case 1:
            inserir_proximo();
            break;
        case 2:
            remover_proximo();
            break;
        case 3:
            compactar();
            printf("Arquivo compactado com sucesso.\n");
            break;
        case 4:
            dump();
            break;
        case 5:
            carregar_arquivos_teste();
            break;
        case 0:
            printf("Saindo...\n");
            break;
        default:
            printf("Opcao invalida.\n");
            break;
        }
    } while (opcao != 0);

    // Liberar memória alocada
    if (faixas_insere)
        free(faixas_insere);
    if (codigos_remove)
        free(codigos_remove);

    return 0;
}