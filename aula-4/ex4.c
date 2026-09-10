#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO_DADOS "seguradoras.dat"

typedef struct
{
    char codigo[10];
    char nome[51];
    char seguradora[51];
    char tipo[31];
} Segurado;

typedef struct
{
    int insercoes_lidas;
    int remocoes_lidas;
} EstadoExecucao;

// --- FUNÇÕES DE CONTROLE DE ESTADO ---
EstadoExecucao ler_estado()
{
    EstadoExecucao estado = {0, 0};
    FILE *f = fopen("estado.bin", "rb");
    if (f != NULL)
    {
        fread(&estado, sizeof(EstadoExecucao), 1, f);
        fclose(f);
    }
    return estado;
}

void salvar_estado(EstadoExecucao estado)
{
    FILE *f = fopen("estado.bin", "wb");
    if (f != NULL)
    {
        fwrite(&estado, sizeof(EstadoExecucao), 1, f);
        fclose(f);
    }
}

// --- INICIALIZAÇÃO ---
void inicializar_arquivo()
{
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");
    if (f == NULL)
    {
        f = fopen(ARQUIVO_DADOS, "w+b");
        int cabecalho = -1;
        fwrite(&cabecalho, sizeof(int), 1, f);
    }
    fclose(f);
}

// --- 1. INSERÇÃO (FIRST-FIT) ---
void inserir(Segurado s)
{
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");
    if (!f)
        return;

    char registro[150];
    sprintf(registro, "%s#%s#%s#%s", s.codigo, s.nome, s.seguradora, s.tipo);
    int tam_registro = strlen(registro);

    int head, atual, anterior = -1, tam_espaco, proximo;
    fseek(f, 0, SEEK_SET);
    fread(&head, sizeof(int), 1, f);

    atual = head;
    int inserido = 0;

    while (atual != -1)
    {
        fseek(f, atual, SEEK_SET);
        fread(&tam_espaco, sizeof(int), 1, f);
        fgetc(f); // Pula o '*'
        fread(&proximo, sizeof(int), 1, f);

        if (tam_espaco >= tam_registro)
        {
            if (anterior == -1)
            {
                fseek(f, 0, SEEK_SET);
                fwrite(&proximo, sizeof(int), 1, f);
            }
            else
            {
                fseek(f, anterior + sizeof(int) + 1, SEEK_SET);
                fwrite(&proximo, sizeof(int), 1, f);
            }

            fseek(f, atual + sizeof(int), SEEK_SET);
            fwrite(registro, sizeof(char), tam_registro, f);

            int fragmento = tam_espaco - tam_registro;
            for (int i = 0; i < fragmento; i++)
                fputc('\0', f);

            inserido = 1;
            break;
        }
        anterior = atual;
        atual = proximo;
    }

    if (!inserido)
    {
        fseek(f, 0, SEEK_END);
        fwrite(&tam_registro, sizeof(int), 1, f);
        fwrite(registro, sizeof(char), tam_registro, f);
    }
    fclose(f);
}

// --- 2. REMOÇÃO ---
void remover(const char *codigo_alvo)
{
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");
    if (!f)
        return;

    int head, tam_registro, offset_atual = sizeof(int);
    char buffer[150];

    fseek(f, 0, SEEK_SET);
    fread(&head, sizeof(int), 1, f);

    fseek(f, offset_atual, SEEK_SET);
    while (fread(&tam_registro, sizeof(int), 1, f) == 1)
    {
        long pos_dados = ftell(f);
        char flag = fgetc(f);

        if (flag != '*')
        {
            fseek(f, pos_dados, SEEK_SET);
            fread(buffer, sizeof(char), tam_registro, f);
            buffer[tam_registro] = '\0';

            char *hashtag = strchr(buffer, '#');
            if (hashtag != NULL)
            {
                int tam_codigo = hashtag - buffer;
                if (strncmp(buffer, codigo_alvo, tam_codigo) == 0 && strlen(codigo_alvo) == tam_codigo)
                {

                    fseek(f, pos_dados, SEEK_SET);
                    fputc('*', f);
                    fwrite(&head, sizeof(int), 1, f);

                    fseek(f, 0, SEEK_SET);
                    fwrite(&offset_atual, sizeof(int), 1, f);
                    break;
                }
            }
        }
        offset_atual = pos_dados + tam_registro;
        fseek(f, offset_atual, SEEK_SET);
    }
    fclose(f);
}

