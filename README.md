# 🖥️ Simulador de Memória Virtual (VM) - Multithreading & Paging

Este projeto consiste em um simulador de gestão de memória virtual que realiza a tradução de endereços lógicos (virtuais) em endereços físicos, gerenciando uma **Tabela de Páginas** e uma **TLB (Translation Lookaside Buffer)** integrada. O grande diferencial do projeto é o suporte à concorrência utilizando múltiplas threads para otimizar as buscas no hardware simulado.

O programa foi desenvolvido e validado para garantir correspondência absoluta (**100% de igualdade via teste `diff -s`**) com as saídas de referência do enunciado (gabarito).

---

## 💻 Sistema Operacional de Desenvolvimento

O projeto foi totalmente implementado, testado e homologado no seguinte ambiente:

- **Sistema Operacional:** Linux (distribuições baseadas em Ubuntu/Debian ou ambientes POSIX compatíveis).
- **Compilador:** `gcc` (GNU Compiler Collection).
- **Biblioteca de Concorrência:** POSIX Threads (`pthread`).

---

## 📁 Descrição dos Arquivos Utilizados

### `vm.c`

Código-fonte principal e unificado do simulador. Contém toda a lógica de:

- Tratamento e validação de erros de entrada;
- Cálculo de paginação (página e offset);
- Estruturas de dados da Tabela de Páginas e da TLB;
- Rotinas de substituição de páginas e entradas (FIFO e LRU);
- Infraestrutura concorrente baseada em `pthread` para busca paralela na TLB utilizando 16 threads independentes.

### `Makefile`

Arquivo de automação de compilação profissional.

Responsável por:

- Configurar flags rigorosas de compilação;
- Vincular a biblioteca de concorrência (`-pthread`);
- Ativar otimizações de código (`-O2`);
- Separar os diferentes alvos de compilação;
- Disponibilizar diretivas de limpeza (`clean`) através de alvos `.PHONY`.

### `addresses.txt`

Arquivo de entrada contendo a lista de endereços lógicos (inteiros de 16 bits em formato texto), um por linha, a serem traduzidos pelo simulador.

### `BACKING_STORE.bin`

Arquivo binário que simula o armazenamento secundário (*hard drive*).

Características:

- Possui exatamente **65.536 bytes**;
- É utilizado para carregar dados para a memória física sempre que ocorre um evento de **Page Fault**.

---

## ⚙️ Funcionalidades Implementadas

### ✅ Tradução de Endereços

- Conversão de endereços lógicos de 16 bits em endereços físicos;
- Separação automática entre número da página e deslocamento (*offset*);
- Extração correta dos bytes armazenados na memória física.

### ✅ Gerenciamento de TLB

- Cache de tradução com 16 entradas;
- Políticas FIFO e LRU;
- Contabilização de TLB Hits;
- Atualização automática das entradas durante traduções.

### ✅ Gerenciamento de Memória Física

- Memória física limitada a 128 frames;
- Carregamento sob demanda (*Demand Paging*);
- Tratamento completo de Page Faults;
- Leitura de páginas diretamente do `BACKING_STORE.bin`.

### ✅ Algoritmos de Substituição

#### FIFO (First-In, First-Out)

- Remove a página ou entrada mais antiga;
- Implementação baseada em fila circular.

#### LRU (Least Recently Used)

- Remove a página ou entrada menos recentemente utilizada;
- Atualização contínua do histórico de acesso.

### ✅ Multithreading

- Busca paralela na TLB utilizando 16 threads;
- Distribuição concorrente da carga de trabalho;
- Sincronização utilizando POSIX Threads (`pthread`);
- Redução do custo de busca em cenários de alta carga.

### ✅ Validação de Entrada

- Verificação da quantidade correta de argumentos;
- Validação dos algoritmos informados;
- Tratamento de erros de abertura de arquivos;
- Mensagens claras de erro para entradas inválidas.

---

## 🛠️ Como Compilar o Programa

A compilação do projeto é simplificada e automatizada por meio do **Makefile**.

Abra o terminal na pasta do projeto e execute:

```bash
make
```

Este comando invocará o compilador com as flags:

```bash
-Wall -Wextra -pthread -O2
```

Ao final do processo serão gerados:

