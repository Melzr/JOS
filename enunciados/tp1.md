# TP1: Manejo de memoria

El principal objetivo de este TP es la implementación del sistema de memoria virtual en JOS.

Las partes 1 a 3 corresponden a los tres principales componentes del sistema de memoria en cualquier sistema operativo:

1. la lista de páginas físicas libres, construida a partir de la cantidad de memoria de la computadora.

2. funciones para manipular page tables y page directories.

3. el espacio virtual de memoria del propio kernel, que persiste toda la ejecución una vez inicializado.

La parte 4 consiste en una optimización para un uso reducido de memoria en sistemas modernos.

Al finalizar el TP1 el resultado de la auto-corrección debe tener este aspecto:

```console
$ make grade
running JOS: OK (2.1s)
Physical page allocator: OK
Page management: OK
Kernel page directory: OK
Page management 2: OK
Large pages: OK (2.4s)
Score: 5/5
```

<br/>

## Parte 1: Memoria física

En JOS, la información sobre el estado de cada página física se mantiene en un arreglo global definido en pmap.c (el tamaño del arreglo es, precisamente, el número total de páginas físicas):

    struct PageInfo *pages;  // Arreglo de páginas físicas.

El arreglo se crea en tiempo de ejecución porque el número de páginas no es una constante sino que depende de la cantidad real de memoria de la máquina donde corre JOS.

La función `i386_detect_memory()` es quien detecta la cantidad de memoria disponible. Esta función ya está implementada y no es necesario cambiarla. El resultado se almacena en la variable global `npages`: el número total de páginas físicas.

De cada página física interesa saber si está asignada o no. Se emplea un entero en lugar de un booleano para, en el futuro, poder compartir una misma página en varios procesos. Asimismo, para poder encontrar la siguiente página libre en O(1), el mismo struct PageInfo actúa como lista enlazada de páginas libres.

```C
struct PageInfo {
    uint16_t pp_ref;
    struct PageInfo *pp_link;
};
```

#### Tarea: mem_init_pages

Añadir a `mem_init()` código para crear el arreglo de páginas `pages`. Se debe determinar cuánto espacio se necesita, e inicializar a 0 usando `memset()`.

En esta fase temprana de la inicialización del sistema, la memoria se reserva mediante la función auxiliar `boot_alloc()`. Una vez se termine de inicializar el sistema de memoria, todas las reservas se realizarán mediante la función `page_alloc()` a implementar en esta parte.

#### Tarea: boot_alloc

Completar la implementación de `boot_alloc()` en pmap.c; esta función implementa una “reserva rudimentaria” de la siguiente manera:

- la variable estática nextfree guarda la siguiente posición de memoria que se puede usar (alineada a 4096 bytes).
- la reserva se realiza siempre en páginas físicas (múltiplos de 4096 bytes).
- se devuelve una dirección virtual, no física.

Si no hubiera suficiente memoria, la invocación debe resultar en panic().

#### Tarea: boot_alloc_pos

Incluir en el archivo TP1.md:

a. Un cálculo manual de la primera dirección de memoria que devolverá boot_alloc() tras el arranque. Se puede calcular a partir del binario compilado (obj/kern/kernel), usando los comandos readelf y/o nm y operaciones matemáticas.

b. Una sesión de GDB en la que, poniendo un breakpoint en la función boot_alloc(), se muestre el valor devuelto en esa primera llamada, usando el comando GDB finish.

#### Tarea: page_init

El siguiente paso tras construir el arreglo `pages` es inicializar la lista de páginas libres. Es una lista enlazada cuya cabeza se guarda en la variable estática `page_free_list`. Para construirla, se debe enlazar cada página a la siguiente exceptuando aquellas que ya estén en uso o que nunca se deban usar.

Esta función no modifica el campo `pp_ref` de PageInfo. Simplemente decide, para cada página física, si incluirla en la lista de páginas libres inicial, o ignorarla. En particular, se deben ignorar las páginas que quedaron en uso tras el arranque.

Implementar la función `page_init()`, cuya documentación describe exactamente qué regiones de memoria no se deben incluir en la lista (consultar también el archivo memlayout.h):

- la página 0
- una sección de 384K para I/O
- la región donde se encuentra el código del kernel
- toda la memoria ya asignada por boot_alloc()

#### Tarea: page_alloc