// --- 3. COMPACTAÇÃO (CORRIGIDA) ---
void compactar()
{
    FILE *f_orig = fopen(ARQUIVO_DADOS, "rb");
    if (!f_orig)
        return;
    FILE *f_temp = fopen("temp.dat", "w+b");

    int cabecalho = -1;
    fwrite(&cabecalho, sizeof(int), 1, f_temp);

    int tam_registro;
    fseek(f_orig, sizeof(int), SEEK_SET);

    while (fread(&tam_registro, sizeof(int), 1, f_orig) == 1)
    {
        long pos_dados = ftell(f_orig);
        char flag = fgetc(f_orig);

        if (flag != '*')
        {
            fseek(f_orig, pos_dados, SEEK_SET);

            char *buffer = malloc(tam_registro + 1);
            fread(buffer, sizeof(char), tam_registro, f_orig);
            buffer[tam_registro] = '\0';

            int tamanho_real = strlen(buffer);

            fwrite(&tamanho_real, sizeof(int), 1, f_temp);
            fwrite(buffer, sizeof(char), tamanho_real, f_temp);
            free(buffer);
        }
        fseek(f_orig, pos_dados + tam_registro, SEEK_SET);
    }

    fclose(f_orig);
    fclose(f_temp);
    remove(ARQUIVO_DADOS);
    rename("temp.dat", ARQUIVO_DADOS);
    printf("Arquivo compactado com sucesso! Fragmentacao interna e externa removidas.\n");
}

// --- 4. DUMP DO ARQUIVO ---
void dump()
{
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    if (!f)
        return;

    int head;
    fread(&head, sizeof(int), 1, f);
    printf("\n--- DUMP DO ARQUIVO (Cabecalho LED: %d) ---\n", head);

    int tam_registro, offset = sizeof(int);
    fseek(f, offset, SEEK_SET);

    while (fread(&tam_registro, sizeof(int), 1, f) == 1)
    {
        long pos_dados = ftell(f);
        char flag = fgetc(f);

        printf("[Offset: %04d | Tam Gravado: %03d bytes] -> ", offset, tam_registro);
        if (flag == '*')
        {
            int prox;
            fread(&prox, sizeof(int), 1, f);
            printf("LIVRE (Proximo Offset: %d)\n", prox);
        }
        else
        {
            fseek(f, pos_dados, SEEK_SET);
            char buffer[150];
            fread(buffer, sizeof(char), tam_registro, f);
            buffer[tam_registro] = '\0';

            int tamanho_real = strlen(buffer);
            int fragmentacao = tam_registro - tamanho_real;

            printf("DADOS: %s (Frag. Interna: %d bytes)\n", buffer, fragmentacao);
        }
        offset = pos_dados + tam_registro;
        fseek(f, offset, SEEK_SET);
    }
    printf("-------------------------------------------\n");
    fclose(f);
}

// --- 5. CARREGA ARQUIVOS ---
void carregar_arquivos()
{
    EstadoExecucao estado = ler_estado();
    int lote_tamanho = 2;

    // --- Processa Inserções ---
    FILE *f_ins = fopen("insere.bin", "rb");
    if (f_ins != NULL)
    {
        fseek(f_ins, 0, SEEK_END);
        int total_ins = ftell(f_ins) / sizeof(Segurado);
        rewind(f_ins);

        Segurado *vetor_ins = malloc(total_ins * sizeof(Segurado));
        fread(vetor_ins, sizeof(Segurado), total_ins, f_ins);
        fclose(f_ins);

        int lidos_agora = 0;
        for (int i = estado.insercoes_lidas; i < total_ins && lidos_agora < lote_tamanho; i++)
        {
            inserir(vetor_ins[i]);
            estado.insercoes_lidas++;
            lidos_agora++;
            printf("  -> Inserido do lote: %s\n", vetor_ins[i].codigo);
        }
        free(vetor_ins);
    }
    else
    {
        printf("Arquivo 'insere.bin' nao encontrado.\n");
    }

    // --- Processa Remoções ---
    FILE *f_rem = fopen("remove.bin", "rb");
    if (f_rem != NULL)
    {
        typedef struct
        {
            char cod[10];
        } Codigo;

        fseek(f_rem, 0, SEEK_END);
        int total_rem = ftell(f_rem) / sizeof(Codigo);
        rewind(f_rem);

        Codigo *vetor_rem = malloc(total_rem * sizeof(Codigo));
        fread(vetor_rem, sizeof(Codigo), total_rem, f_rem);
        fclose(f_rem);

        int lidos_agora = 0;
        for (int i = estado.remocoes_lidas; i < total_rem && lidos_agora < lote_tamanho; i++)
        {
            remover(vetor_rem[i].cod);
            estado.remocoes_lidas++;
            lidos_agora++;
            printf("  -> Removido do lote: %s\n", vetor_rem[i].cod);
        }
        free(vetor_rem);
    }
    else
    {
        printf("Arquivo 'remove.bin' nao encontrado.\n");
    }

    salvar_estado(estado);
    printf("Lote concluido. Marcadores -> Insercoes: %d | Remocoes: %d\n", estado.insercoes_lidas, estado.remocoes_lidas);
}

