#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_REG_MAX 512
#define TAM_CAMPO_MAX 100

int pega_registro(FILE *p_out, char *p_reg)
{
    int bytes;

    if (!fread(&bytes, sizeof(int), 1, p_out))
        return 0;
    else
    {
        fread(p_reg, bytes, 1, p_out);
        p_reg[bytes] = '\0';
        return bytes;
    }
}

int pega_campo(char *p_registro, int *p_pos, char *p_campo)
{
    char ch;
    int i = 0;

    p_campo[i] = '\0';

    ch = p_registro[*p_pos];
    while ((ch != '|') && (ch != EOF) && (ch != '\0'))
    {
        p_campo[i] = ch;
        i++;
        ch = p_registro[++(*p_pos)];
    }
    ++(*p_pos);
    p_campo[i] = '\0';

    return strlen(p_campo);
}

void grava_registro(FILE *p_in, const char *p_reg)
{
    int bytes = (int)strlen(p_reg);
    fwrite(&bytes, sizeof(int), 1, p_in);
    fwrite(p_reg, sizeof(char), bytes, p_in);
}

void extrai_cpf(char *registro, char *cpf)
{
    int pos = 0;
    pega_campo(registro, &pos, cpf);
}

void imprime_registro(char *registro)
{
    int pos = 0;
    char cpf[TAM_CAMPO_MAX];
    char nome[TAM_CAMPO_MAX];
    char sobrenome[TAM_CAMPO_MAX];
    char telefone[TAM_CAMPO_MAX];
    char cidade[TAM_CAMPO_MAX];

    pega_campo(registro, &pos, cpf);
    pega_campo(registro, &pos, nome);
    pega_campo(registro, &pos, sobrenome);
    pega_campo(registro, &pos, telefone);
    pega_campo(registro, &pos, cidade);

    printf("  CPF: %s | Nome: %s %s | Tel: %s | Cidade: %s\n",
           cpf, nome, sobrenome, telefone, cidade);
}

void monta_registro(char *destino, const char *cpf, const char *nome,
                    const char *sobrenome, const char *telefone, const char *cidade)
{
    sprintf(destino, "%s|%s|%s|%s|%s|", cpf, nome, sobrenome, telefone, cidade);
}

int inserir_ordenado(const char *nome_arquivo, const char *novo_reg)
{
    char novo_cpf[TAM_CAMPO_MAX];
    char reg_atual[TAM_REG_MAX];
    char cpf_atual[TAM_CAMPO_MAX];
    char reg_temp[TAM_REG_MAX];
    strcpy(reg_temp, novo_reg);
    extrai_cpf(reg_temp, novo_cpf);

    FILE *f_in = fopen(nome_arquivo, "rb");

    if (f_in == NULL)
    {
        FILE *f_novo = fopen(nome_arquivo, "wb");
        if (f_novo == NULL)
        {
            printf("Erro ao criar arquivo %s.\n", nome_arquivo);
            return 0;
        }
        grava_registro(f_novo, novo_reg);
        fclose(f_novo);
        return 1;
    }

    FILE *f_temp = fopen("temp.bin", "wb");
    if (f_temp == NULL)
    {
        printf("Erro ao criar arquivo temporario.\n");
        fclose(f_in);
        return 0;
    }

    int inserido = 0;
    int duplicado = 0;

    while (pega_registro(f_in, reg_atual) > 0)
    {
        extrai_cpf(reg_atual, cpf_atual);
        int cmp = strcmp(novo_cpf, cpf_atual);

        if (cmp == 0)
        {
            duplicado = 1;
            break;
        }
        else if (cmp < 0 && !inserido)
        {
            grava_registro(f_temp, novo_reg);
            inserido = 1;
            grava_registro(f_temp, reg_atual);
        }
        else
        {
            grava_registro(f_temp, reg_atual);
        }
    }

    if (duplicado)
    {
        fclose(f_in);
        fclose(f_temp);
        remove("temp.bin");
        printf("Erro: CPF %s ja cadastrado no sistema (duplicatas nao permitidas).\n", novo_cpf);
        return 0;
    }

    if (!inserido)
    {
        grava_registro(f_temp, novo_reg);
    }

    fclose(f_in);
    fclose(f_temp);

    remove(nome_arquivo);
    rename("temp.bin", nome_arquivo);
    return 1;
}