Implementar la función `page_alloc()`, que saca una página de la lista de páginas libres y devuelve su `struct PageInfo` asociado.

Si el argumento a la función incluye `ALLOC_ZERO`, se escriben ceros en toda la longitud de la página.

**Responder**: ¿en qué se diferencia `page2pa()` de `page2kva()`?

#### Tarea: page_free

Implementar la función `page_free()` siguiendo los comentarios en el código.

Para esta primera parte, la función de corrección `check_page_alloc()` debe terminar con éxito:

```console
$ make qemu-nox
Physical memory: 131072K available, base = 640K ...
check_page_alloc() succeeded!
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
```

## Parte 2: Memoria virtual

En esta parte no se realizarán cambios en `mem_init()`, solamente se implementarán funciones para manejar las entradas de page directory y page tables.

#### Tarea: pgdir_walk

La función:

    pte_t *pgdir_walk(pde_t *pgdir, const void *va) ...

se encarga de navegar la estructura de doble nivel page directory → page table entry.

Recibe una dirección virtual más un puntero al comienzo del directorio, y devuelve un puntero a la page table entry correspondiente. Por ejemplo, para la dirección:

    0x4C3C00A  (binario: 0000010011 0000111100 000000001010)
                        ---------- ----------
                        PDE (19)   PTE (60)
devolvería un puntero a la 60.ª posición en la page table a que apunte `pgdir[19]`. (En caso de `pgdir[19]` ser NULL, el parámetro booleano create marcaría si se debe crear la page table correspondiente, o no.)

Ayuda 1: revisar todas las macros disponibles en la primera parte del archivo mmu.h.

Ayuda 2: tanto page directories como page tables almacenan direcciones físicas (ya que es la MMU quien las procesa). La función debe devolver una dirección virtual.

#### Tarea: page_lookup

La función:

```C
struct PageInfo *page_lookup(pde_t *pgdir, void *va) ...
```

devuelve la página física en la que se aloja una dirección virtual. Emplea `pgdir_walk()` para encontrar en qué page table entry se encuentra mapeada la dirección, y dereferencia el puntero devuelto para acceder a su contenido.

De nuevo, revisar las macros en el archivo mmu.h, en particular `PTE_ADDR()`.

#### Tarea: page_insert

La función `page_insert()` configura la relación dirección virtual → dirección física en un determinado directorio.

De manera similar a `page_lookup()`, obtiene con `pgdir_walk()` el PTE donde se debe configurar, y escribe en esa posición el número de página más los permisos apropiados.

Ayuda: el paso de “TLB invalidation” se puede realizar directamente como parte de la función `page_remove()`.

#### Tarea: page_remove

Implementar la función `page_remove()` siguiendo los comentarios en el código.

Para esta segunda parte, la función de corrección `check_page()` debe terminar con éxito:

```console
$ make qemu-nox
Physical memory: 131072K available, base = 640K ...
check_page_alloc() succeeded!
check_page() succeeded!
^^^^^^^^^^^^^^^^^^^^^^^
```

## Parte 3: Page directory del kernel

Al comienzo de `mem_init()` se reserva una página para el page directory del kernel, y se guarda en la variable global `kern_pgdir`. Al final de la función, se carga en %cr3 en sustitución del usado en el proceso de arranque.

Antes de poder usarlo, se le debe añadir las entradas correspondientes a la configuración expresada en memlayout.h para direcciones mayores que `UTOP`.

#### Tarea: boot_map_region

Implementar la función encargada de configurar estas regiones:

```C
static void boot_map_region(pgdir, va, size, pa, int perm) ...
```

Es similar a `page_insert()` en tanto que escribe en el PTE correspondiente, pero:

1. no incrementa el contador de referencias de las páginas
2. puede ser llamada con regiones mucho mayores de 4KiB

#### Tarea: kernel_pgdir_setup

Añadir en `mem_init()` las tres llamadas a `boot_map_region()` necesarias para configurar:

- el stack del kernel en `KSTACKTOP`
- el arreglo pages en `UPAGES`
- los primeros 256 MiB de memoria física en `KERNBASE`

Para esta tercera parte, deben terminar con éxito las funciones de corrección `check_kern_pgdir()` y `check_page_installed_pgdir()`:

