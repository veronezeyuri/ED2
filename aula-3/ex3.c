#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO_PRINCIPAL "clientes.bin"
#define ARQUIVO_TEMP "clientes_tmp.bin"
#define SEP '|'
#define TAM_CAMPO 128
#define TAM_REG 640
#define MAX_REGISTROS 4096

typedef struct
{
    char cpf[TAM_CAMPO];
    char nome[TAM_CAMPO];
    char sobrenome[TAM_CAMPO];
    char telefone[TAM_CAMPO];
    char cidade[TAM_CAMPO];
} Cliente;

static int pega_campo(const char *buf, int *pos, char *dest)
{
    int i = 0;
    while (buf[*pos] != '\0' && buf[*pos] != SEP)
    {
        dest[i++] = buf[(*pos)++];
    }
    dest[i] = '\0';
    if (buf[*pos] == SEP)
        (*pos)++;
    return i > 0;
}

static int pega_registro(FILE *f, Cliente *c)
{
    char buf[TAM_REG];
    if (!fgets(buf, sizeof(buf), f))
        return 0;

    int pos = 0;
    pega_campo(buf, &pos, c->cpf);
    pega_campo(buf, &pos, c->nome);
    pega_campo(buf, &pos, c->sobrenome);
    pega_campo(buf, &pos, c->telefone);
    pega_campo(buf, &pos, c->cidade);
    return 1;
}

static void grava_registro(FILE *f, const Cliente *c)
{
    fprintf(f, "%s%c%s%c%s%c%s%c%s%c\n",
            c->cpf, SEP,
            c->nome, SEP,
            c->sobrenome, SEP,
            c->telefone, SEP,
            c->cidade, SEP);
}

static int inserir(const char *arquivo, const Cliente *novo)
{
    FILE *orig = fopen(arquivo, "rb");
    FILE *tmp = fopen(ARQUIVO_TEMP, "wb");
    if (!tmp)
    {
        perror("fopen tmp");
        if (orig)
            fclose(orig);
        return -1;
    }

    int inserido = 0;
    Cliente atual;

    if (orig)
    {
        while (pega_registro(orig, &atual))
        {
            int cmp = strcmp(atual.cpf, novo->cpf);
            if (cmp == 0)
            {
                fprintf(stderr, "CPF %s ja existe.\n", novo->cpf);
                grava_registro(tmp, &atual);
                inserido = 1;
                while (pega_registro(orig, &atual))
                    grava_registro(tmp, &atual);
                goto fim;
            }
            if (!inserido && cmp > 0)
            {
                grava_registro(tmp, novo);
                inserido = 1;
            }
            grava_registro(tmp, &atual);
        }
        fclose(orig);
        orig = NULL;
    }

    if (!inserido)
        grava_registro(tmp, novo);

fim:
    if (orig)
        fclose(orig);
    fclose(tmp);

    if (rename(ARQUIVO_TEMP, arquivo) != 0)
    {
        perror("rename");
        return -1;
    }
    return 0;
}

static int remover(const char *arquivo, const char *cpf)
{
    FILE *orig = fopen(arquivo, "rb");
    if (!orig)
    {
        perror("fopen");
        return -1;
    }

    FILE *tmp = fopen(ARQUIVO_TEMP, "wb");
    if (!tmp)
    {
        perror("fopen tmp");
        fclose(orig);
        return -1;
    }

    Cliente c;
    int encontrado = 0;
    while (pega_registro(orig, &c))
    {
        if (strcmp(c.cpf, cpf) == 0)
        {
            encontrado = 1;
        }
        else
        {
            grava_registro(tmp, &c);
        }
    }

    fclose(orig);
    fclose(tmp);

    if (!encontrado)
    {
        fprintf(stderr, "CPF %s nao encontrado.\n", cpf);
        remove(ARQUIVO_TEMP);
        return 1;
    }

    if (rename(ARQUIVO_TEMP, arquivo) != 0)
    {
        perror("rename");
        return -1;
    }
    return 0;
}