int remover_fisico(const char *nome_arquivo, const char *cpf_alvo)
{
    FILE *f_in = fopen(nome_arquivo, "rb");
    if (f_in == NULL)
    {
        printf("Arquivo %s nao encontrado.\n", nome_arquivo);
        return 0;
    }

    FILE *f_temp = fopen("temp.bin", "wb");
    if (f_temp == NULL)
    {
        printf("Erro ao criar arquivo temporario.\n");
        fclose(f_in);
        return 0;
    }

    char reg_atual[TAM_REG_MAX];
    char cpf_atual[TAM_CAMPO_MAX];
    int removido = 0;

    while (pega_registro(f_in, reg_atual) > 0)
    {
        extrai_cpf(reg_atual, cpf_atual);
        if (strcmp(cpf_atual, cpf_alvo) == 0)
        {
            removido = 1;
        }
        else
        {
            grava_registro(f_temp, reg_atual);
        }
    }

    fclose(f_in);
    fclose(f_temp);

    if (removido)
    {
        remove(nome_arquivo);
        rename("temp.bin", nome_arquivo);
        return 1;
    }
    else
    {
        remove("temp.bin");
        return 0;
    }
}

int atualizar_cliente(const char *nome_arquivo, const char *cpf_alvo,
                      const char *novo_nome, const char *novo_sobrenome,
                      const char *novo_tel, const char *nova_cidade)
{
    FILE *f_in = fopen(nome_arquivo, "rb");
    if (f_in == NULL)
    {
        printf("Arquivo %s nao encontrado.\n", nome_arquivo);
        return 0;
    }

    FILE *f_temp = fopen("temp.bin", "wb");
    if (f_temp == NULL)
    {
        printf("Erro ao criar arquivo temporario.\n");
        fclose(f_in);
        return 0;
    }

    char reg_atual[TAM_REG_MAX];
    char cpf_atual[TAM_CAMPO_MAX];
    int atualizado = 0;

    while (pega_registro(f_in, reg_atual) > 0)
    {
        extrai_cpf(reg_atual, cpf_atual);
        if (strcmp(cpf_atual, cpf_alvo) == 0)
        {
            char reg_modificado[TAM_REG_MAX];
            monta_registro(reg_modificado, cpf_alvo, novo_nome, novo_sobrenome, novo_tel, nova_cidade);
            grava_registro(f_temp, reg_modificado);
            atualizado = 1;
        }
        else
        {
            grava_registro(f_temp, reg_atual);
        }
    }

    fclose(f_in);
    fclose(f_temp);

    if (atualizado)
    {
        remove(nome_arquivo);
        rename("temp.bin", nome_arquivo);
        return 1;
    }
    else
    {
        remove("temp.bin");
        return 0;
    }
}

/*
 * ============================================================================
 * RESPOSTA À PEGADINHA PROPOSITIAL DO PROFESSOR (Item 4)
 * ============================================================================
 * O que acontece se houver uma inserção ou remoção entre a criação do
 * índice e a busca?
 *
 * O índice se torna INVÁLIDO. Como os registros possuem tamanho variável,
 * adicionar ou remover um cliente desloca fisicamente os bytes de todos os
 * registros seguintes. O índice gerado é estritamente uma "FOTO" (snapshot)
 * do arquivo naquele exato instante. Se usarmos offsets desatualizados, o
 * fseek cairá no meio de campos de texto ou delimitadores, corrompendo a leitura.
 *
 * TRADE-OFF DAS DUAS ABORDAGENS:
 *
 * 1) Recalcular o índice a cada chamada de busca (Abordagem segura):
 *    - Vantagem: Consistência absoluta. Nunca acessaremos um offset inválido.
 *    - Desvantagem: Destrói o desempenho. Varrer o arquivo inteiro para
 *      montar o vetor custa O(N) operações de disco, o que anula por completo
 *      o ganho de velocidade (O(log N)) proporcionado pela busca binária.
 *
 * 2) Receber o índice já pronto como parâmetro (Manter em memória):
 *    - Vantagem: Alta performance real. Permite múltiplas consultas rápidas
 *      em tempo logarítmico (O(log N)).
 *    - Desvantagem: Complexidade de sincronização. Qualquer inserção, remoção
 *      ou atualização no arquivo obriga o sistema a recalcular e atualizar
 *      imediatamente o índice mantido na memória RAM para evitar falhas.
 * ============================================================================
 */