// --- FUNÇÃO AUXILIAR PARA GERAR ARQUIVOS DE TESTE ---
void gerar_arquivos_teste()
{
    FILE *f1 = fopen("insere.bin", "rb");
    if (f1 == NULL)
    {
        f1 = fopen("insere.bin", "wb");

        Segurado dados[4];
        memset(dados, 0, sizeof(dados));

        strcpy(dados[0].codigo, "39001");
        strcpy(dados[0].nome, "Veronica");
        strcpy(dados[0].seguradora, "Porto Seguro");
        strcpy(dados[0].tipo, "Residencial");
        strcpy(dados[1].codigo, "39002");
        strcpy(dados[1].nome, "Carlos");
        strcpy(dados[1].seguradora, "SulAmerica");
        strcpy(dados[1].tipo, "Automovel");
        strcpy(dados[2].codigo, "39003");
        strcpy(dados[2].nome, "Ana");
        strcpy(dados[2].seguradora, "Allianz");
        strcpy(dados[2].tipo, "Vida");
        strcpy(dados[3].codigo, "39004");
        strcpy(dados[3].nome, "Pedro");
        strcpy(dados[3].seguradora, "Bradesco");
        strcpy(dados[3].tipo, "Saude");

        fwrite(dados, sizeof(Segurado), 4, f1);
        fclose(f1);

        FILE *f2 = fopen("remove.bin", "wb");
        typedef struct
        {
            char cod[10];
        } Codigo;
        Codigo remocoes[2];
        memset(remocoes, 0, sizeof(remocoes));

        strcpy(remocoes[0].cod, "39001");
        strcpy(remocoes[1].cod, "39003");

        fwrite(remocoes, sizeof(Codigo), 2, f2);
        fclose(f2);

        printf("Arquivos de teste 'insere.bin' e 'remove.bin' criados.\n");
    }
    else
    {
        fclose(f1);
    }
}

// --- FUNÇÃO PRINCIPAL COM MENU ---
int main()
{
    gerar_arquivos_teste();
    inicializar_arquivo();

    int opcao;
    do
    {
        printf("\n--- MENU SEGURADORAS ---\n");
        printf("1. Inserir Registro Manualmente\n");
        printf("2. Remover Registro Manualmente\n");
        printf("3. Compactar Arquivo\n");
        printf("4. Dump do Arquivo\n");
        printf("5. Carregar Lote (Processa 2 por vez)\n");
        printf("0. Sair\n");
        printf("Escolha: ");
        scanf("%d", &opcao);

        if (opcao == 1)
        {
            Segurado s;
            memset(&s, 0, sizeof(Segurado));
            printf("Codigo: ");
            scanf("%s", s.codigo);
            printf("Nome: ");
            scanf(" %[^\n]", s.nome);
            printf("Seguradora: ");
            scanf(" %[^\n]", s.seguradora);
            printf("Tipo de Seguro: ");
            scanf(" %[^\n]", s.tipo);
            inserir(s);
            printf("Registro inserido.\n");
        }
        else if (opcao == 2)
        {
            char cod[10];
            printf("Codigo a remover: ");
            scanf("%s", cod);
            remover(cod);
            printf("Remocao executada.\n");
        }
        else if (opcao == 3)
        {
            compactar();
        }
        else if (opcao == 4)
        {
            dump();
        }
        else if (opcao == 5)
        {
            carregar_arquivos();
        }
    } while (opcao != 0);

    return 0;
}