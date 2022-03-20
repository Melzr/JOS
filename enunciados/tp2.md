# TP2: Procesos de usuario

En este trabajo veremos cómo es la estructura interna que utiliza un sistema operativo para modelar un proceso. También se implementarán los mecanismos de cambio de contexto, tanto desde user space a kernel land, como viceversa.

## Caso de estudio: JOS 

Se cubre la ejecución de un solo proceso, esto es: una vez inicializado el sistema, el kernel lanzará el programa indicado por línea de comandos y, una vez finalizado éste, volverá al monitor de JOS. En los subsiguientes TPs se abordará la ejecución de múltiples programas simultáneamente.

Los programas de usuario se encuentran en el directorio user, y la biblioteca estándar en lib. Por ejemplo, el programa `user/hello.c`:

```C
#include <inc/lib.h>

void umain(int argc, char **argv) {
    cprintf("hello, world\n");
    cprintf("i am environment %08x\n", thisenv->env_id);
}
```

una vez finalizada la parte 4 se va a poder ejecutar mediante `make run-hello-nox`:

```console
$ make run-hello-nox
[00000000] new env 00001000
hello, world
i am environment 00001000
[00001000] exiting gracefully
[00001000] free env 00001000
Destroyed the only environment - nothing more to do!
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K>
```

**Nota**: JOS usa el término environment para referirse a proceso, porque la semántica de los procesos en JOS diverge de la semántica típica en Unix. En esta consigna, se usa “proceso” directamente para environment.

<br/>

## Parte 1: Inicializaciones

En JOS, toda la información de un proceso de usuario se guarda en un `struct Env`, el cual se define en el archivo `inc/env.h`. Env contiene, notablemente, los siguientes campos:

- env_id: identificador numérico del proceso
- env_parent_id: identificador numérico del proceso padre
- env_status: estado del proceso (en ejecución, listo para ejecución, bloqueado…)

Así como:

- env_pgdir: el page directory del proceso
- env_tf: un struct Trapframe (definido en inc/trap.h) donde guardar el estado de la CPU (registros, etc.) cuando se interrumpe la ejecución del proceso. De esta manera, al reanudar el proceso es posible restaurar con exactitud su estado anterior.

La constante `NENV`, por su parte, limita la cantidad máxima de procesos concurrentes en el sistema; el límite actual es 1024. Este límite facilita la creación de procesos de la siguiente manera:

- al arrancar el sistema, se pre-reserva un arreglo de `NENV` elementos `struct Env` (de manera similar al arreglo pages del TP1)
- al crear procesos, no será necesario reservar memoria de manera dinámica, sino que se usan los `struct Env` del arreglo
- el arreglo se configura en una lista enlazada `env_free_list` de la que se puede obtener el siguiente Env libre en O(1). Cuando se destruye un proceso, se reinserta su struct en la lista.

Tanto el arreglo como la lista de procesos libres se definen en `kern/env.c`:

```C
// Arreglo de procesos (variable global, de longitud NENV).
struct Env *envs = NULL;

// Lista enlazada de `struct Env` libres.
static struct Env *env_free_list;

// Proceso actualmente en ejecución (inicialmente NULL).
struct Env *curenv = NULL;
```

#### Tarea: mem_init_envs
Añadir a `mem_init()` código para crear el arreglo de procesos `envs`. Se debe determinar cuánto espacio se necesita, e inicializar a 0 usando `memset()`.

Mapear `envs`, con permiso de sólo lectura para usuarios, en `UENVS` del page directory del kernel.
Tras esta tarea, la función `check_kern_pgdir()` debe reportar éxito:

```C
$ make qemu-nox
Physical memory: 131072K available, base = 640K ...
check_page_alloc() succeeded!
check_page() succeeded!
check_kern_pgdir() succeeded!
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
check_page_installed_pgdir() succeeded!
```

#### Tarea: env_init

Inicializar, en la función `env_init()` de `env.c`, la lista de `struct Env` libres. Para facilitar la corrección automática, se pide que la lista enlazada siga el orden del arreglo (esto es, que `env_free_list` apunte a `&envs[0]`).

#### Tarea: env_alloc

La función `env_alloc()`, ya implementada, encuentra un `struct Env` libre y lo inicializa para su uso. Entre otras cosas:

- le asigna un identificador numérico único
- lo marca como listo para ejecutar (`ENV_RUNNABLE`)
- inicializa sus segmentos de datos y código con permisos adecuados

Se pide leer la función `env_alloc()` en su totalidad y responder las siguientes preguntas:

1. ¿Qué identificadores se asignan a los primeros 5 procesos creados? (Usar base hexadecimal.)
2. Supongamos que al arrancar el kernel se lanzan `NENV` procesos a ejecución. A continuación se destruye el proceso asociado a `envs[630]` y se lanza un proceso que cada segundo muere y se vuelve a lanzar (se destruye, y se vuelve a crear). ¿Qué identificadores tendrán esos procesos en las primeras cinco ejecuciones?

Aclaración: se pide el ID de proceso (env_id) de cada una de las ejecuciones que corresponden a la misma posicion en el `struct Env`.

#### Tarea: env_setup_vm

Desde `env_alloc()` se llama a `env_setup_vm()` (no implementada) para configurar el page directory correspondiente al nuevo proceso. Implementar esta función siguiendo las instrucciones en el código fuente.

Ayuda: es una función muy corta, y apenas le faltan 3 líneas de código por añadir. Se permite usar `memcpy()`.

#### Tarea: env_init_percpu

La función `env_init()` hace una llamada a `env_init_percpu()` para configurar sus segmentos. Antes de ello, se invoca a la instrucción `lgdt`. Responder:

- ¿Cuántos bytes escribe la función `lgdt`, y dónde?
- ¿Qué representan esos bytes?

<br/>

## Parte 2: Carga de ELF

El segundo paso para lanzar un proceso, tras inicializar su `struct Env`, es copiar el código del programa a memoria para que pueda ser ejecutado. Normalmente, el código se carga del sistema de archivos, pero en JOS no tenemos soporte para discos todavía.

Por el momento, para ejecutar un programa en JOS, el linker empotra el código máquina del programa al final de la imagen del kernel. La posición y tamaño de cada programa disponible se marca con símbolos en el binario. Por ejemplo, el código para ejecutar user/hello.c se puede encontrar así:

```console
$ grep user_hello obj/kern/kernel.sym
00008948 A _binary_obj_user_hello_size
f01217f4 D _binary_obj_user_hello_start
f012a13c D _binary_obj_user_hello_end
```

Es decir, 549 KiB a partir de la dirección de enlazado 0xf01217f4. Ahí en realidad se encuentra un archivo ELF (ver `readelf -a obj/user/hello`).

En la parte 3, se lanzará el programa mediante:

```C
// Este símbolo marca el comienzo del ELF user/hello.c.
extern uint8_t _binary_obj_user_hello_start[];

// No es necesario indicar el tamaño; env_create()  lo
// encuentra vía las cabeceras ELF.
env_create(_binary_obj_user_hello_start, ENV_TYPE_USER);
```

O, de manera más sencilla usando la macro `ENV_CREATE`:

```C
ENV_CREATE(user_hello, ENV_TYPE_USER);
```

#### Tarea: region_alloc

Se puede usar `page_insert()` para reservar 4 KiB de memoria en el espacio de memoria de un proceso. Para facilitar la carga del código en memoria, la función auxiliar `region_alloc()` reserva una cantidad arbitraria de memoria.

Se pide la función `region_alloc()` siguiendo las instrucciones en su documentación. Atención a los alineamientos.

Ayuda: usar las funciones `page_alloc()` y `page_insert()`.

#### Tarea: load_icode

La función `load_icode()` recibe un puntero a un binario ELF, y lo carga en el espacio de memoria de un proceso en las direcciones que corresponda. En particular, para cada uno de los e_phnum segmentos o program headers de tipo PT_LOAD:

- reserva memsz bytes de memoria con `region_alloc()` en la dirección va del segmento
- copia filesz bytes desde binary + offset a va
- escribe a 0 el resto de bytes desde va + filesz hasta va + memsz

Se debe, además, configurar el entry point del proceso.

Ayuda: usar las funciones `memcpy()` y `memset()` en el espacio de direcciones del proceso, y la documentación de la función.

<br/>