long *monta_indice_offsets(FILE *p_arq, int *p_total_registros)
{
    rewind(p_arq);

    int capacidade = 16;
    int total = 0;
    long *offsets = (long *)malloc(capacidade * sizeof(long));
    if (!offsets)
        return NULL;

    char reg_descarte[TAM_REG_MAX];
    long offset_atual = ftell(p_arq);

    while (pega_registro(p_arq, reg_descarte) > 0)
    {
        if (total >= capacidade)
        {
            capacidade *= 2;
            long *novo_ptr = (long *)realloc(offsets, capacidade * sizeof(long));
            if (!novo_ptr)
            {
                free(offsets);
                return NULL;
            }
            offsets = novo_ptr;
        }

        offsets[total++] = offset_atual;
        offset_atual = ftell(p_arq);
    }

    *p_total_registros = total;
    return offsets;
}

int busca_binaria_por_indice(FILE *p_arq, const long *offsets, int total_registros,
                             const char *cpf_busca, char *reg_resultado)
{
    int inicio = 0;
    int fim = total_registros - 1;
    char cpf_meio[TAM_CAMPO_MAX];

    while (inicio <= fim)
    {
        int meio = inicio + (fim - inicio) / 2;

        fseek(p_arq, offsets[meio], SEEK_SET);

        if (pega_registro(p_arq, reg_resultado) <= 0)
            return 0;

        extrai_cpf(reg_resultado, cpf_meio);
        int cmp = strcmp(cpf_busca, cpf_meio);

        if (cmp == 0)
        {
            return 1;
        }
        else if (cmp < 0)
        {
            fim = meio - 1;
        }
        else
        {
            inicio = meio + 1;
        }
    }

    return 0;
}

int merge_arquivos_ordenados(const char *arq1, const char *arq2, const char *arq_saida)
{
    FILE *f1 = fopen(arq1, "rb");
    FILE *f2 = fopen(arq2, "rb");

    if (!f1 && !f2)
    {
        printf("Nenhum dos arquivos de entrada foi encontrado.\n");
        return 0;
    }

    FILE *fout = fopen(arq_saida, "wb");
    if (!fout)
    {
        printf("Erro ao criar arquivo de saida %s.\n", arq_saida);
        if (f1)
            fclose(f1);
        if (f2)
            fclose(f2);
        return 0;
    }

    char reg1[TAM_REG_MAX], reg2[TAM_REG_MAX];
    char cpf1[TAM_CAMPO_MAX], cpf2[TAM_CAMPO_MAX];

    int tem1 = (f1 != NULL) ? (pega_registro(f1, reg1) > 0) : 0;
    int tem2 = (f2 != NULL) ? (pega_registro(f2, reg2) > 0) : 0;

    if (tem1)
        extrai_cpf(reg1, cpf1);
    if (tem2)
        extrai_cpf(reg2, cpf2);

    while (tem1 && tem2)
    {
        int cmp = strcmp(cpf1, cpf2);

        if (cmp < 0)
        {
            grava_registro(fout, reg1);
            tem1 = (pega_registro(f1, reg1) > 0);
            if (tem1)
                extrai_cpf(reg1, cpf1);
        }
        else if (cmp > 0)
        {
            grava_registro(fout, reg2);
            tem2 = (pega_registro(f2, reg2) > 0);
            if (tem2)
                extrai_cpf(reg2, cpf2);
        }
        else
        {
            grava_registro(fout, reg1);
            tem1 = (pega_registro(f1, reg1) > 0);
            if (tem1)
                extrai_cpf(reg1, cpf1);
            tem2 = (pega_registro(f2, reg2) > 0);
            if (tem2)
                extrai_cpf(reg2, cpf2);
        }
    }

    while (tem1)
    {
        grava_registro(fout, reg1);
        tem1 = (pega_registro(f1, reg1) > 0);
    }

    while (tem2)
    {
        grava_registro(fout, reg2);
        tem2 = (pega_registro(f2, reg2) > 0);
    }

    if (f1)
        fclose(f1);
    if (f2)
        fclose(f2);
    fclose(fout);

    return 1;
}

