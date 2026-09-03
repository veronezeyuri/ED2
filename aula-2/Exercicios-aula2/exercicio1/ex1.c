#include <stdio.h>
#include <stdlib.h>

#define ESC 0x1B

int main()
{
    FILE *file = fopen("file.txt", "w");
    if (file == NULL)
    {
        printf("ERROR opening file");
        return 1;
    }

    printf("Digite o que deseja salvar no arquivo. Pressione ESC depois ENTER para salvar e sair\n");

    int ch;
    while ((ch = getchar()) != EOF && ch != ESC)
    {
        if (fputc(ch, file) == EOF)
        {
            printf("error writing to file");
            fclose(file);
            return 2;
        }
    }

    if (fclose(file) != 0)
    {
        printf("Error closing file");
        return 3;
    }

    printf("Dados salvos com sucesso em 'file.txt'\n");
}