/**
 * ===========================================================================
 *  ex_comentado.c — Sistema de Catálogo de Streaming Musical
 * ===========================================================================
 *
 *  VISÃO GERAL DO EXERCÍCIO
 *  ~~~~~~~~~~~~~~~~~~~~~~~~
 *  Este programa gerencia um catálogo de faixas musicais de uma plataforma
 *  de streaming. As gravadoras fornecem: código da faixa, nome, artista e
 *  gênero musical. Tudo é armazenado em um ARQUIVO BINÁRIO em disco
 *  (memória secundária), nunca em vetores na RAM para dados permanentes.
 *
 *  FORMATO DO ARQUIVO (dados.bin)
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  O arquivo usa REGISTROS DE TAMANHO VARIÁVEL. Cada registro tem:
 *
 *    [4 bytes: tamanho do payload][payload de tamanho variável]
 *
 *  O payload é uma string no formato: "CÓDIGO|NOME|ARTISTA|GÊNERO\0"
 *  Exemplo: "7042|Águas de Março|Elis Regina|MPB\0"
 *
 *  O PRIMEIRO registro do arquivo é especial — é o CABEÇALHO:
 *    [4 bytes: offset do primeiro espaço livre na lista ligada]
 *  Se não há espaços livres, esse valor é -1.
 *
 *  Visualmente, o arquivo fica assim:
 *
 *    Byte 0        Byte 4                    Byte 4+4+tam1          ...
 *    ┌──────────┬──────────┬────────────┬──────────┬────────────┬──────
 *    │ HEAD=-1  │ tam1=26  │ payload1   │ tam2=30  │ payload2   │ ...
 *    └──────────┴──────────┴────────────┴──────────┴────────────┴──────
 *    cabeçalho   registro 1              registro 2
 *
 *  CONCEITOS-CHAVE IMPLEMENTADOS
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  1. REGISTROS DE TAMANHO VARIÁVEL — cada registro ocupa apenas o espaço
 *     que seus dados precisam (diferente de registros fixos).
 *
 *  2. LISTA LIGADA DE ESPAÇOS LIVRES — quando um registro é removido, seu
 *     espaço não é desperdiçado. Ele entra em uma lista encadeada DENTRO
 *     do próprio arquivo, permitindo reaproveitamento.
 *
 *  3. BEST-FIT — ao inserir, percorre TODA a lista de espaços livres e
 *     escolhe o MENOR espaço que comporta o novo registro. Isso minimiza
 *     a fragmentação (desperdício de espaço).
 *
 *  4. FRAGMENTAÇÃO INTERNA — se um espaço livre é maior que o necessário,
 *     o registro é inserido ali mesmo, mas o espaço "sobra" é desperdiçado
 *     dentro do registro (fragmentação interna). O campo de tamanho mantém
 *     o valor original (maior) para que a travessia do arquivo funcione.
 *
 *  5. COMPACTAÇÃO — reconstrói o arquivo do zero, copiando apenas os
 *     registros válidos e eliminando TODA fragmentação (interna e externa).
 *
 *  O QUE É O ARQUIVO "ex" (binário compilado)?
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  O arquivo "ex" (sem extensão) é o EXECUTÁVEL gerado ao compilar este
 *  código-fonte. Ele é gerado com:
 *      gcc -o ex ex.c
 *  E executado com:
 *      ./ex
 *  "ex" é o programa em linguagem de máquina — não é legível por humanos.
 *  O código-fonte (.c) é o que você edita; o executável é o que você roda.
 *
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 *  INCLUDES — Bibliotecas padrão do C
 * ---------------------------------------------------------------------------
 *  stdio.h  → Funções de entrada/saída: printf, scanf, fopen, fread, fwrite,
 *             fseek, ftell, fclose, remove, rename
 *  stdlib.h → Gerenciamento de memória: malloc, free; e funções auxiliares
 *  string.h → Manipulação de strings: strlen, strcmp, strncmp, sprintf, memcpy
 * ---------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 *  CONSTANTES (defines)
 * ---------------------------------------------------------------------------
 *  Usamos #define para dar nomes significativos a valores fixos.
 *  Isso facilita a manutenção: se o nome do arquivo mudar, alteramos
 *  em um único lugar.
 * ---------------------------------------------------------------------------
 */
#define ARQUIVO_DADOS  "dados.bin"   /* Arquivo principal com os registros       */
#define ARQUIVO_INSERE "insere.bin"  /* Arquivo de entrada com faixas a inserir  */
#define ARQUIVO_REMOVE "remove.bin"  /* Arquivo de entrada com códigos a remover */
#define ARQUIVO_ESTADO "estado.bin"  /* Salva posição atual nos vetores de teste */
#define FIM_LISTA      -1           /* Marcador de fim da lista ligada          */