## Parte 3: Lanzar procesos

Una vez llamado a `env_init()`, el kernel llama a `env_create()` y `env_run()`:

- `env_create()` combina todas las funciones de partes anteriores para dejar el proceso listo para ejecución

- `env_run()` se invoca cada vez que se desea pasar a ejecución un proceso que está listo

#### Tarea: env_create

Implementar la función `env_create()` siguiendo la documentación en el código.

Si, por ejemplo, `env_alloc()` devuelve un código de error, se puede usar el modificador `"%e"` de la función `panic()` para formatear el error:

```C
if (err < 0)
    panic("env_create: %e", err);
```

#### Tarea: env_pop_tf

La función `env_pop_tf()` ya implementada es en JOS el último paso de un context switch a modo usuario. Antes de implementar `env_run()`, responder a las siguientes preguntas:

1. Dada la secuencia de instrucciones assembly en la función, describir qué contiene durante su ejecución:
    - el tope de la pila justo antes popal
    - el tope de la pila justo antes iret
    - el tercer elemento de la pila justo antes de iret

2.¿Cómo determina la CPU (en x86) si hay un cambio de ring (nivel de privilegio)? Ayuda: Responder antes en qué lugar exacto guarda x86 el nivel de privilegio actual. ¿Cuántos bits almacenan ese privilegio?

#### Tarea: env_run

Implementar la función `env_run()` siguiendo las instrucciones en el código fuente. Tras arrancar el kernel, esta función lanza en ejecución el proceso configurado en `i386_init()`.

**Nota**: El programa por omisión, user/hello.c, imprime una cadena en pantalla mediante la llamada al sistema `sys_cputs()`. Al no haber implementado aún soporte para llamadas al sistema, se observará una triple falla en QEMU (o un loop de reboots, según la versión de QEMU). En la tarea a continuación se guía el uso de GDB para averiguar cuándo aborta exactamente el programa.

#### Tarea: gdb_hello

Arrancar el programa hello.c bajo GDB. Se puede usar, en lugar de `make qemu-gdb-nox`:

```console
$ make run-hello-nox-gdb
$ make gdb
```

Se pide mostrar una sesión de GDB con los siguientes pasos:

1. Poner un breakpoint en `env_pop_tf()` y continuar la ejecución hasta allí.

2. En QEMU, entrar en modo monitor (Ctrl-a c), y mostrar las cinco primeras líneas del comando `info registers`.

3. De vuelta a GDB, imprimir el valor del argumento tf:

```console
(gdb) p tf
$1 = ...
```

Ayuda: Es posible que GDB diga que la variable tf no está definida. En ese caso, se recomienda aumentar el nivel de debug en GNUMakefile, usando -ggdb3 en lugar de -gstabs, en todos los lugares que aparece.

Importante: Si se usa -ggdb3 para esta tarea, se debe restaurar la configuración con -gstabs antes de proseguir a la parte 4.

4. Imprimir, con `x/Nx tf` tantos enteros como haya en el struct Trapframe donde `N = sizeof(Trapframe) / sizeof(int)`.

(Se puede calcular a mano afuera de GDB, o mediante el comando: `print sizeof(struct Trapframe) / sizeof(int)`, utilizando ese resultado en `x/Nx tf`)

5. Avanzar hasta justo después del `movl ...,%esp`, usando `si M` para ejecutar tantas instrucciones como sea necesario en un solo paso:

```console
(gdb) disas
(gdb) si M
```

6. Comprobar, con `x/Nx $sp` que los contenidos son los mismos que tf (donde N es el tamaño de tf).

7. Describir cada uno de los valores. Para los valores no nulos, se debe indicar dónde se configuró inicialmente el valor, y qué representa.

8. Continuar hasta la instrucción `iret`, sin llegar a ejecutarla. Mostrar en este punto, de nuevo, las cinco primeras líneas de `info registers` en el monitor de QEMU. Explicar los cambios producidos.

9. Ejecutar la instrucción `iret`. En ese momento se ha realizado el cambio de contexto y los símbolos del kernel ya no son válidos.

    - imprimir el valor del contador de programa con `p $pc` o `p $eip`
    - cargar los símbolos de hello con el comando `add-symbol-file`, así:
    ```console
    (gdb) add-symbol-file obj/user/hello 0x800020
    add symbol table from file "obj/user/hello" at
            .text_addr = 0x800020
    (y or n) y
    Reading symbols from obj/user/hello...
    ```
    - volver a imprimir el valor del contador de programa