- O arquivo objeto intermediário `vm.o`;
- O executável principal `vm`.

### Limpeza dos Arquivos Gerados

Para remover todos os binários e arquivos temporários:

```bash
make clean
```

---

## 🚀 Como Executar o Programa

O programa recebe exatamente **3 argumentos** via linha de comando.

### Sintaxe

```bash
./vm <arquivo_de_enderecos> <algoritmo_pagina> <algoritmo_tlb>
```

### Parâmetros Suportados

#### `<arquivo_de_enderecos>`

Caminho para o arquivo contendo os endereços lógicos.

Exemplo:

```text
addresses.txt
```

#### `<algoritmo_pagina>`

Política de substituição da Tabela de Páginas quando a memória física de 128 frames estiver cheia.

Opções:

- `fifo` → First-In, First-Out
- `lru` → Least Recently Used

#### `<algoritmo_tlb>`

Política de substituição para as 16 entradas da TLB.

Opções:

- `fifo` → substituição circular da entrada mais antiga;
- `lru` → substituição baseada no último acesso.

### Exemplo de Execução

Utilizando:

- LRU para a Tabela de Páginas;
- FIFO para a TLB.

```bash
./vm addresses.txt lru fifo
```

O programa processará os endereços e gerará o arquivo:

```text
correct.txt
```

contendo todas as traduções realizadas.

---

## 🧪 Como Testar o Programa

Após a execução, o simulador:

- Exibe estatísticas gerais no terminal;
- Grava as traduções detalhadas em `correct.txt`.

### Estatísticas Exibidas

- Número total de traduções;
- Quantidade de Page Faults;
- Quantidade de TLB Hits;
- Taxa de Page Faults;
- Taxa de TLB Hits.

---

## 📋 Como Testar Cada Rubrica

A implementação foi estruturada para permitir a validação individual de cada requisito do projeto através de diferentes alvos do `Makefile`.

| Rubrica | Comando de Compilação | Execução |
|----------|----------------------|-----------|
| TLB com thread, FIFO | `make` | `./vm address.txt fifo fifo` |
| TLB com thread, LRU | `make` | `./vm address.txt lru lru` |
| TLB sem thread, FIFO | `make vm_nothread` | `./vm address.txt fifo fifo` |
| TLB sem thread, LRU | `make vm_nothread` | `./vm address.txt lru lru` |
| Sem TLB, 128 frames, FIFO | `make vm128_notlb` | `./vm address.txt fifo fifo` |
| Sem TLB, 128 frames, LRU | `make vm128_notlb` | `./vm address.txt lru lru` |
| Sem TLB, 256 frames | `make vm256` | `./vm address.txt fifo fifo` |

### 🎯 Objetivo de Cada Configuração

#### TLB com Thread

Implementação completa contendo:

- TLB ativa;
- Busca concorrente utilizando múltiplas threads;
- Políticas FIFO ou LRU.

#### TLB sem Thread

Implementação da TLB funcionando de forma sequencial:

- Sem paralelismo;
- Mesma lógica de tradução;
- Utilizada para comparação de desempenho.

#### Sem TLB (128 Frames)

Execução utilizando apenas:

- Tabela de páginas;
- Memória física limitada a 128 frames;
- Substituição FIFO ou LRU.

#### Sem TLB (256 Frames)

Configuração especial que:

- Desabilita completamente a TLB;
- Disponibiliza 256 frames na memória física;
- Permite observar o comportamento do simulador sem pressão de substituição de páginas.

---

## 📊 Conceitos de Sistemas Operacionais Aplicados

Este projeto aplica diretamente os conceitos clássicos apresentados por Silberschatz em Sistemas Operacionais:

- Memória Virtual;
- Paginação;
- Tabela de Páginas;
- Translation Lookaside Buffer (TLB);
- Demand Paging;
- Page Faults;
- Algoritmos FIFO;
- Algoritmos LRU;
- Substituição de Páginas;
- Programação Concorrente;
- Threads POSIX (`pthread`).

---

## 👤 Autor

Desenvolvido por **Gabriel Lima** como parte dos requisitos da disciplina de **Sistemas Operacionais**, com foco na implementação prática de mecanismos de memória virtual, gerenciamento de páginas e programação concorrente.