/* ---------------------------------------------------------------------------
 *  STRUCT Faixa — Estrutura que representa uma faixa musical
 * ---------------------------------------------------------------------------
 *  Uma struct agrupa vários dados relacionados em um único "pacote".
 *  Aqui, cada Faixa tem 4 campos, dimensionados conforme a especificação:
 *
 *  Campo      Tamanho máximo       No struct (com \0)
 *  ─────      ──────────────       ──────────────────
 *  cod        4 caracteres (fixo)  char[5]   → 4 chars + terminador '\0'
 *  nome       60 caracteres        char[61]  → 60 chars + '\0'
 *  artista    50 caracteres        char[51]  → 50 chars + '\0'
 *  genero     20 caracteres        char[21]  → 20 chars + '\0'
 *
 *  IMPORTANTE: Em C, toda string termina com '\0' (byte nulo), por isso
 *  precisamos de 1 byte extra em cada campo.
 *
 *  sizeof(Faixa) = 5 + 61 + 51 + 21 = 138 bytes
 *  Esse é o tamanho de cada elemento nos arquivos insere.bin/remove.bin.
 * ---------------------------------------------------------------------------
 */
typedef struct
{
    char cod[5];      /* Código da faixa: exatamente 4 dígitos + '\0'       */
    char nome[61];    /* Nome da faixa: até 60 caracteres + '\0'            */
    char artista[51]; /* Nome do artista: até 50 caracteres + '\0'          */
    char genero[21];  /* Gênero musical: até 20 caracteres + '\0'           */
} Faixa;

/* ---------------------------------------------------------------------------
 *  VARIÁVEIS GLOBAIS — Dados carregados em memória dos arquivos de teste
 * ---------------------------------------------------------------------------
 *  Variáveis globais ficam acessíveis em TODAS as funções do programa.
 *
 *  faixas_insere → Ponteiro para o vetor de Faixas lido de insere.bin
 *                  (alocado dinamicamente com malloc)
 *  total_insere  → Quantas faixas existem no vetor
 *  pos_insere    → Quantas já foram inseridas (índice do próximo a inserir)
 *
 *  codigos_remove → Ponteiro para o vetor de códigos lido de remove.bin
 *                   Declaração especial: "ponteiro para arrays de 5 chars"
 *  total_remove   → Quantos códigos existem no vetor
 *  pos_remove     → Quantos já foram removidos
 *
 *  POR QUE salvar pos_insere/pos_remove?
 *  Porque o programa pode ser encerrado e reaberto. Ao reabrir, ele precisa
 *  saber onde parou para não re-inserir/re-remover dados já processados.
 * ---------------------------------------------------------------------------
 */
Faixa *faixas_insere = NULL;      /* NULL = ainda não foi carregado           */
int total_insere = 0;
int pos_insere = 0;               /* Próxima faixa a ser inserida             */

char (*codigos_remove)[5] = NULL; /* Ponteiro para vetor de strings de 5 bytes*/
int total_remove = 0;
int pos_remove = 0;               /* Próximo código a ser removido            */


/* ===========================================================================
 *  FUNÇÃO: inicializar_arquivo()
 * ===========================================================================
 *  OBJETIVO: Criar o arquivo de dados com o cabeçalho, MAS APENAS se ele
 *            ainda não existir. Se já existir, não faz nada.
 *
 *  POR QUE?
 *  A especificação diz: "Não criar o arquivo toda vez que o programa for
 *  aberto (fazer verificação)." Isso porque se o programa for reaberto,
 *  os dados anteriores devem ser preservados.
 *
 *  COMO FUNCIONA:
 *  1. Tenta abrir o arquivo em modo leitura binária ("rb")
 *  2. Se fopen retorna NULL → arquivo não existe → cria com cabeçalho
 *  3. Se fopen retorna um ponteiro → arquivo já existe → apenas fecha
 *
 *  O CABEÇALHO é um único inteiro (4 bytes) no início do arquivo,
 *  contendo o offset (posição em bytes) do primeiro espaço livre.
 *  Inicialmente é -1 (FIM_LISTA) porque não há espaços livres.
 *
 *  LAYOUT APÓS INICIALIZAÇÃO:
 *    Byte 0   Byte 3
 *    ┌────────────┐
 *    │ HEAD = -1  │   ← 4 bytes, valor -1 (sem espaços livres)
 *    └────────────┘
 *    Arquivo total: 4 bytes
 * ===========================================================================
 */
void inicializar_arquivo()
{
    /*
     * fopen("dados.bin", "rb"):
     *   "r" = read (leitura)
     *   "b" = binary (binário, não texto)
     * Retorna NULL se o arquivo não existe.
     */
    FILE *f = fopen(ARQUIVO_DADOS, "rb");

    if (!f)  /* !f equivale a (f == NULL) → arquivo não existe */
    {
        /*
         * Cria o arquivo em modo escrita binária ("wb").
         * "w" = write → se existir, sobrescreve (mas já sabemos que não existe)
         * "b" = binary
         */
        f = fopen(ARQUIVO_DADOS, "wb");

        int head = FIM_LISTA;  /* head = -1 → lista de espaços vazia */

        /*
         * fwrite(&head, sizeof(int), 1, f):
         *   &head       → endereço da variável a escrever
         *   sizeof(int) → tamanho de cada elemento (4 bytes)
         *   1           → quantidade de elementos
         *   f           → arquivo destino
         * Resultado: escreve os 4 bytes do inteiro -1 no início do arquivo
         */
        fwrite(&head, sizeof(int), 1, f);
        fclose(f);
    }
    else
    {
        /* Arquivo já existe → apenas fecha, preservando os dados */
        fclose(f);
    }
}