Mostrar una última vez la salida de `info registers` en QEMU, y explicar los cambios producidos.

10. Poner un breakpoint temporal (`tbreak`, se aplica una sola vez) en la función `syscall()` y explicar qué ocurre justo tras ejecutar la instrucción `int $0x30`. Usar, de ser necesario, el monitor de QEMU.

<br/>

## Parte 4: Interrupts y syscalls

Una vez lanzado un proceso, este debe poder interaccionar con el sistema operativo para realizar tareas como imprimir por pantalla o leer archivos. Asimismo, el sistema operativo debe estar preparado para manejar excepciones que deriven de la ejecución de un proceso (por ejemplo, si realiza una división por cero o dereferencia un puntero nulo).
    
#### Tarea: kern\_idt

En JOS, todas las excepciones, interrupciones y traps se derivan a la función `trap()`, definida en _trap.c_. Esta función recibe un puntero a un _struct Trapframe_ como parámetro, por lo que cada interrupt handler debe, en cooperación con la CPU, dejar uno en el stack antes de llamar a `trap()`.

Se debe definir ahora en JOS interrupt handlers para todas las interrupciones de la arquitectura x86 (Ver Tabla 6-1 en [Intel 64 and IA-32 Architectures Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), Vol 3A). Esto se realiza en dos partes:

1.  en `trap_init()`, se usará la macro `SETGATE` para configurar la tabla de descriptores de interrupciones (IDT), alojada en el arreglo global `idt[]`.
    
2.  previamente, se debe definir cada interrupt handler en _trapentry.S_. Para no repetir demasiado código, se proporcionan las macros `TRAPHANDLER` y `TRAPHANDLER_NOEC` (leer cuidadosamente su documentación). Los nombres de los interrupt handlers tienen que ser distintos a cualquier función definida ya en JOS, por ejemplo el handler para designar a la interrupción _breakpoint_ no se debería llamar `breakpoint` pues ya existe una función `breakpoint()` definida en el kernel. Un patrón posible puede ser `trap_N`, siendo N el número de la interrupción según la tabla de Intel.
    
    Ambas macros comparten código común en una función `_alltraps`, que se debe implementar también en assembler.
    
    **Ayuda**: cargar GD\_KD en `%ds` y `%es` mediante un registro intermedio de 16 bits (por ejemplo, `%ax`). Considerar, además, que `GD_KD` es una constante numérica, no una dirección de memoria (`‘mov $GD_KD’` vs `‘mov GD_KD’`).
    

Tras esta tarea, deben pasar las siguientes pruebas:

```console
$ make grade
divzero: OK (0.7s)
softint: OK (0.9s)
badsegment: OK (0.9s)
Part A score: 3/3
```

**Responder:**

-   ¿Cómo decidir si usar `TRAPHANDLER` o `TRAPHANDLER_NOEC`? ¿Qué pasaría si se usara solamente la primera?
    
-   ¿Qué cambia, en la invocación de handlers, el segundo parámetro _(istrap)_ de la macro `SETGATE`? ¿Por qué se elegiría un comportamiento u otro durante un syscall?
    
-   Leer _user/softint.c_ y ejecutarlo con `make run-softint-nox`. ¿Qué interrupción trata de generar? ¿Qué interrupción se genera? Si son diferentes a la que invoca el programa… ¿cuál es el mecanismo por el que ocurrió esto, y por qué motivos? ¿Qué modificarían en JOS para cambiar este comportamiento?
    

#### Tarea: kern\_interrupts

Para este TP, se manejan las siguientes dos excepciones: breakpoint (n.º 3) y page fault (n.º 14). El manejo de excepciones se centraliza en la función `trap_dispatch()`, que decide a qué otra función de C invocar según el valor de _tf->tf\_trapno_:

-   para `T_BRKPT` se invoca a `monitor()` con el _Trapframe_ adecuado.
-   para `T_PGFLT` se invoca a `page_fault_handler()` (ya implementado).