```console
$ make qemu-nox
Physical memory: 131072K available, base = 640K ...
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
check_page_installed_pgdir() succeeded!

$ make grade
running JOS: OK (2.1s)
backtrace count: OK
backtrace arguments: OK
backtrace symbols: OK
backtrace lines: OK
Physical page allocator: OK
Page management: OK
Kernel page directory: OK
Page management 2: OK
Large pages: OK (2.4s)
Score: 9/9
```

## Parte 4: Large pages

Usando large pages, se puede mapear 4 MiB de un espacio de direcciones usando un solo PDE —una sola entrada en el page directory— sin necesidad de una page table intermedia. Se debe activar soporte para large pages mediante el registro `%cr4` de la CPU, y poner a 1 el bit `PTE_PS` en el PDE correspondiente.

#### Tarea: entry_pgdir_large

Durante el arranque de JOS, antes de llamar a `i386_init()`, se mapean los primeros 4 MiB de memoria física en las direcciones 0x0 y 0xF0000000. Para ello, se crea un page directory inicial con dos entradas (entry_pgdir) y un page table asociado que lista cada página individual en esos 4 MiB (entry_pgtable). Al tratarse de 4 MiB exactos, no obstante, se puede conseguir el mismo efecto usando tan solo `entry_pgdir`.

Se pide:

1. añadir en entry.S el código necesario para activar soporte de large pages en la CPU; se realiza con el flag CR4_PSE en el registro %cr4. (PSE es el acrónimo de page size extensions.)

2. modificar `entry_pgdir` para que haga uso de “large pages”

3. eliminar el arreglo estático `entry_pgtable`, ya que no debería necesitarse más.

Para no generar un diff innecesariamente grande, se puede omitir de la compilación mediante una instrucción al pre-procesador:

```C
#if 0  // entry_pgtable no longer needed.
pte_t entry_pgtable[NPTENTRIES] = {
    0x000000 | PTE_P | PTE_W,
    ...
    0x3ff000 | PTE_P | PTE_W,
};
#endif
```

#### Tarea: map_region_large

Modificar la función `boot_map_region()` para que use page directory entries de 4 MiB cuando sea apropiado. (En particular, sólo se pueden usar en direcciones alineadas a 22 bits.)

Guardar la implementación con un flag `TP1_PSE`:

```C
static void boot_map_region(...)
{
#ifndef TP1_PSE
    // Código original.
#else
    // Nueva implementación.
#endif
}
```

Responder las siguientes dos preguntas, específicamente en el contexto de JOS:

- ¿cuánta memoria se ahorró de este modo? (en KiB)
- ¿es una cantidad fija, o depende de la memoria física de la computadora?

**Requisitos de la implementación**

La evaluación de esta tarea se realizará conforme a los dos siguientes criterios:

1. Que se comparta la mayor parte de código entre ambas implementaciones (la nueva con PSE activado, y la original sin large pages). Esto supone minimizar la cantidad de código dentro del `#ifndef`, y su `#else`.

    - Ayuda: se puede armar directamente la implementación con este objetivo en mente, o realizar una primera implementación en que se duplique todo el código en ambas ramas, y luego se generalice.
2. Que se usen large pages en todas las ocasiones posibles adentro del rango indicado, y no solo conforme a los valores iniciales de pa y va. (A esto también nos referiremos como “usar large pages oportunísticamente”.)

Esto quiere decir lo siguiente: el uso de large pages en x86 require que tanto dirección física como virtual estén alineadas a 4MiB. Es posible que los valores iniciales de pa y va en la función no estén alineados a 4MiB (y solo a 4KiB). Pero en ese caso, y si el parámetro size es lo suficientemente grande, llegará un momento en que sí lo estén. En ese momento, si la cantidad restante de memoria es mayor o igual a 4MiB, se debe usar una large page.

Ejemplos:

    va=0, pa=0,      size=(4 << 20): 1×4MiB
    va=0, pa=0,      size=(4 << 20 | 4 << 10): 1×4Mib + 1×4KiB
    va=0, pa=0x1000, size=(4 << 20 | 4 << 10): 1025×4KiB

    va=0x1000,   pa=0x1000,   size=(4 << 20): 1024×4KiB
    va=0x1000,   pa=0x1000,   size=(4 << 20 | 4 << 10): 1025×4KiB
    va=0x1000,   pa=0x1000,   size=(8 << 20): 1024×4KiB + 1×4 MiB
    va=0x3ff000, pa=0x3ff000, size=(4 << 20 | 4 << 10): 1×4KiB + 1×4MiB