/* ===========================================================================
 *  FUNÇÃO: inserir(Faixa fx)
 * ===========================================================================
 *  OBJETIVO: Inserir uma nova faixa no arquivo de dados.
 *
 *  ESTRATÉGIA: BEST-FIT (melhor encaixe)
 *  ~~~~~~~~~~~
 *  Antes de inserir no final, o algoritmo percorre TODA a lista de espaços
 *  livres (espaços deixados por registros removidos) procurando o MENOR
 *  espaço que ainda assim comporte o novo registro.
 *
 *  POR QUE BEST-FIT e não FIRST-FIT?
 *  - First-Fit: pega o primeiro espaço que cabe → rápido, mas desperdiça mais
 *  - Best-Fit: pega o que melhor encaixa → mais lento, mas desperdiça menos
 *  A especificação exige Best-Fit.
 *
 *  FRAGMENTAÇÃO INTERNA:
 *  Se o espaço livre tem 50 bytes e o novo registro precisa de 30, os 20
 *  bytes restantes ficam inutilizados DENTRO do registro. Isso é a
 *  "fragmentação interna", que a especificação permite.
 *  O campo de tamanho NÃO é atualizado (continua 50), garantindo que a
 *  travessia do arquivo funcione corretamente (salta 50 bytes para
 *  chegar ao próximo registro).
 *
 *  FLUXO DA FUNÇÃO:
 *  1. Monta o payload: "cod|nome|artista|genero\0"
 *  2. Lê o cabeçalho (head) para saber onde começa a lista de livres
 *  3. Percorre a lista procurando o best-fit
 *  4. Se encontrou → remove da lista e escreve ali
 *     Se não encontrou → escreve no final do arquivo
 * ===========================================================================
 */