Además, la excepción de breakpoint se debe poder lanzar desde programas de usuario. En general, esta excepción se usa para implementar el depurado de código.

Tras esta tarea, los siguientes tests pasan también:

```console
$ make grade
...
faultread: OK (1.0s)
faultreadkernel: OK (1.0s)
faultwrite: OK (1.9s)
faultwritekernel: OK (1.1s)
breakpoint: OK (1.9s)
```

#### Tarea: kern\_syscalls

Hoy en día, la mayoría de sistemas operativos implementan sus _syscalls_ en x86 mediante las instrucciones SYSCALL/SYSRET (64-bits) o SYSENTER/SYSEXIT (32-bits). Tradicionalmente, no obstante, siempre se implementaron mediante una interrupción por software de tipo _trap_.

En JOS, se elige la interrupción n.º 48 (0x30) como slot para `T_SYSCALL`. Tras la implementación de syscalls, el programa _user/hello_ podrá imprimir su mensaje en pantalla.

Pasos a seguir:

1.  Definir un interrupt handler adicional en _trapentry.S_ y configurarlo adecuadamente en `trap_init()`.
    
2.  Invocar desde `trap_dispatch()` a la función `syscall()` definida en _kern/syscall.c_. A la hora de especificar los parámetros de la función, se debe respetar la convención de llamada de JOS para syscalls (leer y estudiar el archivo _lib/syscall.c_).
    
3.  Implementar en `syscall()` soporte para cada tipo de syscall definido en _inc/syscall.h_. Se debe devolver `-E_INVAL` para números de syscall desconocidos.
    
    Nota: solo hace falta despachar, desde `syscall()`, cada tipo a las funciones estáticas ya implementadas: `SYS_cputs` a `sys_cputs()`, `SYS_getenvid` a `sys_getenvid()`, etc.
    

```console
$ make grade
...
testbss: OK (1.0s)
hello: OK (1.0s)
```

**IMPORTANTE**: Aplicar el siguiente cambio a _user/hello.c:_

```
--- user/hello.c
+++ user/hello.c
@@ -5,5 +5,5 @@ void
 umain(int argc, char **argv)
 {
        cprintf("hello, world\n");
-       cprintf("i am environment %08x\n", thisenv->env_id);
+       cprintf("i am environment %08x\n", sys_getenvid());
 }
```

<br/>

## Parte 5: protección de memoria

En la implementación actual de algunas syscalls ¡no se realiza suficiente validación! Por ejemplo, con el código actual es posible que cualquier proceso de usuario acceda (imprima) cualquier dato de la memoria del kernel mediante `sys_cputs()`. Ver, por ejemplo, el programa _user/evilhello.c:_

```C
// Imprime el primer byte del entry point como caracter.
sys_cputs(0xf010000c, 1);
```

#### Tarea: user\_evilhello

Ejecutar el siguiente programa y describir qué ocurre:

```C
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    char *entry = (char *) 0xf010000c;
    char first = *entry;
    sys_cputs(&first, 1);
}
```

Responder las siguientes preguntas:

-   ¿En qué se diferencia el código de la versión en _evilhello.c_ mostrada arriba?
-   ¿En qué cambia el comportamiento durante la ejecución?
    -   ¿Por qué? ¿Cuál es el mecanismo?
-   Listar las direcciones de memoria que se acceden en ambos casos, y en qué _ring_ se realizan. ¿Es esto un problema? ¿Por qué?

#### Tarea: user\_mem\_check

Leer la sección [Page faults and memory protection](https://pdos.csail.mit.edu/6.828/2017/labs/lab3/#Page-faults-and-memory-protection) de la consigna original de 6.828 y completar el ejercicio 9:

1.  Llamar a `panic()` en _trap.c_ si un page fault ocurre en el ring 0.
    
2.  Implementar `user_mem_check()`, previa lectura de `user_mem_assert()` en _kern/pmap.c_.
    
3.  Para cada syscall que lo necesite, invocar a `user_mem_assert()` para verificar las ubicaciones de memoria.
    

```
$ make grade
...
buggyhello: OK (1.1s)
buggyhello2: OK (2.1s)
evilhello: OK (0.9s)
Part B score: 10/10
```