static int atualizar(const char *arquivo, const Cliente *novo)
{
    FILE *orig = fopen(arquivo, "rb");
    if (!orig)
    {
        perror("fopen");
        return -1;
    }

    FILE *tmp = fopen(ARQUIVO_TEMP, "wb");
    if (!tmp)
    {
        perror("fopen tmp");
        fclose(orig);
        return -1;
    }

    Cliente c;
    int encontrado = 0;
    while (pega_registro(orig, &c))
    {
        if (strcmp(c.cpf, novo->cpf) == 0)
        {
            encontrado = 1;
            grava_registro(tmp, novo);
        }
        else
        {
            grava_registro(tmp, &c);
        }
    }

    fclose(orig);
    fclose(tmp);

    if (!encontrado)
    {
        fprintf(stderr, "CPF %s nao encontrado.\n", novo->cpf);
        remove(ARQUIVO_TEMP);
        return 1;
    }

    if (rename(ARQUIVO_TEMP, arquivo) != 0)
    {
        perror("rename");
        return -1;
    }
    return 0;
}

/*
 * NOTA SOBRE VALIDADE DO INDICE:
 *   O array de offsets e construido percorrendo o arquivo uma unica vez
 *   e representa uma "foto" do estado do arquivo naquele instante.
 *   Se entre a construcao do indice e a busca alguem inserir ou remover
 *   um registro (alterando os offsets), o indice fica desatualizado e
 *   a busca produzira resultados incorretos.
 *
 *   Trade-off de design:
 *     a) Recalcular o indice dentro de cada chamada de busca garante
 *        consistencia, mas custa O(n) extra para cada busca.
 *     b) Receber o indice como parametro permite reutiliza-lo em buscas
 *        multiplas numa mesma sessao (mais eficiente), mas exige que o
 *        chamador saiba quando o indice ficou obsoleto.
 *
 *   Escolha aqui: recalcular o indice a cada chamada (opcao a),
 *   garantindo que a funcao seja sempre correta de forma autonoma.
 */
static int buscar(const char *arquivo, const char *cpf, Cliente *resultado)
{
    FILE *f = fopen(arquivo, "rb");
    if (!f)
    {
        perror("fopen");
        return -1;
    }

    long offsets[MAX_REGISTROS];
    int n = 0;

    char buf[TAM_REG];
    long off = ftell(f);
    while (n < MAX_REGISTROS && fgets(buf, sizeof(buf), f))
    {
        offsets[n++] = off;
        off = ftell(f);
    }

    int lo = 0, hi = n - 1, encontrado = 0;
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        fseek(f, offsets[mid], SEEK_SET);
        Cliente c;
        if (!pega_registro(f, &c))
            break;

        int cmp = strcmp(c.cpf, cpf);
        if (cmp == 0)
        {
            if (resultado)
                *resultado = c;
            encontrado = 1;
            break;
        }
        else if (cmp < 0)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    fclose(f);
    return encontrado;
}

static int merge(const char *arq1, const char *arq2, const char *saida)
{
    FILE *f1 = fopen(arq1, "rb");
    FILE *f2 = fopen(arq2, "rb");
    FILE *out = fopen(saida, "wb");

    if (!f1)
    {
        perror(arq1);
        if (f2)
            fclose(f2);
        if (out)
            fclose(out);
        return -1;
    }
    if (!f2)
    {
        perror(arq2);
        fclose(f1);
        if (out)
            fclose(out);
        return -1;
    }
    if (!out)
    {
        perror(saida);
        fclose(f1);
        fclose(f2);
        return -1;
    }

    Cliente c1, c2;
    int tem1 = pega_registro(f1, &c1);
    int tem2 = pega_registro(f2, &c2);

    while (tem1 && tem2)
    {
        int cmp = strcmp(c1.cpf, c2.cpf);
        if (cmp < 0)
        {
            grava_registro(out, &c1);
            tem1 = pega_registro(f1, &c1);
        }
        else if (cmp > 0)
        {
            grava_registro(out, &c2);
            tem2 = pega_registro(f2, &c2);
        }
        else
        {
            grava_registro(out, &c1);
            tem1 = pega_registro(f1, &c1);
            tem2 = pega_registro(f2, &c2);
        }
    }

    while (tem1)
    {
        grava_registro(out, &c1);
        tem1 = pega_registro(f1, &c1);
    }
    while (tem2)
    {
        grava_registro(out, &c2);
        tem2 = pega_registro(f2, &c2);
    }

    fclose(f1);
    fclose(f2);
    fclose(out);
    return 0;
}