void inserir(Faixa fx)
{
    /*
     * "r+b" = leitura E escrita em binário.
     * O "+" significa que podemos tanto ler quanto escrever.
     * Diferente de "wb" que apagaria o conteúdo existente.
     * IMPORTANTE: "r+b" falha se o arquivo não existir (por isso chamamos
     * inicializar_arquivo() antes).
     */
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");

    /*
     * MONTAGEM DO PAYLOAD
     * ~~~~~~~~~~~~~~~~~~~
     * sprintf funciona como printf, mas escreve em uma string (buffer)
     * em vez de na tela. O formato é: "cod|nome|artista|genero"
     *
     * Buffer de 150 bytes é suficiente porque o tamanho máximo possível é:
     *   4 (cod) + 1 (|) + 60 (nome) + 1 (|) + 50 (artista) + 1 (|) +
     *   20 (genero) + 1 (\0) = 138 bytes
     */
    char payload[150];
    sprintf(payload, "%s|%s|%s|%s", fx.cod, fx.nome, fx.artista, fx.genero);

    /*
     * req_size = tamanho necessário para armazenar o payload.
     * strlen retorna o comprimento SEM contar o '\0', então somamos 1.
     * O '\0' precisa ser salvo porque ao ler de volta usamos funções de
     * string (strlen, strcmp) que dependem dele para saber onde a string acaba.
     */
    int req_size = strlen(payload) + 1;

    /* --------------- LÊ O CABEÇALHO (head da lista de livres) --------------- */

    int head;
    /*
     * fseek(f, 0, SEEK_SET): posiciona o cursor no INÍCIO do arquivo.
     *   0         → offset de 0 bytes
     *   SEEK_SET  → a partir do início
     */
    fseek(f, 0, SEEK_SET);
    fread(&head, sizeof(int), 1, f);

    /* --------------- BUSCA BEST-FIT NA LISTA DE ESPAÇOS LIVRES --------------- */

    /*
     * Variáveis para rastrear o melhor encaixe:
     *   best_off   → offset do melhor espaço encontrado (-1 = nenhum)
     *   prev_best  → offset do nó ANTERIOR ao melhor (para manipular a lista)
     *   min_diff   → menor diferença (espaço_livre - tamanho_necessário)
     *   curr_off   → offset do nó atual sendo visitado
     *   prev_off   → offset do nó anterior ao atual
     *
     * POR QUE precisamos do "anterior" (prev)?
     * Porque a lista é simplesmente encadeada — para remover um nó do meio,
     * precisamos atualizar o ponteiro "próximo" do nó anterior.
     */
    int best_off = -1, prev_best = -1, min_diff = 9999999;
    int curr_off = head, prev_off = -1;

    /*
     * PERCORRE A LISTA DE ESPAÇOS LIVRES
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * A lista está DENTRO do arquivo de dados. Cada nó da lista é um
     * registro removido com este formato:
     *
     *   [4 bytes: tamanho original] [1 byte: '@'] [4 bytes: próximo offset]
     *
     * O '@' é um marcador que indica "este registro foi removido".
     * O "próximo offset" aponta para o próximo espaço livre (ou -1 se fim).
     *
     * Exemplo visual da lista no arquivo:
     *
     *   head=44
     *      ↓
     *   [off=44: tam=34 | '@' | prox=130] → [off=130: tam=28 | '@' | prox=-1]
     *
     * A travessia termina quando curr_off == FIM_LISTA (-1).
     */
    while (curr_off != FIM_LISTA)
    {
        int size, next;
        char m;  /* marcador '@' */

        /* Posiciona no nó atual e lê seus campos */
        fseek(f, curr_off, SEEK_SET);
        fread(&size, sizeof(int), 1, f);   /* tamanho do espaço disponível   */
        fread(&m, sizeof(char), 1, f);     /* marcador '@'                   */
        fread(&next, sizeof(int), 1, f);   /* offset do próximo nó da lista  */

        /*
         * LÓGICA DO BEST-FIT:
         * 1. O espaço cabe? (size >= req_size)
         * 2. É o melhor até agora? (sobra é menor que a menor já vista)
         *
         * Se size == req_size → encaixe perfeito (diff = 0), melhor caso!
         */
        if (size >= req_size)
        {
            if (size - req_size < min_diff)
            {
                min_diff = size - req_size;
                best_off = curr_off;    /* salva offset do melhor espaço */
                prev_best = prev_off;   /* salva quem vem antes dele    */
            }
        }

        /* Avança para o próximo nó da lista */
        prev_off = curr_off;
        curr_off = next;
    }

    /* --------------- DECISÃO: REUSAR ESPAÇO OU INSERIR NO FINAL --------------- */

    if (best_off != -1)
    {
        /*
         * CASO 1: ENCONTROU UM ESPAÇO LIVRE ADEQUADO
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * Precisamos:
         * (a) Remover este nó da lista de espaços livres
         * (b) Escrever o novo payload neste espaço
         */

        /* (a) Lê o ponteiro "próximo" do nó que será removido da lista */
        int next_best;
        fseek(f, best_off + sizeof(int) + sizeof(char), SEEK_SET);
        fread(&next_best, sizeof(int), 1, f);

        /*
         * REMOÇÃO DE NÓ DA LISTA ENCADEADA
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * Para remover um nó, fazemos o anterior apontar para o próximo,
         * "pulando" o nó removido:
         *
         *   ANTES:  ... → [prev] → [best] → [next] → ...
         *   DEPOIS: ... → [prev] ─────────→ [next] → ...
         *
         * Caso especial: se prev_best == -1, o nó removido é o PRIMEIRO
         * da lista (HEAD), então atualizamos o cabeçalho do arquivo.
         */
        if (prev_best == -1)
        {
            /* best é o HEAD → atualiza o cabeçalho para pular este nó */
            fseek(f, 0, SEEK_SET);
            fwrite(&next_best, sizeof(int), 1, f);
        }
        else
        {
            /*
             * best está no meio/fim → atualiza o "próximo" do nó anterior
             * Posição do campo "próximo" em um nó removido:
             *   offset do nó + sizeof(int) + sizeof(char)
             *   ou seja: pula o tamanho (4) e o marcador '@' (1)
             */
            fseek(f, prev_best + sizeof(int) + sizeof(char), SEEK_SET);
            fwrite(&next_best, sizeof(int), 1, f);
        }

        /*
         * (b) ESCREVE O PAYLOAD NO ESPAÇO REAPROVEITADO
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * Sobrescreve a partir de best_off + sizeof(int), ou seja,
         * APÓS o campo de tamanho (que NÃO é atualizado — isso é a
         * fragmentação interna).
         *
         * O campo de tamanho original é preservado porque:
         * - A travessia do arquivo usa esse valor para saber quantos
         *   bytes pular até o próximo registro
         * - Se atualizássemos para o tamanho menor, os bytes restantes
         *   ficariam "perdidos" entre registros
         *
         * Na COMPACTAÇÃO, o tamanho será recalculado corretamente.
         */
        fseek(f, best_off + sizeof(int), SEEK_SET);
        fwrite(payload, sizeof(char), req_size, f);
    }
    else
    {
        /*
         * CASO 2: NENHUM ESPAÇO LIVRE SERVE → INSERE NO FINAL
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         * fseek(f, 0, SEEK_END) posiciona o cursor no FIM do arquivo.
         * Escrevemos: [tamanho][payload]
         *
         * SEEK_END → a partir do final do arquivo
         */
        fseek(f, 0, SEEK_END);
        fwrite(&req_size, sizeof(int), 1, f);      /* campo de tamanho */
        fwrite(payload, sizeof(char), req_size, f); /* dados do registro */
    }

    fclose(f);
}


/* ===========================================================================
 *  FUNÇÃO: remover(const char *cod_alvo)
 * ===========================================================================
 *  OBJETIVO: Remover uma faixa pelo código, diretamente no arquivo.
 *
 *  A remoção NÃO apaga dados fisicamente. Ela MARCA o registro como
 *  removido usando o caractere '@' e adiciona o espaço na lista ligada
 *  de espaços disponíveis.
 *
 *  POR QUE NÃO APAGAR DE VERDADE?
 *  Porque para "apagar" em um arquivo, teríamos que reescrever TODOS
 *  os registros seguintes (deslocá-los para fechar o "buraco").
 *  Isso é extremamente custoso. A solução é marcar como removido e
 *  reaproveitar o espaço depois.
 *
 *  FORMATO DE UM REGISTRO REMOVIDO:
 *    [4 bytes: tamanho original] [1 byte: '@'] [4 bytes: offset do próximo]
 *
 *  O '@' indica "este espaço está disponível para reuso".
 *  O "offset do próximo" forma a lista ligada de espaços livres.
 *
 *  REGRA IMPORTANTE: novos espaços são adicionados SEMPRE NO FINAL
 *  da lista (não no início). Isso significa que precisamos percorrer
 *  toda a lista até achar o último nó (tail).
 *
 *  FLUXO:
 *  1. Percorre todos os registros do arquivo sequencialmente
 *  2. Para cada registro válido (não removido), compara o código
 *  3. Se encontrar, marca com '@' e adiciona à lista de livres
 *  4. Se não encontrar, simplesmente não faz nada
 * ===========================================================================
 */
