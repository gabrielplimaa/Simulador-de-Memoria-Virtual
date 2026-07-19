#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#define TLB_SIZE 16
#define PAGE_TABLE_SIZE 256
#define FRAME_SIZE 256

#ifndef CONFIG_FRAMES_ATIVOS
#define CONFIG_FRAMES_ATIVOS 128
#endif

#ifndef CONFIG_USAR_THREADS
#define CONFIG_USAR_THREADS 1
#endif

#ifndef CONFIG_DESABILITAR_TLB
#define CONFIG_DESABILITAR_TLB 0
#endif


typedef struct memoria{
    int tabela_paginas[PAGE_TABLE_SIZE];
    signed char **frames_fisico;

    int tlb_pagina[TLB_SIZE];
    int tlb_frame[TLB_SIZE];
    int tlb_tempo[TLB_SIZE];

    int *frame_reverso;
    int *frame_tempo;

    char algoritmo_pagina[10];
    char algoritmo_tlb[10];
    int  frames_ativos;
    int  desabilitar_tlb;
}memoria;

typedef struct dados{
    int id;
    int pagina_procurada;
    int tlb_pagina_val;
    int tlb_frame_val;
} dados;

pthread_mutex_t tlb_mutex = PTHREAD_MUTEX_INITIALIZER;
int frame_compartilhado = -1;
int indice_tlb_compartilhado = -1;

void *buscar_entrada_tlb(void *arg){
    dados *d = (dados*) arg;
    if(d->tlb_pagina_val != -1 && d->tlb_pagina_val == d->pagina_procurada){
        pthread_mutex_lock(&tlb_mutex);
        if(frame_compartilhado == -1){
            frame_compartilhado = d->tlb_frame_val;
            indice_tlb_compartilhado = d->id;
        }
        pthread_mutex_unlock(&tlb_mutex);
    }
    pthread_exit(NULL);
}

static int buscar_tlb_sequencial(memoria *m, int numero_pagina, int *frame_out){
    for(int i = 0; i < TLB_SIZE; i++){
        if(m->tlb_pagina[i] != -1 && m->tlb_pagina[i] == numero_pagina){
            *frame_out = m->tlb_frame[i];
            return i;
        }
    }
    return -1;
}

static int buscar_tlb_threads(memoria *m, int numero_pagina, int *frame_out){
    pthread_t threads[TLB_SIZE];
    dados thread_data[TLB_SIZE];

    frame_compartilhado = -1;
    indice_tlb_compartilhado = -1;

    for(int i = 0; i < TLB_SIZE; i++){
        thread_data[i].id = i;
        thread_data[i].pagina_procurada = numero_pagina;
        thread_data[i].tlb_pagina_val = m->tlb_pagina[i];
        thread_data[i].tlb_frame_val = m->tlb_frame[i];
        pthread_create(&threads[i], NULL, buscar_entrada_tlb, (void *) &thread_data[i]);
    }
    for (int i = 0; i < TLB_SIZE; i++)
        pthread_join(threads[i], NULL);

    *frame_out = frame_compartilhado;
    return indice_tlb_compartilhado;
}

static int inserir_tlb(memoria *m, int numero_pagina, int frame, int tempo_global, int *proxtlb_fifo){
    int indice = -1;
    for(int i = 0; i < TLB_SIZE; i++){
        if (m->tlb_pagina[i] == -1) { indice = i; break; }
    }

    if(indice == -1){
        if(strcmp(m->algoritmo_tlb, "fifo") == 0){
            indice = *proxtlb_fifo;
            *proxtlb_fifo = (*proxtlb_fifo + 1) % TLB_SIZE;
        }else{
            int menor = m->tlb_tempo[0];
            indice = 0;
            for(int i = 1; i < TLB_SIZE; i++){
                if(m->tlb_tempo[i] < menor){
                    menor  = m->tlb_tempo[i];
                    indice = i;
                }
            }
        }
    }

    m->tlb_pagina[indice] = numero_pagina;
    m->tlb_frame[indice] = frame;
    m->tlb_tempo[indice] = tempo_global;
    return indice;
}

static void liberar_recursos(memoria *m, FILE *enderecos, FILE *backing_store, FILE *saida){
    if(m->frames_fisico){
        for(int i = 0; i < m->frames_ativos; i++)
            if(m->frames_fisico[i]) free(m->frames_fisico[i]);
        free(m->frames_fisico);
    }
    if(m->frame_reverso) free(m->frame_reverso);
    if(m->frame_tempo) free(m->frame_tempo);
    if(enderecos) fclose(enderecos);
    if(backing_store) fclose(backing_store);
    if(saida) fclose(saida);
}