static void listar(const char *arquivo)
{
    FILE *f = fopen(arquivo, "rb");
    if (!f)
    {
        perror("fopen");
        return;
    }

    Cliente c;
    int i = 0;
    while (pega_registro(f, &c))
    {
        printf("[%d] CPF=%-14s Nome=%-20s %s Tel=%-15s Cidade=%s\n",
               ++i, c.cpf, c.nome, c.sobrenome, c.telefone, c.cidade);
    }
    if (i == 0)
        printf("Arquivo vazio.\n");
    fclose(f);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr,
                "Uso:\n"
                "  %s inserir   <cpf> <nome> <sobrenome> <telefone> <cidade>\n"
                "  %s remover   <cpf>\n"
                "  %s atualizar <cpf> <nome> <sobrenome> <telefone> <cidade>\n"
                "  %s buscar    <cpf>\n"
                "  %s listar\n"
                "  %s merge     <importados.bin> <saida.bin>\n",
                argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "inserir") == 0 || strcmp(cmd, "atualizar") == 0)
    {
        if (argc != 7)
        {
            fprintf(stderr, "Uso: %s %s <cpf> <nome> <sobrenome> <telefone> <cidade>\n",
                    argv[0], cmd);
            return 1;
        }
        Cliente c;
        strncpy(c.cpf, argv[2], TAM_CAMPO - 1);
        c.cpf[TAM_CAMPO - 1] = '\0';
        strncpy(c.nome, argv[3], TAM_CAMPO - 1);
        c.nome[TAM_CAMPO - 1] = '\0';
        strncpy(c.sobrenome, argv[4], TAM_CAMPO - 1);
        c.sobrenome[TAM_CAMPO - 1] = '\0';
        strncpy(c.telefone, argv[5], TAM_CAMPO - 1);
        c.telefone[TAM_CAMPO - 1] = '\0';
        strncpy(c.cidade, argv[6], TAM_CAMPO - 1);
        c.cidade[TAM_CAMPO - 1] = '\0';

        int ret = (strcmp(cmd, "inserir") == 0)
                      ? inserir(ARQUIVO_PRINCIPAL, &c)
                      : atualizar(ARQUIVO_PRINCIPAL, &c);
        if (ret == 0)
            printf("Operacao '%s' realizada com sucesso.\n", cmd);
        return ret < 0 ? 1 : ret;
    }
    else if (strcmp(cmd, "remover") == 0)
    {
        if (argc != 3)
        {
            fprintf(stderr, "Uso: %s remover <cpf>\n", argv[0]);
            return 1;
        }
        int ret = remover(ARQUIVO_PRINCIPAL, argv[2]);
        if (ret == 0)
            printf("Registro removido com sucesso.\n");
        return ret < 0 ? 1 : ret;
    }
    else if (strcmp(cmd, "buscar") == 0)
    {
        if (argc != 3)
        {
            fprintf(stderr, "Uso: %s buscar <cpf>\n", argv[0]);
            return 1;
        }
        Cliente c;
        int r = buscar(ARQUIVO_PRINCIPAL, argv[2], &c);
        if (r == 1)
        {
            printf("Encontrado:\n");
            printf("  CPF       : %s\n", c.cpf);
            printf("  Nome      : %s %s\n", c.nome, c.sobrenome);
            printf("  Telefone  : %s\n", c.telefone);
            printf("  Cidade    : %s\n", c.cidade);
        }
        else if (r == 0)
        {
            printf("CPF %s nao encontrado.\n", argv[2]);
        }
        return r < 0 ? 1 : 0;
    }
    else if (strcmp(cmd, "listar") == 0)
    {
        listar(ARQUIVO_PRINCIPAL);
        return 0;
    }
    else if (strcmp(cmd, "merge") == 0)
    {
        if (argc != 4)
        {
            fprintf(stderr, "Uso: %s merge <importados.bin> <saida.bin>\n", argv[0]);
            return 1;
        }
        int ret = merge(ARQUIVO_PRINCIPAL, argv[2], argv[3]);
        if (ret == 0)
            printf("Merge concluido em '%s'.\n", argv[3]);
        return ret < 0 ? 1 : 0;
    }
    else
    {
        fprintf(stderr, "Comando desconhecido: %s\n", cmd);
        return 1;
    }
}