void remover(const char *cod_alvo)
{
    FILE *f = fopen(ARQUIVO_DADOS, "r+b");

    int head;             /* offset do primeiro espaço livre (HEAD da lista) */
    int curr = sizeof(int); /* posição atual: começa APÓS o cabeçalho (byte 4) */

    fseek(f, 0, SEEK_SET);
    fread(&head, sizeof(int), 1, f);

    /*
     * ftell(f) retorna a posição atual do cursor no arquivo.
     * Após fseek(SEEK_END), ftell retorna o TAMANHO total do arquivo.
     * Usamos isso para saber quando parar a travessia.
     */
    fseek(f, 0, SEEK_END);
    int end = ftell(f);

    /*
     * TRAVESSIA SEQUENCIAL DO ARQUIVO
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Percorremos registro por registro. Para cada registro:
     *   1. Lemos o tamanho (4 bytes)
     *   2. Lemos os dados (tamanho bytes)
     *   3. Verificamos se está removido (começa com '@')
     *   4. Se não está removido, comparamos o código
     *   5. Avançamos: curr += sizeof(int) + size
     *
     * Esta é a única forma de percorrer registros de tamanho variável:
     * lemos o tamanho e usamos ele para saber onde o próximo começa.
     */
    while (curr < end)
    {
        int size;
        fseek(f, curr, SEEK_SET);
        fread(&size, sizeof(int), 1, f);

        /*
         * malloc(size): aloca 'size' bytes na memória RAM.
         * Precisamos de alocação dinâmica porque 'size' é variável —
         * cada registro pode ter um tamanho diferente.
         */
        char *buf = malloc(size);
        fread(buf, sizeof(char), size, f);

        /*
         * buf[0] != '@' → este registro NÃO está removido.
         * Se buf[0] == '@', é um espaço livre e devemos ignorá-lo.
         */
        if (buf[0] != '@')
        {
            /*
             * strncmp(buf, cod_alvo, 4):
             * Compara os primeiros 4 bytes de buf com cod_alvo.
             * O payload começa com o código (4 dígitos), seguido de '|'.
             * Exemplo: buf = "7042|Aguas de Marco|Elis Regina|MPB"
             *          cod_alvo = "7042"
             * strncmp compara: '7','0','4','2' ↔ '7','0','4','2' → retorna 0 (igual!)
             *
             * POR QUE 4? Porque o código tem exatamente 4 caracteres (fixo).
             */
            if (strncmp(buf, cod_alvo, 4) == 0)
            {
                /* ============ ENCONTROU O REGISTRO A REMOVER ============ */

                /*
                 * PASSO 1: MARCAR COMO REMOVIDO
                 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                 * Escreve '@' no primeiro byte do payload (logo após o tamanho).
                 * Escreve -1 (FIM_LISTA) nos 4 bytes seguintes (ponteiro "próximo").
                 *
                 * Antes: [tam=30][7042|Aguas de Marco|Elis Regina|MPB\0]
                 * Depois: [tam=30][@][-1][...dados antigos que serão ignorados...]
                 *
                 * O campo de tamanho (30) NÃO é alterado. Isso é importante
                 * porque a travessia do arquivo usa esse valor para saber
                 * quantos bytes pular até o próximo registro.
                 */
                fseek(f, curr + sizeof(int), SEEK_SET);
                char marker = '@';
                int next_off = FIM_LISTA;
                fwrite(&marker, sizeof(char), 1, f);
                fwrite(&next_off, sizeof(int), 1, f);

                /*
                 * PASSO 2: ADICIONAR AO FINAL DA LISTA DE ESPAÇOS LIVRES
                 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                 * A especificação diz: "Um novo espaço disponível deve ser
                 * acrescentado sempre no final da lista (e não no início)."
                 *
                 * Dois casos:
                 */
                if (head == FIM_LISTA)
                {
                    /*
                     * CASO A: Lista está VAZIA (head == -1)
                     * O novo nó é o primeiro e único → head aponta para ele.
                     *
                     * Antes: head = -1
                     * Depois: head = curr (offset do registro recém removido)
                     */
                    fseek(f, 0, SEEK_SET);
                    fwrite(&curr, sizeof(int), 1, f);
                }
                else
                {
                    /*
                     * CASO B: Lista NÃO está vazia
                     * Precisamos percorrer até o ÚLTIMO nó (tail) e fazer
                     * ele apontar para o novo espaço.
                     *
                     * ANTES: head → [A] → [B] → -1
                     * DEPOIS: head → [A] → [B] → [NOVO] → -1
                     *
                     * O "NOVO" já tem next=-1 (escrito acima no passo 1).
                     */
                    int tail_off = head;
                    int next_tail;

                    /* Percorre a lista até encontrar o último nó (next == -1) */
                    while (1)
                    {
                        /*
                         * O campo "próximo" de um nó removido está em:
                         * offset_do_nó + sizeof(int) + sizeof(char)
                         * = offset + 4 (tamanho) + 1 ('@') = offset + 5
                         */
                        fseek(f, tail_off + sizeof(int) + sizeof(char), SEEK_SET);
                        fread(&next_tail, sizeof(int), 1, f);

                        if (next_tail == FIM_LISTA)
                            break; /* Encontrou o último! */

                        tail_off = next_tail; /* Avança para o próximo nó */
                    }

                    /*
                     * Atualiza o "próximo" do último nó para apontar para
                     * o novo espaço (curr).
                     * O novo espaço já tem next=-1 (configurado no passo 1).
                     */
                    fseek(f, tail_off + sizeof(int) + sizeof(char), SEEK_SET);
                    fwrite(&curr, sizeof(int), 1, f);
                }

                free(buf);
                break; /* Encontrou e removeu → sai do while */
            }
        }

        free(buf);
        /*
         * Avança para o próximo registro:
         * curr (início do registro atual) + sizeof(int) (campo tamanho) + size (dados)
         */
        curr += sizeof(int) + size;
    }

    fclose(f);
}