int main(int argc, char *argv[]){
    if(argc != 4){
        fprintf(stderr,"Erro: Numero incorreto de argumentos.\n" "Uso: %s <arquivo_enderecos> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 1;
    }

    for(int i = 0; argv[2][i]; i++) argv[2][i] = (char) tolower((unsigned char) argv[2][i]);
    for(int i = 0; argv[3][i]; i++) argv[3][i] = (char) tolower((unsigned char) argv[3][i]);

    memoria m;
    memset(&m, 0, sizeof(m));
    strcpy(m.algoritmo_pagina, argv[2]);
    strcpy(m.algoritmo_tlb, argv[3]);
    m.frames_ativos = CONFIG_FRAMES_ATIVOS;
    m.desabilitar_tlb = CONFIG_DESABILITAR_TLB;

    if(strcmp(m.algoritmo_pagina, "fifo") != 0 && strcmp(m.algoritmo_pagina, "lru")  != 0){
        fprintf(stderr, "Erro: Algoritmo de pagina invalido '%s'. Use 'fifo' ou 'lru'.\n", m.algoritmo_pagina);
        return 1;
    }

    if(strcmp(m.algoritmo_tlb, "fifo") != 0 && strcmp(m.algoritmo_tlb, "lru")  != 0){
        fprintf(stderr, "Erro: Algoritmo de TLB invalido '%s'. Use 'fifo' ou 'lru'.\n", m.algoritmo_tlb);
        return 1;
    }
    int usar_threads = (m.desabilitar_tlb) ? 0 : CONFIG_USAR_THREADS;

    m.frames_fisico = (signed char **) malloc(m.frames_ativos * sizeof(signed char *));
    if(!m.frames_fisico) { fprintf(stderr, "Erro: malloc falhou.\n"); return 1; }

    for(int i = 0; i < m.frames_ativos; i++){
        m.frames_fisico[i] = (signed char *) malloc(FRAME_SIZE * sizeof(signed char));
        if (!m.frames_fisico[i]) {
            fprintf(stderr, "Erro: malloc falhou.\n");
            for (int j = 0; j < i; j++) free(m.frames_fisico[j]);
            free(m.frames_fisico);
            return 1;
        }
    }

    m.frame_reverso = (int *) malloc(m.frames_ativos * sizeof(int));
    m.frame_tempo = (int *) malloc(m.frames_ativos * sizeof(int));
    if(!m.frame_reverso || !m.frame_tempo){
        fprintf(stderr, "Erro: malloc falhou.\n");
        liberar_recursos(&m, NULL, NULL, NULL);
        return 1;
    }

    for(int i = 0; i < PAGE_TABLE_SIZE; i++) m.tabela_paginas[i] = -1;
    for(int i = 0; i < m.frames_ativos; i++) { m.frame_tempo[i] = 0; m.frame_reverso[i] = -1; }
    for(int i = 0; i < TLB_SIZE; i++){ m.tlb_pagina[i] = -1; m.tlb_frame[i] = -1; m.tlb_tempo[i] = 0; }

    FILE *enderecos = fopen(argv[1], "r");
    if(!enderecos){
        fprintf(stderr, "Erro: O arquivo de enderecos '%s' nao existe ou nao pode ser aberto.\n", argv[1]);
        liberar_recursos(&m, NULL, NULL, NULL);
        return 1;
    }

    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");
    if(!backing_store){
        fprintf(stderr, "Erro: O arquivo 'BACKING_STORE.bin' nao foi encontrado.\n");
        liberar_recursos(&m, enderecos, NULL, NULL);
        return 1;
    }

    FILE *saida = fopen("correct.txt", "w");
    if(!saida){
        fprintf(stderr, "Erro: Nao foi possivel criar o arquivo de saida 'correct.txt'.\n");
        liberar_recursos(&m, enderecos, backing_store, NULL);
        return 1;
    }
    char linha[50];
    int total_enderecos = 0;
    int page_faults = 0;
    int tlb_hits = 0;
    int proxframe_livre = 0;
    int fifo_page_ptr = 0;
    int proxtlb_fifo = 0;
    int tempo_global = 0;

    while(fgets(linha, sizeof(linha), enderecos) != NULL){
        int len = (int) strlen(linha);
        while(len > 0 && isspace((unsigned char) linha[len - 1]))
            linha[--len] = '\0';
        if(len == 0) continue;

        int linha_valida = 1;
        for(int i = 0; i < len; i++){
            if(!isdigit((unsigned char) linha[i])){
                linha_valida = 0;
                break;
            }
        }
        if(!linha_valida){
            fprintf(stderr, "Erro: Formato invalido detectado no arquivo de enderecos.\n");
            liberar_recursos(&m, enderecos, backing_store, saida);
            return 1;
        }

        int endereco = atoi(linha);
        if(endereco < 0 || endereco > 65535){
            fprintf(stderr, "Erro: Endereco %d fora dos limites (0-65535).\n", endereco);
            liberar_recursos(&m, enderecos, backing_store, saida);
            return 1;
        }

        int numero_pagina = endereco / FRAME_SIZE;
        int offset = endereco % FRAME_SIZE;
        tempo_global++;
        total_enderecos++;

        int frame = -1;
        int tlb_imprimir = -1;
        int tlb_hit_indice = -1;

        if(!m.desabilitar_tlb){
            if(usar_threads)
                tlb_hit_indice = buscar_tlb_threads(&m, numero_pagina, &frame);
            else{
                tlb_hit_indice = buscar_tlb_sequencial(&m, numero_pagina, &frame);
            }          
        }

        if(tlb_hit_indice != -1){
            tlb_hits++;
            tlb_imprimir = tlb_hit_indice;
            if(strcmp(m.algoritmo_tlb, "lru") == 0)
                m.tlb_tempo[tlb_hit_indice] = tempo_global;
        }else{
            frame = m.tabela_paginas[numero_pagina];

            if(frame == -1){
                page_faults++;

                if(proxframe_livre < m.frames_ativos){
                    frame = proxframe_livre++;
                }else{
                    if(strcmp(m.algoritmo_pagina, "fifo") == 0){
                        frame = fifo_page_ptr;
                        fifo_page_ptr = (fifo_page_ptr + 1) % m.frames_ativos;
                    }else{
                        int menor_tempo = m.frame_tempo[0];
                        frame = 0;
                        for(int i = 1; i < m.frames_ativos; i++){
                            if(m.frame_tempo[i] < menor_tempo){
                                menor_tempo = m.frame_tempo[i];
                                frame = i;
                            }
                        }
                    }

                    int pagina_antiga = m.frame_reverso[frame];
                    if(pagina_antiga != -1){
                        m.tabela_paginas[pagina_antiga] = -1;
                        for(int i = 0; i < TLB_SIZE; i++){
                            if(m.tlb_pagina[i] == pagina_antiga){
                                m.tlb_pagina[i] = -1;
                                m.tlb_frame[i]  = -1;
                                m.tlb_tempo[i]  = 0;
                            }
                        }
                    }
                }

                fseek(backing_store, numero_pagina * FRAME_SIZE, SEEK_SET);
                size_t lidos = fread(m.frames_fisico[frame], sizeof(char), FRAME_SIZE, backing_store);
                (void) lidos;

                m.tabela_paginas[numero_pagina] = frame;
                m.frame_reverso[frame] = numero_pagina;

                if(strcmp(m.algoritmo_pagina, "fifo") == 0)
                    m.frame_tempo[frame] = tempo_global;
            }

            if(!m.desabilitar_tlb) tlb_imprimir = inserir_tlb(&m, numero_pagina, frame, tempo_global, &proxtlb_fifo);
        }

        if(strcmp(m.algoritmo_pagina, "lru") == 0){
            m.frame_tempo[frame] = tempo_global;
        }

        int endereco_fisico = (frame * FRAME_SIZE) + offset;
        int valor = (int) m.frames_fisico[frame][offset];

        if(m.desabilitar_tlb){
            fprintf(saida, "Virtual address: %d Physical address: %d Value: %d\n", endereco, endereco_fisico, valor);
        }else{
            fprintf(saida, "Virtual address: %d TLB: %d Physical address: %d Value: %d\n", endereco, tlb_imprimir, endereco_fisico, valor);
        }
    }

    fprintf(saida, "Number of Translated Addresses = %d\n", total_enderecos);
    fprintf(saida, "Page Faults = %d\n", page_faults);
    fprintf(saida, "Page Fault Rate = %.3f\n", (float) page_faults / total_enderecos);
    fprintf(saida, "TLB Hits = %d\n", tlb_hits);
    fprintf(saida, "TLB Hit Rate = %.3f\n", (float) tlb_hits / total_enderecos);

    liberar_recursos(&m, enderecos, backing_store, saida);
    return 0;
}