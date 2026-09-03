#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    char nome_arquivo[256] = "file.txt";
    int n;

    if (argc >= 3)
    {
        snprintf(nome_arquivo, sizeof(nome_arquivo), "%s", argv[1]);
        n = atoi(argv[2]);
    }
    else
    {
        printf("Digite o nome do arquivo: ");
        if (scanf("%255s", nome_arquivo) != 1)
        {
            printf("Erro ao ler o nome do arquivo.\n");
            return 1;
        }

        printf("Digite a quantidade de ultimas linhas (n): ");
        if (scanf("%d", &n) != 1 || n <= 0)
        {
            printf("Valor invalido para n.\n");
            return 1;
        }
    }

    FILE *arq = fopen(nome_arquivo, "rb");
    if (arq == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return 1;
    }

    if (fseek(arq, 0, SEEK_END) != 0)
    {
        perror("Erro ao posicionar ponteiro no final do arquivo");
        fclose(arq);
        return 1;
    }

    long tamanho = ftell(arq);
    if (tamanho == 0)
    {
        fclose(arq);
        return 0;
    }

    long pos = tamanho - 1;
    int linhas_encontradas = 0;

    fseek(arq, pos, SEEK_SET);
    if (fgetc(arq) == '\n')
    {
        pos--;
    }

    while (pos >= 0 && linhas_encontradas < n)
    {
        fseek(arq, pos, SEEK_SET);
        int ch = fgetc(arq);

        if (ch == '\n')
        {
            linhas_encontradas++;
            if (linhas_encontradas == n)
            {
                break;
            }
        }
        pos--;
    }

    if (pos < 0)
    {
        fseek(arq, 0, SEEK_SET);
    }

    int ch;
    while ((ch = fgetc(arq)) != EOF)
    {
        putchar(ch);
    }

    if (fclose(arq) != 0)
    {
        perror("Erro ao fechar o arquivo");
        return 1;
    }

    return 0;
}