/* ===========================================================================
 *  FUNÇÃO: compactar()
 * ===========================================================================
 *  OBJETIVO: Reconstruir o arquivo, eliminando todos os registros removidos
 *            e toda a fragmentação (interna e externa).
 *
 *  O QUE É FRAGMENTAÇÃO?
 *  ~~~~~~~~~~~~~~~~~~~~
 *  - EXTERNA: "buracos" no arquivo onde havia registros removidos.
 *    Exemplo: [válido][REMOVIDO][válido] → o espaço do REMOVIDO é desperdiçado
 *
 *  - INTERNA: quando um registro usa um espaço maior do que precisa.
 *    Exemplo: espaço de 50 bytes, registro de 30 → 20 bytes desperdiçados
 *
 *  ESTRATÉGIA:
 *  ~~~~~~~~~~
 *  1. Abre o arquivo original para LEITURA
 *  2. Cria um arquivo temporário para ESCRITA
 *  3. Copia apenas os registros válidos, recalculando o tamanho real
 *  4. Substitui o arquivo original pelo temporário
 *
 *  POR QUE RECALCULAR O TAMANHO?
 *  Porque durante o best-fit, o campo de tamanho pode ter ficado com o
 *  valor original (maior). Usando strlen(buf)+1, obtemos o tamanho REAL
 *  do payload, eliminando a fragmentação interna.
 *
 *  RESULTADO:
 *  - Registros contíguos, sem buracos
 *  - Tamanhos corretos (sem fragmentação interna)
 *  - Cabeçalho resetado para -1 (sem espaços livres)
 *  - Arquivo menor (ou igual, se não havia fragmentação)
 * ===========================================================================
 */
void compactar()
{
    FILE *f = fopen(ARQUIVO_DADOS, "rb");     /* Leitura do original         */
    FILE *tmp = fopen("temp.bin", "wb");      /* Escrita do novo arquivo     */

    /*
     * O novo arquivo começa com cabeçalho = -1 (nenhum espaço livre,
     * já que estamos eliminando todos os buracos).
     */
    int empty_head = FIM_LISTA;
    fwrite(&empty_head, sizeof(int), 1, tmp);

    /* Determina onde começa e termina a área de registros */
    int curr = sizeof(int); /* pula o cabeçalho (4 bytes) */
    fseek(f, 0, SEEK_END);
    int end = ftell(f);     /* tamanho total do arquivo original */

    /*
     * PERCORRE TODOS OS REGISTROS DO ARQUIVO ORIGINAL
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * Para cada registro:
     * - Se válido (não começa com '@'): copia para o novo arquivo
     *   com o tamanho recalculado
     * - Se removido: ignora (não copia)
     */
    while (curr < end)
    {
        int size;
        fseek(f, curr, SEEK_SET);
        fread(&size, sizeof(int), 1, f);
        char *buf = malloc(size);
        fread(buf, sizeof(char), size, f);

        if (buf[0] != '@')  /* registro válido */
        {
            /*
             * strlen(buf) + 1 = tamanho REAL do payload (sem fragmentação)
             *
             * Exemplo: espaço tinha 50 bytes, payload real "7042|Song|Art|Pop\0"
             * strlen("7042|Song|Art|Pop") = 17 → real_size = 18
             * Antes da compactação: tamanho armazenado = 50 (fragmentação = 32)
             * Depois: tamanho armazenado = 18 (sem fragmentação)
             */
            int real_size = strlen(buf) + 1;
            fwrite(&real_size, sizeof(int), 1, tmp);
            fwrite(buf, sizeof(char), real_size, tmp);
        }
        /* Se buf[0] == '@' → removido → simplesmente NÃO copia */

        free(buf);
        curr += sizeof(int) + size; /* avança para o próximo registro */
    }

    fclose(f);
    fclose(tmp);

    /*
     * SUBSTITUIÇÃO ATÔMICA DO ARQUIVO
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * remove() apaga o arquivo antigo.
     * rename() renomeia "temp.bin" para "dados.bin".
     *
     * POR QUE não escrever direto no original?
     * Se o programa crashar no meio da escrita, perderíamos os dados.
     * Escrevendo em um temporário, o original fica intacto até o rename().
     */
    remove(ARQUIVO_DADOS);
    rename("temp.bin", ARQUIVO_DADOS);
}


