#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define BYTES_POR_LINHA 16

int main(void)
{
    char nome_arquivo[256];
    FILE *arq;
    unsigned char buffer[BYTES_POR_LINHA];
    int bytes_lidos;
    int i;

    printf("Digite o nome do arquivo para o hexdump: ");
    if (scanf("%255s", nome_arquivo) != 1)
    {
        printf("Erro ao ler o nome do arquivo.\n");
        return 1;
    }

    arq = fopen(nome_arquivo, "rb");
    if (arq == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return 1;
    }

    printf("\n");

    while ((bytes_lidos = fread(buffer, 1, BYTES_POR_LINHA, arq)) > 0)
    {

               for (i = 0; i < BYTES_POR_LINHA; i++)
        {
            if (i < bytes_lidos)
            {
                printf("%02X ", buffer[i]);
            }
            else
            {
                printf("   ");
            }
        }

        printf("  ");

        for (i = 0; i < bytes_lidos; i++)
        {
            if (isprint(buffer[i]))
            {
                printf("%c", buffer[i]);
            }
            else
            {
                printf(".");
            }
        }

        printf("\n");
    }

    if (fclose(arq) != 0)
    {
        perror("Erro ao fechar o arquivo");
        return 1;
    }

    return 0;
}