void listar_arquivo(const char *nome_arquivo)
{
    FILE *f = fopen(nome_arquivo, "rb");
    if (!f)
    {
        printf("Arquivo '%s' esta vazio ou nao existe.\n", nome_arquivo);
        return;
    }

    char reg[TAM_REG_MAX];
    int contador = 0;
    printf("\n--- Conteudo do arquivo: %s ---\n", nome_arquivo);

    while (pega_registro(f, reg) > 0)
    {
        contador++;
        printf("[%02d] ", contador);
        imprime_registro(reg);
    }

    if (contador == 0)
        printf("(Nenhum registro encontrado)\n");
    else
        printf("Total de registros: %d\n", contador);

    printf("----------------------------------------\n\n");
    fclose(f);
}

void limpa_linha(char *str)
{
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
    }
}

void le_string(const char *rotulo, char *buffer, int tamanho)
{
    printf("%s", rotulo);
    if (fgets(buffer, tamanho, stdin) != NULL)
    {
        limpa_linha(buffer);
    }
}

void pausa_enter(void)
{
    printf("\nPressione ENTER para continuar...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

int main(void)
{
    const char *NOME_ARQ = "clientes.bin";
    int opcao;

    do
    {
        printf("\n============================================\n");
        printf("SISTEMA DE ARQUIVOS DE REGISTROS VARIAVEIS\n");
        printf("============================================\n");
        printf("1. Inserir cliente ordenado por CPF (Requisito 1)\n");
        printf("2. Remover cliente fisicamente (Requisito 2)\n");
        printf("3. Atualizar dados de cliente (Requisito 3)\n");
        printf("4. Buscar cliente por CPF via Busca Binaria (Requisito 4)\n");
        printf("5. Intercalar com 'importados.bin' (Requisito 5)\n");
        printf("6. Listar todos os clientes de 'clientes.bin'\n");
        printf("7. Listar 'clientes_merged.bin'\n");
        printf("0. Sair\n");
        printf("Escolha uma opcao: ");

        if (scanf("%d", &opcao) != 1)
        {
            printf("Entrada invalida. Encerrando...\n");
            break;
        }

        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;

        switch (opcao)
        {
        case 1:
        {
            char cpf[TAM_CAMPO_MAX];
            char nome[TAM_CAMPO_MAX];
            char sobrenome[TAM_CAMPO_MAX];
            char telefone[TAM_CAMPO_MAX];
            char cidade[TAM_CAMPO_MAX];
            char novo_reg[TAM_REG_MAX];

            printf("\n--- Novo Cliente ---\n");
            le_string("CPF (apenas digitos ou formatado): ", cpf, sizeof(cpf));
            le_string("Nome: ", nome, sizeof(nome));
            le_string("Sobrenome: ", sobrenome, sizeof(sobrenome));
            le_string("Telefone: ", telefone, sizeof(telefone));
            le_string("Cidade: ", cidade, sizeof(cidade));

            monta_registro(novo_reg, cpf, nome, sobrenome, telefone, cidade);

            if (inserir_ordenado(NOME_ARQ, novo_reg))
            {
                printf("Cliente inserido com sucesso em ordem!\n");
            }
            pausa_enter();
            break;
        }

        case 2:
        {
            char cpf[TAM_CAMPO_MAX];
            printf("\n--- Remocao Fisica ---\n");
            le_string("Digite o CPF a remover: ", cpf, sizeof(cpf));

            if (remover_fisico(NOME_ARQ, cpf))
            {
                printf("Cliente com CPF %s removido fisicamente com sucesso!\n", cpf);
            }
            else
            {
                printf("CPF %s nao encontrado no arquivo.\n", cpf);
            }
            pausa_enter();
            break;
        }

        case 3:
        {
            char cpf[TAM_CAMPO_MAX];
            char nome[TAM_CAMPO_MAX];
            char sobrenome[TAM_CAMPO_MAX];
            char telefone[TAM_CAMPO_MAX];
            char cidade[TAM_CAMPO_MAX];

            printf("\n--- Atualizacao Preservando Ordem ---\n");
            le_string("Digite o CPF do cliente a atualizar: ", cpf, sizeof(cpf));

            FILE *f_check = fopen(NOME_ARQ, "rb");
            int total = 0;
            long *offsets = NULL;
            char reg_encontrado[TAM_REG_MAX];
            int achou = 0;

            if (f_check)
            {
                offsets = monta_indice_offsets(f_check, &total);
                if (offsets)
                {
                    achou = busca_binaria_por_indice(f_check, offsets, total, cpf, reg_encontrado);
                    free(offsets);
                }
                fclose(f_check);
            }

            if (!achou)
            {
                printf("CPF %s nao encontrado para atualizacao.\n", cpf);
                pausa_enter();
                break;
            }

            printf("Registro atual:\n");
            imprime_registro(reg_encontrado);
            printf("\nDigite os novos dados:\n");
            le_string("Novo Nome: ", nome, sizeof(nome));
            le_string("Novo Sobrenome: ", sobrenome, sizeof(sobrenome));
            le_string("Novo Telefone: ", telefone, sizeof(telefone));
            le_string("Nova Cidade: ", cidade, sizeof(cidade));

            if (atualizar_cliente(NOME_ARQ, cpf, nome, sobrenome, telefone, cidade))
            {
                printf("Cliente atualizado com sucesso!\n");
            }
            pausa_enter();
            break;
        }

        case 4:
        {
            char cpf[TAM_CAMPO_MAX];
            printf("\n--- Busca Binaria Adaptada por Indice ---\n");
            le_string("Digite o CPF para busca: ", cpf, sizeof(cpf));

            FILE *f = fopen(NOME_ARQ, "rb");
            if (!f)
            {
                printf("Arquivo %s inexistente ou vazio.\n", NOME_ARQ);
                pausa_enter();
                break;
            }

            int total_registros = 0;
            long *offsets = monta_indice_offsets(f, &total_registros);

            if (!offsets || total_registros == 0)
            {
                printf("Nao ha registros no arquivo.\n");
                if (offsets)
                    free(offsets);
                fclose(f);
                pausa_enter();
                break;
            }

            char reg_busca[TAM_REG_MAX];
            int encontrado = busca_binaria_por_indice(f, offsets, total_registros, cpf, reg_busca);

            if (encontrado)
            {
                printf("\n[SUCESSO] Cliente encontrado via Busca Binaria:\n");
                imprime_registro(reg_busca);
            }
            else
            {
                printf("\n[AVISO] Cliente com CPF %s nao foi encontrado.\n", cpf);
            }

            free(offsets);
            fclose(f);
            pausa_enter();
            break;
        }

        case 5:
        {
            printf("\n--- Merge de Arquivos Ordenados ---\n");
            printf("Intercalando 'clientes.bin' e 'importados.bin' -> 'clientes_merged.bin'...\n");

            if (merge_arquivos_ordenados(NOME_ARQ, "importados.bin", "clientes_merged.bin"))
            {
                printf("Merge concluido com sucesso em 'clientes_merged.bin'!\n");
                listar_arquivo("clientes_merged.bin");
            }
            pausa_enter();
            break;
        }

        case 6:
            listar_arquivo(NOME_ARQ);
            pausa_enter();
            break;

        case 7:
            listar_arquivo("clientes_merged.bin");
            pausa_enter();
            break;

        case 0:
            printf("Encerrando o programa. Bons estudos!\n");
            break;

        default:
            printf("Opcao invalida. Tente novamente.\n");
            pausa_enter();
            break;
        }

    } while (opcao != 0);

    return 0;
}