/* ===========================================================================
 *  FUNÇÃO: dump()
 * ===========================================================================
 *  OBJETIVO: Exibir na tela todo o conteúdo do arquivo, registro por registro.
 *
 *  Mostra:
 *  - O offset do HEAD (primeiro espaço livre)
 *  - Para cada registro:
 *    - Seu offset (posição no arquivo)
 *    - Seu tamanho
 *    - Se é DADOS (válido) ou REMOVIDO
 *    - Se removido, mostra o próximo nó da lista de livres
 *
 *  ÚTIL PARA: depuração, verificação visual de que as operações estão
 *  funcionando corretamente.
 *
 *  EXEMPLO DE SAÍDA:
 *    --- DUMP (HEAD Offset: 44) ---
 *    [Off:    4] Tamanho:  36 | DADOS: 7042|Aguas de Marco|Elis Regina|MPB
 *    [Off:   44] Tamanho:  34 | REMOVIDO | Prox: -1
 *    [Off:   82] Tamanho:  44 | DADOS: 1523|Garota de Ipanema|Tom Jobim|Bossa Nova
 *    ------------------------------
 * ===========================================================================
 */
void dump()
{
    FILE *f = fopen(ARQUIVO_DADOS, "rb");
    if (!f)
        return; /* Arquivo não existe → nada para mostrar */

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
            /*
             * REGISTRO REMOVIDO
             * memcpy copia bytes brutos — aqui, lê o inteiro de 4 bytes
             * que está logo após o '@', que é o offset do próximo nó.
             */
            int next;
            memcpy(&next, buf + 1, sizeof(int));
            printf("[Off: %4d] Tamanho: %3d | REMOVIDO | Prox: %d\n", curr, size, next);
        }
        else
        {
            /*
             * REGISTRO VÁLIDO
             * buf é uma string terminada em '\0', então podemos usar %s.
             */
            printf("[Off: %4d] Tamanho: %3d | DADOS: %s\n", curr, size, buf);
        }

        free(buf);
        curr += sizeof(int) + size;
    }

    printf("------------------------------\n");
    fclose(f);
}


/* ===========================================================================
 *  FUNÇÕES DE PERSISTÊNCIA DE ESTADO
 * ===========================================================================
 *  salvar_estado() / carregar_estado()
 *
 *  PROBLEMA: O programa pode ser encerrado e reaberto múltiplas vezes.
 *  Os dados dos registros estão seguros em dados.bin. Mas e as posições
 *  nos vetores de inserção/remoção (pos_insere, pos_remove)?
 *
 *  SOLUÇÃO: Salvar essas posições em um arquivo auxiliar (estado.bin).
 *  Ao reabrir o programa e carregar os arquivos de teste, restauramos
 *  as posições anteriores.
 *
 *  FORMATO DO estado.bin:
 *    [4 bytes: pos_insere] [4 bytes: pos_remove]
 *    Total: 8 bytes
 * ===========================================================================
 */
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

void carregar_estado()
{
    FILE *f = fopen(ARQUIVO_ESTADO, "rb");
    if (f)
    {
        fread(&pos_insere, sizeof(int), 1, f);
        fread(&pos_remove, sizeof(int), 1, f);
        fclose(f);
    }
    /* Se o arquivo não existir, pos_insere e pos_remove ficam com seus
     * valores iniciais (0), o que é correto para uma primeira execução. */
}


/* ===========================================================================
 *  FUNÇÃO: carregar_arquivos_teste()
 * ===========================================================================
 *  OBJETIVO: Carregar os dados dos arquivos insere.bin e remove.bin
 *            para a memória RAM (vetores), facilitando os testes.
 *
 *  insere.bin contém um vetor de structs Faixa escritas consecutivamente.
 *  Cada Faixa tem sizeof(Faixa) = 138 bytes.
 *  O total de faixas = tamanho_do_arquivo / sizeof(Faixa).
 *
 *  remove.bin contém códigos de 5 bytes cada (4 dígitos + '\0').
 *  O total de códigos = tamanho_do_arquivo / 5.
 *
 *  POR QUE CARREGAR EM MEMÓRIA?
 *  A especificação sugere: "carregar o arquivo em memória (um vetor de
 *  struct, por exemplo) e ir acessando cada posição conforme as inserções
 *  vão ocorrendo." Isso é mais prático do que ler do arquivo a cada operação.
 * ===========================================================================
 */
