CC     = gcc
CFLAGS = -Wall -Wextra

all: vm

# Padrão: 128 frames, com threads, TLB ativa
vm: vm.c
	$(CC) $(CFLAGS) -pthread \
	  -DCONFIG_FRAMES_ATIVOS=128 \
	  -DCONFIG_USAR_THREADS=1 \
	  -DCONFIG_DESABILITAR_TLB=0 \
	  vm.c -o vm

# Sem TLB, 256 frames
vm256: vm.c
	$(CC) $(CFLAGS) -pthread \
	  -DCONFIG_FRAMES_ATIVOS=256 \
	  -DCONFIG_USAR_THREADS=1 \
	  -DCONFIG_DESABILITAR_TLB=1 \
	  vm.c -o vm

# Sem TLB, 128 frames
vm128_notlb: vm.c
	$(CC) $(CFLAGS) -pthread \
	  -DCONFIG_FRAMES_ATIVOS=128 \
	  -DCONFIG_USAR_THREADS=1 \
	  -DCONFIG_DESABILITAR_TLB=1 \
	  vm.c -o vm

# Com TLB, sem threads, 128 frames
vm_nothread: vm.c
	$(CC) $(CFLAGS) \
	  -DCONFIG_FRAMES_ATIVOS=128 \
	  -DCONFIG_USAR_THREADS=0 \
	  -DCONFIG_DESABILITAR_TLB=0 \
	  vm.c -o vm

clean:
	rm -f vm correct.txt