void carregar_arquivos_teste()
{
    /* ------------------- CARREGAR insere.bin ------------------- */
    FILE *f = fopen(ARQUIVO_INSERE, "rb");
    if (f)
    {
        /*
         * TÉCNICA: Descobrir o tamanho do arquivo para calcular
         * quantos elementos ele contém.
         *   1. fseek(SEEK_END) → vai pro final
         *   2. ftell → retorna a posição (= tamanho total em bytes)
         *   3. total = tamanho / sizeof(um_elemento)
         */
        fseek(f, 0, SEEK_END);
        long tam = ftell(f);
        total_insere = tam / sizeof(Faixa);
        fseek(f, 0, SEEK_SET); /* Volta pro início para ler os dados */

        /* Libera memória anterior se existir (evita memory leak) */
        if (faixas_insere)
            free(faixas_insere);

        /*
         * malloc: aloca espaço na RAM para 'total_insere' structs Faixa.
         * (Faixa *) é um cast → diz ao compilador o tipo do ponteiro.
         */
        faixas_insere = (Faixa *)malloc(total_insere * sizeof(Faixa));

        /* Lê TODAS as faixas de uma vez do arquivo para o vetor */
        fread(faixas_insere, sizeof(Faixa), total_insere, f);
        fclose(f);

        printf("Carregadas %d faixas de %s.\n", total_insere, ARQUIVO_INSERE);
    }
    else
    {
        printf("Arquivo %s nao encontrado.\n", ARQUIVO_INSERE);
    }

    /* ------------------- CARREGAR remove.bin ------------------- */
    f = fopen(ARQUIVO_REMOVE, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long tam = ftell(f);
        total_remove = tam / 5; /* Cada código: 4 chars + '\0' = 5 bytes */
        fseek(f, 0, SEEK_SET);

        if (codigos_remove)
            free(codigos_remove);

        /*
         * codigos_remove é um ponteiro para arrays de char[5].
         * Cada "posição" é um array de 5 bytes (um código completo).
         * Acesso: codigos_remove[i] → retorna a string do i-ésimo código.
         */
        codigos_remove = malloc(total_remove * 5);
        fread(codigos_remove, 5, total_remove, f);
        fclose(f);

        printf("Carregados %d codigos de %s.\n", total_remove, ARQUIVO_REMOVE);
    }
    else
    {
        printf("Arquivo %s nao encontrado.\n", ARQUIVO_REMOVE);
    }

    /* Restaura posições de uma execução anterior */
    carregar_estado();
    printf("Posicao atual: insere=%d/%d, remove=%d/%d\n",
           pos_insere, total_insere, pos_remove, total_remove);
}


/* ===========================================================================
 *  FUNÇÃO: inserir_proximo()
 * ===========================================================================
 *  Função auxiliar para o menu: insere a PRÓXIMA faixa do vetor
 *  faixas_insere (que foi carregado de insere.bin).
 *
 *  Funciona como um "iterador": cada chamada avança pos_insere em 1.
 *  Quando pos_insere >= total_insere, todas as faixas já foram inseridas.
 *  Após cada inserção, salva o estado para persistência.
 * ===========================================================================
 */
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

    Faixa fx = faixas_insere[pos_insere]; /* Pega a próxima faixa do vetor */
    inserir(fx);                          /* Insere no arquivo de dados     */

    printf("Inserida faixa [%d/%d]: %s|%s|%s|%s\n",
           pos_insere + 1, total_insere,
           fx.cod, fx.nome, fx.artista, fx.genero);

    pos_insere++;       /* Avança o índice para a próxima chamada */
    salvar_estado();    /* Persiste a posição no disco             */
}


/* ===========================================================================
 *  FUNÇÃO: remover_proximo()
 * ===========================================================================
 *  Função auxiliar para o menu: remove a faixa cujo código é o PRÓXIMO
 *  do vetor codigos_remove (carregado de remove.bin).
 *
 *  Mesma lógica de "iterador" que inserir_proximo().
 * ===========================================================================
 */
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

    remover(codigos_remove[pos_remove]); /* Remove pelo código */

    pos_remove++;
    salvar_estado();
}


/* ===========================================================================
 *  FUNÇÃO: main()
 * ===========================================================================
 *  Ponto de entrada do programa. Apresenta um menu interativo com as
 *  5 operações especificadas:
 *
 *  1. Inserir faixa    → inserir_proximo() → inserir()
 *  2. Remover faixa    → remover_proximo() → remover()
 *  3. Compactar         → compactar()
 *  4. Dump              → dump()
 *  5. Carregar arquivos → carregar_arquivos_teste()
 *  0. Sair
 *
 *  Usa um loop do-while que repete até o usuário escolher 0.
 *  do-while garante que o menu é mostrado pelo menos uma vez.
 *
 *  COMPILAÇÃO E EXECUÇÃO:
 *    gcc -o ex ex_comentado.c     ← gera o executável "ex"
 *    ./ex                          ← executa o programa
 *
 *  O executável "ex" é um arquivo binário em linguagem de máquina.
 *  Ele contém todo o código compilado deste .c mais as bibliotecas
 *  do C padrão. Não é legível por humanos — para entender o código,
 *  sempre consulte o fonte (.c).
 * ===========================================================================
 */
int main()
{
    /* Primeira coisa: garantir que o arquivo de dados existe */
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

        /*
         * scanf("%d", &opcao): lê um inteiro do teclado.
         * &opcao → endereço da variável onde guardar o valor lido.
         * O & é necessário porque scanf precisa saber ONDE escrever.
         */
        scanf("%d", &opcao);

        /*
         * switch: estrutura de seleção múltipla.
         * Cada 'case' trata uma opção do menu.
         * 'break' é necessário para não "cair" no case seguinte.
         * 'default' trata qualquer valor não coberto pelos cases.
         */
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

    /*
     * LIBERAÇÃO DE MEMÓRIA
     * ~~~~~~~~~~~~~~~~~~~~
     * Toda memória alocada com malloc() deve ser liberada com free()
     * antes do programa encerrar. Caso contrário, temos um "memory leak"
     * (vazamento de memória).
     *
     * Verificamos if(ptr) antes de free() por segurança — free(NULL) é
     * permitido em C, mas a verificação explicita é boa prática.
     */
    if (faixas_insere)
        free(faixas_insere);
    if (codigos_remove)
        free(codigos_remove);

    return 0; /* 0 indica que o programa terminou com sucesso */
}
