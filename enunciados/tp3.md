​
# TP3: Multitarea con desalojo

En este trabajo, se expande la implementación de procesos de usuario del TP2 para:

1.  Ejecutar de manera concurrente múltiples procesos de usuario. Para ello se implementará un **planificador con desalojo**.
    
2.  Permitir que los procesos de usuario puedan crear, a su vez, nuevos procesos. Para ello se implementará la llamada al sistema **fork()**.
    
3.  Ejecutar múltiples procesos en paralelo, añadiendo **soporte para multi-core**.
    
4.  Permitir que los procesos de usuario puedan comunicarse entre sí mediante un **mecanismo de IPC**.
    
5.  Optimizar la implementación de _fork()_ usando el **mecanismo de COW**.
    

El orden de las tareas es algo distinto a la consigna original _MIT_. El _script_ de auto-corrección sigue esta nueva organización:

```console
$ make grade
helloinit: OK (1.5s)
Part 0 score: 1/1

yield: OK (1.1s)
spin0: Timeout! OK (1.2s)
Part 1 score: 2/2

dumbfork: OK (0.7s)
forktree: OK (1.1s)
spin: OK (1.0s)
Part 2 score: 3/3

yield2: OK (1.1s)
stresssched: OK (1.1s)
Part 3 score: 2/2

sendpage: OK (0.9s)
pingpong: OK (1.0s)
primes: OK (3.6s)
Part 4 score: 3/3

faultread: OK (1.5s)
faultwrite: OK (0.9s)
faultdie: OK (1.9s)
faultregs: OK (2.0s)
faultalloc: OK (1.0s)
faultallocbad: OK (2.0s)
faultnostack: OK (2.2s)
faultbadhandler: OK (2.0s)
faultevilhandler: OK (0.9s)
Part 5 score: 9/9

Score: 20/20
```

<br/>

## Parte 0: Múltiples CPUs

Más adelante en este TP (parte 3) se hará uso de cualquier CPU adicional en el sistema para correr múltiples procesos de usuario en paralelo.

El código base del TP ya incluye la detección e inicialización de todas las CPUs presentes. Ese código se estudiará y completará en tareas posteriores.

Por el momento, es necesario desde ya ajustar ciertas partes de JOS para esta nueva configuración (en particular el _stack_, y algunos _flags_ de la CPU).

**Auto-corrección**

Una vez completadas las tareas de esta parte 0, se debe poder lanzar el programa _hello_ con más de una CPU:

```console
$ make run-hello-nox
...
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
hello, world

$ make run-hello-nox CPUS=4
...
SMP: CPU 0 found 4 CPU(s)
enabled interrupts: 1 2
SMP: CPU 1 starting
SMP: CPU 2 starting
SMP: CPU 3 starting
[00000000] new env 00001000
hello, world
```

Para ello, el código base del curso incluye la siguiente modificación:

```
@@ -60,7 +60,11 @@ i386_init(void)
     // Touch all you want.
     ENV_CREATE(user_hello, ENV_TYPE_USER);
 #endif // TEST*

+    // Eliminar esta llamada una vez completada la parte 1
+    // e implementado sched_yield().
+    env_run(&envs[0]);
+
     // Schedule and run the first user environment!
     sched_yield();
 }
```

#### Tarea: mem\_init\_mp

El _layout_ de memoria de JOS indica que en `KSTACKTOP` se acomoda el _stack_ privado del kernel, y que se pueden emplazar en la misma región _stacks_ adicionales para múltiples CPUs:

```
KERNBASE, ---->  +------------------------------+ 0xf0000000
KSTACKTOP        |     CPU0's Kernel Stack      | RW/--  KSTKSIZE
                 | - - - - - - - - - - - - - - -|
                 |      Invalid Memory          | --/--  KSTKGAP
                 +------------------------------+
                 |     CPU1's Kernel Stack      | RW/--  KSTKSIZE
                 | - - - - - - - - - - - - - - -|
                 |      Invalid Memory          | --/--  KSTKGAP
                 +------------------------------+
                 :              .               :
                 :              .               :
MMIOLIM ------>  +------------------------------+ 0xefc00000
```

El espacio físico para estos _stacks_ adicionales se sigue reservando, como en TPs anteriores, en la sección _BSS_ del binario _ELF_. No obstante, se define directamente desde C, en el archivo _mpconfig.c:_

```C
// La constante NCPU limita el número máximo de
// CPUs que puede manejar JOS; actualmente es 8.
unsigned char percpu_kstacks[NCPU][KSTKSIZE];
```

**Implementar la función `mem_init_mp()` en el archivo _pmap.c_.**

Como parte de la inicialización del sistema de memoria, esta función registra en el _page directory_ el espacio asignado para el _stack_ de cada CPU (desde 0 hasta `NCPU-1`, independiente de que estén presentes en el sistema, o no).

#### Tarea: mpentry\_addr

Como se estudia en la tarea multicore\_init, el arranque de CPUs adicionales necesitará pre-reservar una página de memoria física con dirección conocida. Arbitrariamente, JOS asigna la página n.º 7:

```C
// memlayout.h
#define MPENTRY_PADDR0x7000
```

Se debe actualizar la función `page_init` para no incluir esta página en la lista de páginas libres.

Bonus: se podría añadir un _assert_ exigiendo como pre-condición que `MPENTRY_PADDR` sea realmente la dirección de una página (alineada a 12 bits):

```C
assert(MPENTRY_PADDR % PGSIZE == 0);
```

No obstante, al ser `MPENTRY_PADDR` una constante (y no un parámetro), se puede realizar la comprobación en tiempo de compilación mediante un `static assert` de C11:

```C
_Static_assert(MPENTRY_PADDR % PGSIZE == 0,
               "MPENTRY_PADDR is not page-aligned");
```

#### Tarea: mmio\_map\_region

Implementar la función `mmio_map_region()` en el archivo _pmap.c_.

#### Tarea: mpentry\_cr4

Debido al soporte para _large pages_ implementado en el TP1, se deberá ahora activar la extensión PSE para cada CPU adicional por separado—en el archivo _mpentry.S_.

<br/>​

## Parte 1: Planificador y múltiples procesos

Por el momento, y mientras no exista una función `fork()` o similar, solo es posible crear nuevos procesos desde el kernel. Por ejemplo, en _init.c_:

```C
ENV_CREATE(user_hello, ENV_TYPE_USER);
ENV_CREATE(user_hello, ENV_TYPE_USER);
ENV_CREATE(user_hello, ENV_TYPE_USER);
```

El cambio entre distintos procesos se realiza en la función `sched_yield()`, aún por implementar.

#### Tarea: env\_return

La función `env_run()` de JOS está marcada con el atributo _noreturn_. Esto es, si escribiéramos:

```C
ENV_CREATE(user_hello, ENV_TYPE_USER);
ENV_CREATE(user_hello, ENV_TYPE_USER);

env_run(&envs[0]);
env_run(&envs[1]);
```

se vería que solo el primer proceso entra en ejecución, ya que la segunda llamada a `env_run()` no se llegaría a realizar.

Responder:

-   al terminar un proceso su función `umain()` ¿dónde retoma la ejecución el kernel? Describir la secuencia de llamadas desde que termina `umain()` hasta que el kernel dispone del proceso.
    
-   ¿en qué cambia la función `env_destroy()` en este TP, respecto al TP anterior?
    

#### Tarea: sched\_yield

Implementar la función `sched_yield()`: un planificador round-robin. El cambio de proceso (y de privilegio) se realiza con `env_run()`.

Una vez implementado, se puede eliminar la llamada a `env_run(&envs[0])` en _init.c_, y se debería observar que se ejecutan secuencialmente todos los procesos _hello_ configurados:

```console
$ make qemu-nox
[00000000] new env 00001000
[00000000] new env 00001001
[00000000] new env 00001002
hello, world
i am environment 00001000
[00001000] exiting gracefully
hello, world
i am environment 00001001
[00001001] exiting gracefully
hello, world
i am environment 00001002
[00001002] exiting gracefully
No runnable environments in the system!
```

#### Tarea: sys\_yield

La función `sched_yield()` pertenece al kernel y no es accesible para procesos de usuario. Sin embargo, es útil a veces que los procesos puedan desalojarse a sí mismos de la CPU.

1.  Manejar, en _kern/syscall.c_, la llamada al sistema `SYS_yield`, que simplemente llama a `sched_yield()`. En la biblioteca de usuario de JOS ya se encuentra definida la función correspondiente `sys_yield()`.
    
2.  Leer y estudiar el código del programa _user/yield.c_. Cambiar la función `i386_init()` para lanzar tres instancias de dicho programa, y mostrar y explicar la salida de `make qemu-nox`.
    

#### Tarea: timer\_irq

Para tener un planificador con desalojo, el kernel debe periódicamente retomar el control de la CPU y decidir si es necesario cambiar el proceso en ejecución.

Antes de habilitar ninguna interrupción hardware, se debe definir con precisión la disciplina de interrupciones, esto es: cuándo y dónde se deben habilitar e inhabilitar.

Por ejemplo, _xv6_ admite interrupciones en el _ring_ 0 excepto si se va a adquirir un _lock_. Así, en _xv6_ las llamadas a `acquire()` van precedidas de una instrucción `cli` para inhabilitar interrupciones, y a `release()` le sigue un `sti` para restaurarlas.

En JOS se opta por una disciplina menos eficiente pero mucho más amena: en modo kernel no se acepta _ninguna_ interrupción. Dicho de otro modo: las interrupciones, incluyendo el _timer_, solo se reciben cuando hay un proceso de usuario en la CPU.

**Corolario:** una vez arrancado el sistema, el kernel no llama a `cli` como parte de su operación, pues es una pre-condición que en modo kernel están inhabilitadas.

La habilitación/inhabilitación de excepciones se hace de manera automática:

-   al ir a modo usuario, mediante el registro `%eflags`
-   al volver a modo kernel, mediante el flag  _istrap_ de `SETGATE`.

Tarea :

1.  Leer los primeros párrafos de la sección [Clock Interrupts and Preemption](https://pdos.csail.mit.edu/6.828/2017/labs/lab4/#Clock-Interrupts-and-Preemption) de la consigna original en inglés.
    
2.  Añadir una nueva entrada en el _IDT_ para `IRQ_TIMER`. (Por brevedad, no es necesario hacerlo para el resto de IRQ).
    
3.  Habilitar `FL_IF` durante la creación de nuevos procesos.
    
#### Tarea: timer\_preempt

El _handler_ creado para `IRQ_TIMER` desemboca, como todos los demás, en la función `trap_dispatch()`. A mayor complejidad del sistema operativo, mayor será la lista de cosas para hacer—por ejemplo, verificar si es momento de despertar a algún proceso que previamente llamó a `sleep()`.

En JOS, el manejo de la interrupción del _timer_ será, simplemente, invocar a `sched_yield()`

**No olvidar hacer un _ACK_** de la interrupción del _timer_ mediante la función `lapic_eoi()` antes de llamar al _scheduler_.

**Auto-corrección**: Una vez añadida la llamada a `sched_yield()` en `trap_dispatch()`, todas las pruebas de la parte 1 deberían dar OK (podría ser necesario aumentar el _timeout_ de ejecución del test de _spin0_ para que la prueba se pase correctamente; el _timeout_ se define en la función `test_spin0()` del archivo grade-lab4).

<br/>

## Parte 2: Creación dinámica de procesos

Para crear procesos en tiempo de ejecución es necesario que el kernel proporcione alguna primitiva de creación de procesos tipo `fork()/exec()` en Unix, o `spawn()` en Windows.

En JOS todavía no contamos con sistema de archivos, por lo que se complica implementar una llamada como `exec()` a la que se le debe pasar la ruta de un archivo ejecutable. Por ello, nos limitaremos a implementar una llamada de tipo `fork()`, en su versión “exokernel”. Así, el código a ejecutar en distintos procesos queda contenido en un mismo binario.

#### Tarea: sys\_exofork

En el modelo Unix, `fork()` crea un nuevo proceso (equivalente a `env_alloc()`) y a continuación le realiza una copia del espacio de direcciones del proceso padre. Así, el código sigue su ejecución en ambos procesos, pero cualquier cambio en la memoria del hijo no se ve reflejada en el padre.

En JOS y su modelo _exokernel_, la llamada equivalente `sys_exofork()` sólo realizará la primera parte (la llamada a `env_alloc()`), pero dejará al proceso con un espacio de direcciones sin inicializar.

A cambio, el kernel proporciona un número de llamadas al sistema adicionales para que, desde _userspace_, el proceso padre pueda “preparar” al nuevo proceso (aún vacío) con el código que debe ejecutar.

En esta tarea se debe implementar la llamada al sistema `sys_exofork()`, así como las llamadas adicionales:

-   `sys_env_set_status()`: una vez configurado el nuevo proceso, permite marcarlo como listo para ejecución.
    
-   `sys_page_alloc()`: reserva una página física para el proceso, y la mapea en la dirección especificada.
    
-   `sys_page_map()`: _comparte_ una página entre dos procesos, esto es: añade en el _page directory_ de B una página que ya está mapeada en A.
    
-   `sys_page_unmap()`: elimina una página del _page directory_ de un proceso.
    

Una vez implementada cada una de estas funciones, y como ya se hizo en la tarea sys_yield, se debe manejar el enumerado correspondiente a la función `syscall()` (el síntoma de no hacerlo es el error _invalid parameter_).

Aplica también al resto de tareas del TP: **para cada syscall implementada, se debe actualizar el `switch` de syscalls**.

**Modelo de permisos**: Todas estas funciones se pueden llamar bien para el proceso en curso, bien para un proceso hijo. Para comprender el modelo de permisos de JOS, leer y realizar en paralelo la tarea envid2env.

**Auto-corrección**: Tras esta tarea, `make grade` debe reportar éxito en el test _dumbfork:_

```console
$ make grade
...
Part 1 score: 2/2

dumbfork: OK (0.7s)
...
```

#### Tarea: envid2env

La validación de permisos (esto es, si un proceso puede realizar cambios en otro) se centraliza en la función `envid2env()` que, al mismo tiempo, es la que permite obtener un `struct Env` a partir de un _envid_ (de un modo análogo a `pa2page()` para páginas).

En la llamada al sistema `sys_env_destroy()` (ya implementada) se puede encontrar un ejemplo de su uso:

```C
//
// envid2env() obtiene el struct Env asociado a un id.
// Devuelve 0 si el proceso existe y se tienen permisos,
// -E_BAD_ENV en caso contrario.
//
int envid2env(envid_t envid,
              struct Env **env_store, bool checkperm);

int sys_env_destroy(envid_t envid) {
    int r;
    struct Env *e;

    if ((r = envid2env(envid, &e, 1)) < 0)
        return r;    // No existe envid, o no tenemos permisos.

    env_destroy(e);  // Llamada al kernel directa, no verifica.
    return 0;
}
```

Responder:

-   ¿Qué ocurre en JOS si un proceso llama a `sys_env_destroy(0)`?.

#### Tarea: dumbfork

El programa _user/dumbfork.c_ incluye una implementación de `fork()` altamente ineficiente, pues copia físicamente (página a página) el espacio de memoria de padre a hijo.

Como se verá en la parte 6, la manera eficiente de implementar `fork()` es usando [copy-on-write](https://es.wikipedia.org/wiki/Copy-on-write).

Esta función `dumbfork()` muestra, no obstante, que es posible lanzar procesos dinámicamente desde modo usuario contando tan solo con las primitivas de la sección anterior.

Las ejecución de _dumbfork.c_ una vez lanzado el proceso hijo es similar a _yield.c_, visto con anterioridad: el programa cede el control de la CPU un cierto número de veces, y termina.

Tras leer con atención el código, se pide responder las siguientes preguntas:

1.  Si una página **no** es modificable en el padre ¿lo es en el hijo? En otras palabras: ¿se preserva, en el hijo, el _flag_ de solo-lectura en las páginas copiadas?
    
2.  Mostrar, **con código en espacio de usuario**, cómo podría `dumbfork()` verificar si una dirección en el padre es de solo lectura, de tal manera que pudiera pasar como tercer parámetro a `duppage()` un booleano llamado _readonly_ que indicase si la página es modificable o no:
    
    ```C
    envid_t dumbfork(void) {
        // ...
        for (addr = UTEXT; addr < end; addr += PGSIZE) {
            bool readonly;
            //
            // TAREA: dar valor a la variable readonly
            //
            duppage(envid, addr, readonly);
        }
        // ...
    ```
    
    _Ayuda:_ usar las variables globales `uvpd` y/o `uvpt`.
    
3.  Supongamos que se desea actualizar el código de `duppage()` para tener en cuenta el argumento _readonly:_ si este es verdadero, la página copiada no debe ser modificable en el hijo. Es fácil hacerlo realizando una última llamada a `sys_page_map()` para eliminar el flag `PTE_W` en el hijo, cuando corresponda:
    
    ```C
    void duppage(envid_t dstenv, void *addr, bool readonly) {
        // Código original (simplificado): tres llamadas al sistema.
        sys_page_alloc(dstenv, addr, PTE_P | PTE_U | PTE_W);
        sys_page_map(dstenv, addr, 0, UTEMP, PTE_P | PTE_U | PTE_W);
    
        memmove(UTEMP, addr, PGSIZE);
        sys_page_unmap(0, UTEMP);
    
        // Código nuevo: una llamada al sistema adicional para solo-lectura.
        if (readonly) {
            sys_page_map(dstenv, addr, dstenv, addr, PTE_P | PTE_U);
        }
    }
    ```
    
    Esta versión del código, no obstante, incrementa las llamadas al sistema que realiza `duppage()` de tres, a cuatro. Se pide mostrar una versión en el que se implemente la misma funcionalidad _readonly_, pero sin usar en ningún caso más de tres llamadas al sistema.
    
    _Ayuda:_ Leer con atención la documentación de `sys_page_map()` en _kern/syscall.c_, en particular donde avisa que se devuelve error:
    
    > _if `(perm & PTE_W)` is not zero, but srcva is read-only in srcenvid’s address space._
    

#### Tarea: fork\_v0

Se pide implementar la función `fork_v0()` en el archivo _lib/fork.c_, conforme a las características descritas a continuación. Esta versión actúa de paso intermedio entre `dumbfork()` y la versión final _copy-on-write_.

El comportamiento externo de `fork_v0()` es el mismo que el de `fork()`, devolviendo en el padre el _id_ de proceso creado, y 0 en el hijo. En caso de ocurrir cualquier error, se puede invocar a `panic()`.

El comienzo será parecido a `dumbfork()`:

-   llamar a `sys_exofork()`
-   en el hijo, actualizar la variable global `thisenv`

La copia del espacio de direcciones del padre al hijo difiere de `dumbfork()` de la siguiente manera:

-   se abandona el uso de `end`; en su lugar, se procesan página a página todas las direcciones desde 0 hasta `UTOP`.
    
-   si la página (dirección) está mapeada, se invoca a la función `dup_or_share()`
    
-   la función `dup_or_share()` que se ha de añadir tiene un prototipo similar al de `duppage()`:
    
    ```C
    static void
    dup_or_share(envid_t dstenv, void *va, int perm) ...
    ```
    
    La principal diferencia con la funcion `duppage()` de _user/dumbfork.c_ es: si la página es de solo-lectura, se _comparte_ en lugar de crear una copia.
    
    La principal diferencia con la función `duppage()` que se implementará en la parte 6 (archivo _lib/fork.c_, ejercicio 12 de la consigna original) es: si la página es de escritura, simplemente se crea una copia; todavía no se marca como copy-on-write.
    
    **Nota:** Al recibir _perm_ como parámetro, solo `fork_v0()` necesita consultar _uvpd_ y _uvpt_.
    
    **Ayuda:** Con `pte & PTE_SYSCALL` se limitan los flags a aquellos que `sys_page_map()` acepta.
    

Finalmente, se marca el hijo como `ENV_RUNNABLE`. La función `fork()` se limitará, por el momento, a llamar `fork_v0()`:

```C
envid_t fork(void) {
    // LAB 4: Your code here.
    return fork_v0();
}
```

<br/>

## Parte 3: Ejecución en paralelo (multi-core)

> **Una nota sobre terminología**: JOS usa el término _CPU_ cuando sería más correcto usar _core_, o “unidad de procesamiento”. Una CPU (circuito físico) puede tener más de una de estas unidades. En términos de equivalencia, tanto en JOS como en esta consigna el término “CPU” se puede interpretar indistintamente como “core”, o como “single-core CPU”.

Con lo implementado hasta ahora, si se pasa el parámetro `CPUS=n` a `make qemu-nox`, JOS inicializa correctamente cada _core_, pero todos menos uno entran en un bucle infinito en la función _mp\_main():_

```C
void mp_main(void)
{
  // Cargar kernel page directory.
  lcr3(PADDR(kern_pgdir));

  /* Otras inicializaciones ... */

  // Ciclo infinito. En el futuro, habrá una llamada a
  // sched_yield() para empezar a ejecutar procesos de
  // usuario en CPUs adicionales.
  for (;;)
    ;
}
```

#### Tarea: multicore\_init

En este ejercicio se pide responder las siguientes preguntas sobre cómo se habilita el soporte multi-core en JOS. Para ello se recomienda leer con detenimiento, además del código, la _[Parte A](https://pdos.csail.mit.edu/6.828/2017/labs/lab4/#Part-A--Multiprocessor-Support-and-Cooperative-Multitasking)_ de la consigna original al completo.

Preguntas:

1.  ¿Qué código copia, y a dónde, la siguiente línea de la función _boot\_aps()_?
    
    ```C
     memmove(code, mpentry_start, mpentry_end - mpentry_start);
    ```
    
2.  ¿Para qué se usa la variable global `mpentry_kstack`? ¿Qué ocurriría si el espacio para este _stack_ se reservara en el archivo _kern/mpentry.S_, de manera similar a `bootstack` en el archivo _kern/entry.S_?
    
3.  En el archivo _kern/mpentry.S_ se puede leer:
    
    ```
     # We cannot use kern_pgdir yet because we are still
     # running at a low EIP.
     movl $(RELOC(entry_pgdir)), %eax
    ```
    
    -   ¿Qué valor tendrá el registro _%eip_ cuando se ejecute esa línea?
        
        Responder con redondeo a 12 bits, justificando desde qué región de memoria se está ejecutando este código.
        

#### Tarea: trap\_init\_percpu

Tal y como se implementó en el TP anterior, cuando se pasa de modo usuario a modo kernel, la CPU guarda el _stack_ del proceso y, como medida de seguridad, carga en el registro `%esp` el _stack_ propio del kernel. Tal y como se vio, la instrucción `iret` de `env_pop_tf()` restaura el stack del proceso de usuario.

¿Cómo decide el procesador _qué_ nuevo stack cargar en _%esp_ antes de invocar al _interrupt handler_? 

> If the interrupt handler is going to be executed at higher privilege, a stack switch occurs. \[…\] **The stack pointer for the stack to be used by the handler are obtained from the TSS** for the currently executing task.

El TSS se configura en la función `trap_init_percpu()`, cuya versión del TP2 era:

```C
static struct Taskstate ts;

void trap_init_percpu(void) {
  // Setup a TSS so that we get the right stack
  // when we return to the kernel from userspace.
  ts.ts_esp0 = KSTACKTOP;
  ts.ts_ss0 = GD_KD;

  uint16_t seg = GD_TSS0;
  uint16_t idx = seg >> 3;

  // Plug the the TSS into the global descriptor table.
  gdt[idx] = /* ... */;

  // Load the TSS selector into the task register.
  ltr(seg);
}
```

Si con multi-core no se modificara esta función, ocurriría que el _task register_ de cada core apuntaría al mismo segmento; y, por tanto, ante una interrupción todas las CPUs intentarían usar el mismo stack.

Dicho de otro modo: si en la tarea mem_init_mp se reservó un espacio separado para el stack de cada CPU; y en multicore_init, `boot_ap()` nos aseguramos que cada CPU _arranque_ con el stack adecuado; ahora lo que falta es que ese stack único se siga usando durante el manejo de interrupciones propio a cada CPU.

Para ello, se deberá mejorar la implementación de `trap_init_percpu()` tal que:

-   en lugar de usar la variable global `ts`, se use el campo _cpu\_ts_ del arreglo global `cpus`:
    
    ```C
    // mpconfig.c
    struct CpuInfo cpus[NCPU];
    
    // cpu.h
    struct CpuInfo {
      uint8_t cpu_id;
      volatile unsigned cpu_status;
      struct Env *cpu_env;
      struct Taskstate cpu_ts;
    };
    ```
    
    Para ello basta con eliminar la variable global `ts`, y definirla así dentro de la función:
    
    ```C
    int id = cpunum();
    struct CpuInfo *cpu = &cpus[id];
    struct Taskstate *ts = &cpu->cpu_ts;
    ```
    
-   `GD_TSS0` seguirá siendo el _task segment_ del primer core; para calcular el segmento e índice para cada core adicional, se pueden usar las definiciones:
    
    ```C
    uint16_t idx = (GD_TSS0 >> 3) + id;
    uint16_t seg = idx << 3;
    ```
    
-   el campo `ts->ts_ss0` seguirá apuntando a `GD_KD`; pero `ts->ts_esp0` se deberá inicializar, como en tareas anteriores, de manera dinámica según el valor de `cpunum()`.
    

#### Tarea: kernel\_lock

El manejo de concurrencia y paralelismo _dentro_ del kernel es una de las fuentes más frecuentes de errores de programación.

Según lo explicado en timer_irq, JOS toma la opción fácil respecto a interrupciones y las deshabilita por completo mientras el kernel está en ejecución; ello garantiza que no habrá desalojo de las tareas propias del sistema operativo. Como contraste, se explicó que xv6 permite interrupciones excepto si el código adquirió un lock; y otros sistemas operativos las permiten en todo momento.

Ahora, al introducir soporte multi-core, el kernel debe tener en cuenta que su propio código podría correr en paralelo en más de una CPU. La solución pasa por introducir mecanismos de sincronización.

JOS toma, de nuevo, la opción fácil: además de deshabilitar interrupciones, todo código del kernel en ejecución debe adquirir un lock único: el [big kernel lock](https://en.wikipedia.org/wiki/Big_kernel_lock). De esta manera, el kernel se garantiza que nunca habrá más de una tarea del sistema operativo corriendo en paralelo.

Como contraste, xv6 usa _fine-grained locking_ y tiene locks separados para el scheduler, el sub-sistemas de memoria, consola, etc. Linux y otros sistemas operativos usan, además, [lock-free data structures](https://en.wikipedia.org/wiki/Concurrent_data_structure).

En esta tarea se pide seguir las indicaciones de la consigna original (ejercicio 5) para:

-   en `i386_init()`, adquirir el lock antes de llamar a `boot_aps()`.
-   en `mp_main()`, adquirir el lock antes de llamar a `sched_yield()`.
-   en `trap()`, adquirir el lock **si** se llegó desde userspace.
-   en `env_run()`, liberar el lock justo antes de la llamada a `env_pop_tf()`.

También será necesario modificar el programa de usuario _user/yield.c_ para que imprima el número de _CPU_ en cada iteración. (La macro `thisenv` siempre apunta al proceso que está siendo ejecutado en ese momento, y por ende se puede acceder a los atributos del `struct Env`.)

**Auto-corrección**: Una vez implementado el locking, todas las pruebas de la parte 3 (y anteriores) deberían dar OK:

```console
$ make grade
Part 0 score: 1/1
Part 1 score: 2/2
Part 2 score: 3/3

yield2: OK (1.1s)
stresssched: OK (1.5s)
Part 3 score: 2/2
```

<br/>

## Parte 4: Comunicación entre procesos

En esta parte se implementará un mecanismo comunicación entre procesos. Para evitar mensajes de longitud variable, lo único que se puede enviar es:

-   un entero de 32 bits; y, opcionalmente
-   una página de memoria

Para enviar, un proceso invoca a la función:

```C
void ipc_send(envid_t dst_env, uint32_t val, void *page, int perm);
```

Si el proceso destino existe, esta función sólo devuelve una vez se haya entregado el mensaje.

Los mensajes nunca se entregan de manera asíncrona. Para que se pueda entregar un mensaje, el proceso destino debe invocar a la función:

```C
int32_t ipc_recv(envid_t *src_env, void *page_dest, int *recv_perm);
```

Estas funciones forman parte de la biblioteca del sistema _(lib/ipc.c)_, e invocan a sendas llamadas al sistema que son las que realizan el trabajo (a implementar en _kern/syscall.c_):

-   `sys_ipc_recv()`
-   `sys_ipc_try_send()`

#### Tarea: sys\_ipc\_recv

En este TP, se añadieron al `struct Env` los campos:

```C
struct Env {
  // ...
  bool env_ipc_recving;    // Env is blocked receiving
  void *env_ipc_dstva;     // VA at which to map received page
  uint32_t env_ipc_value;  // Data value sent to us
  envid_t env_ipc_from;    // envid of the sender
  int env_ipc_perm;        // Perm of page mapping received
};
```

El mecanismo de `sys_ipc_recv()` es simple: marcar el proceso como _ENV\_NOT\_RUNNABLE_, poniendo a _true_ el flag `thisenv->env_ipc_recving`. De esta manera:

-   desde `sys_ipc_try_send()` será posible saber si el proceso realmente está esperando un mensaje
    
-   hasta que llegue dicho mensaje, el proceso no entrará en la CPU.
    

#### Tarea: ipc\_recv

La función `ipc_recv()` es el wrapper en _user space_ de `sys_ipc_recv()`. Recibe dos punteros vía los que el proceso puede obtener qué proceso envió el mensaje y, si se envió una página de memoria, los permisos con que fue compartida.

Una vez implementada la función, resolver este ejercicio:

-   Un proceso podría intentar enviar el valor númerico `-E_INVAL` vía `ipc_send()`. ¿Cómo es posible distinguir si es un error, o no?
    
    ```C
    envid_t src = -1;
    int r = ipc_recv(&src, 0, NULL);
    
    if (r < 0)
      if (/* ??? */)
        puts("Hubo error.");
      else
        puts("Valor negativo correcto.")
    ```
    

#### Tarea: ipc\_send

La llamada al sistema `sys_ipc_try_send()` que se implementará en la siguiente tarea es _no bloqueante_, esto es: devuelve inmediatamente con código `-E_IPC_NOT_RECV` si el proceso destino no estaba esperando un mensaje.

Por ello, el wrapper en _user space_ tendrá un ciclo que repetirá la llamada mientras no sea entregado el mensaje.

#### Tarea: sys\_ipc\_try\_send

Implementar la llamada al sistema `sys_ipc_try_send()`, siguiendo los comentarios en el código.

A diferencia de `sys_ipc_recv()`, esta función nunca es bloqueante, es decir, el estado del proceso nunca cambia a `ENV_NOT_RUNNABLE`. Esto significa, entre otras cosas, que desde _userspace_ se hace necesario llamar a `sys_ipc_try_send()` en un ciclo, hasta que tenga éxito. Sin embargo, es más eficiente (y por tanto deseable) que el kernel marque al proceso como _not runnable_, ya que no se necesitaría el ciclo en la biblioteca estándar de JOS.

Se pide ahora explicar cómo se podría implementar una función `sys_ipc_send()` (con los mismos parámetros que `sys_ipc_try_send()`) que sea bloqueante, es decir, que si un proceso A la usa para enviar un mensaje a B, pero B no está esperando un mensaje, el proceso A sea puesto en estado `ENV_NOT_RUNNABLE`, y despertado una vez B llame a `ipc_recv()` (cuya firma _no_ debe ser cambiada).

Es posible que surjan varias alternativas de implementación; para cada una, indicar:

-   qué cambios se necesitan en `struct Env` para la implementación (campos nuevos, y su tipo; campos cambiados, o eliminados, si los hay)
-   qué asignaciones de campos se harían en `sys_ipc_send()`
-   qué código se añadiría en `sys_ipc_recv()`

Responder, para cada diseño propuesto:

-   ¿existe posibilidad de _deadlock?_
-   ¿funciona que varios procesos (A₁, A₂, …) puedan enviar a B, y quedar cada uno bloqueado mientras B no consuma su mensaje? ¿en qué orden despertarían?

```console
$ make grade
...
sendpage: OK (0.6s)
pingpong: OK (1.0s)
primes: OK (4.1s)
Part 4 score: 3/3
```

## Parte 5: Manejo de _page faults_

Una implementación eficiente de `fork()` no copia los contenidos de memoria del padre al hijo sino solamente _el espacio de direcciones_; esta segunda opción es mucho más eficiente pues sólo se han de copiar el _page directory_ y las _page tables_ en uso.

Para las regiones de solo lectura, compartir páginas físicas no supone un problema, pero sí lo hace para las regiones de escritura ya que —por la especificación de `fork()`— las escrituras no deben compartirse entre hijo y padre. Para solucionarlo, se utiliza la técnica de _copy-on-write_, que de forma resumida consiste en:

1.  mapear las páginas de escritura como solo lectura (en ambos procesos)
    
2.  detectar el momento en el que uno de los procesos intenta escribir a una de ellas
    
3.  en ese instante, remplazar —para ese proceso— la página compartida por una página nueva, marcada para escritura, con los contenidos de la antigua.
    

La detección de la escritura del segundo paso se hace mediante el mecanismo estándar de _page faults_. Si se detecta que una escritura fallida ocurrió en una página marcada como _copy-on-write_, el kernel realiza la copia de la página y la ejecución del programa se retoma en la instrucción que generó el error.

No obstante, el `fork()` de JOS se implementa en _userspace_, por lo que los _page faults_ que le sigan deberán ser manejados también desde allí; en otras palabras, será la biblioteca estándar —y no el kernel— quien realice la copia de páginas _copy-on-write_. Para ello, en esta parte del TP se implementará primero un mecanismo de propagación de excepciones de kernel a _userspace_, y después la versión final de `fork()` con _copy-on-write_.

**Nota:** Se recomienda leer al completo el enunciado de esta parte antes de comenzar con su implementación. Una vez hecho esto, el orden de implementación de las tareas se puede alterar más o menos libremente.

#### Tarea: set\_pgfault\_upcall

Desde el lado del kernel, la propagación de excepciones a _userspace_ comienza por saber a qué función de usuario debe saltar el kernel para terminar de manejar la excepción (esta función haría las veces de manejador). Para ello, se añade al struct _Env_ un campo:

```C
struct Env {
    // ...
    void *env_pgfault_upcall;  // Page fault entry point
    // ...
};
```

Para indicar al kernel cuál será el manejador de _page faults_ de un proceso, se introduce la llamada al sistema `sys_env_set_pgfault_upcall()`, que se debe implementar en _kern/syscall.c_ (**no olvidar actualizar el _switch_ al final del archivo**).

**Nota:** El término _upcall_ hace referencia a una llamada que se realiza desde un sistema de bajo nivel a un sistema más alto en la jerarquía. En este caso, una llamada desde el kernel a _userspace_. (Visto así, _upcall_ vendría a ser lo contrario de _syscall_.)

#### Tarea: set\_pgfault\_handler

Cuando el kernel salta al manejador de _userspace_, lo hace en un _stack_ distinto al que estaba usando el programa.

Esto es necesario porque (como se verá) la página fallida por _copy-on-write_ podría ser el propio _stack_ del programa.

Esta “pila de excepciones” se ubica siempre en la dirección fija `UXSTACKTOP`. El kernel de JOS no reserva memoria para esta segunda pila, y los programas que quieran habilitarse un _page fault handler_ deberán obligatoriamente reservar memoria para esa pila antes de llamar a `sys_env_set_pgfault_upcall()`.

Para unificar este proceso, los programas de JOS no llaman directamente a la _syscall_ `sys_env_set_pgfault_upcall()` sino a la función de la biblioteca estándar `set_pgfault_handler()`, la cual reserva memoria para la pila de excepciones antes de instalar el manejador. Se pide implementar esta función en _lib/pgfault.c_.

**Nota:** Como se puede ver en la documentación y el esqueleto de la función, el manejador que se instala no es el que se recibe como parámetro sino que se instala siempre uno común llamado `_pgfault_upcall`. Los motivos se explican en detalle en la siguiente tarea, pgfault\_upcall

#### Tarea: pgfault\_upcall

Un hecho importante del manejador en _userspace_ es que cuando el manejador termina, la ejecución _no_ vuelve al kernel sino que lo hace directamente a la instrucción que originalmente causó la falla. Esto quiere decir que, en el mecanismo de _upcalls_, es el propio código de usuario quien debe asegurarse que la ejecución del manejador es transparente, restaurando para ello los registros a sus valores originales, etc.

Así, y de manera similar a cómo la función `trap()` toma un struct _Trapframe_ como parámetro, el manejador de _page faults_ toma como parámetro un struct _UTrapframe_. En ambos casos, el struct contiene todos los valores que se deberán restaurar:

```C
struct UTrapframe {
    // Descripción del page fault.
    uint32_t utf_fault_va;
    uint32_t utf_err;

    // Estado original a restaurar.
    struct PushRegs utf_regs;
    uintptr_t utf_eip;
    uint32_t utf_eflags;
    uintptr_t utf_esp;
};
```

En el kernel, la secuencia de llamadas para manejar una interrupción es la siguiente:

```
_alltraps  CALL→  trap(tf)  CALL→  env_pop_tf(curenv->tf_eip)
```

Cabe notar que la secuencia no vuelve a `_alltraps` para restaurar el estado sino que el kernel decide salir por otra vía (porque _curenv_ podría haber cambiado). Por otra parte, tanto `_alltraps` como `env_pop_tf` están implementadas en asembler porque es difícil manejar el estado de la CPU (registros, etc.) desde C. Es esta última funcion, `env_pop_tf`, la que restaura el estado del proceso.

En cambio, el manejador de _page faults_ de _userspace_ sí retorna por un flujo de llamadas estándar (pues siempre se vuelve al lugar original):

```
_pgfault_upcall  CALL→  _pgfault_handler(utf)  RET→  _pgfault_upcall
```

Como se vio en la tarea anterior, la función `_pgfault_upcall` es el manejador que siempre se instala en JOS; es una función escrita en asembler que actúa de punto de entrada y salida para cualquier manejador escrito en C. Su esqueleto es:

```
_pgfault_upcall:
    //
    // (1) Preparar argumentos
    //
    ...

    //
    // (2) Llamar al manejador en C, cuya dirección está
    // en la variable global _pgfault_handler.
    //
    ...

    //
    // (3) Restaurar estado, volviendo al EIP y ESP originales.
    //
    ...
```

Se pide implementar esta función en _lib/pfentry.S_. El punto 1 y 2 son sencillos, y el 3 se asemeja en espíritu a la función `env_pop_tf()`; tanto para el punto 2 como el 3 debería haber en el tope de la pila un puntero a _UTrapframe_.

**Nota**: Como se explica en detalle en la siguiente tarea, nada más entrar al manejador ya hay en la pila un struct _UTrapframe_ completo, esto es, en `(%esp)` está el campo _fault\_va_ del struct.

#### Tarea: page\_fault\_handler

Del lado del kernel, la función que invoca al manejador de _page faults_ de _userspace_ es `page_fault_handler()` en _kern/trap.c_; a esta función se la llama desde `trap_dispatch()` —ya vista en TPs anteriores— cuando `tf->tf_trapno` es `T_PGFLT`.

Anteriormente, la implementación de `page_fault_handler()` simplemente abortaba el proceso en que se produjo la falla. Ahora, en cambio, deberá detectar si `env->env_pgfault_upcall` existe y, en ese caso, inicializar en la pila de excepciones del proceso un struct _UTrapframe_ para saltar a continuación allí con el manejador configurado:

```C
void page_fault_handler(struct Trapframe *tf) {
  //
  // ...
  //

  // LAB 4: Your code here.
  if (...) {
    struct UTrapframe *u;

    // Inicializar a la dirección correcta por abajo de UXSTACKTOP.
    // No olvidar llamadas a user_mem_assert().
    u = ...;

    // Completar el UTrapframe, copiando desde "tf".
    u->utf_fault_va = ...;
    ...

    // Cambiar a dónde se va a ejecutar el proceso.
    tf->tf_eip = ...
    tf->tf_esp = ...

    // Saltar.
    env_run(...);
  }

  //
  // ...
  //
}
```

Tras implementar las cuatro tareas anteriores, deberían pasar todas las pruebas de la parte 5:

```console
$ make grade
...
faultread: OK (0.6s)
faultwrite: OK (0.6s)
faultdie: OK (0.8s)
faultregs: OK (1.0s)
faultalloc: OK (1.0s)
faultallocbad: OK (1.0s)
faultnostack: OK (1.0s)
faultbadhandler: OK (1.0s)
faultevilhandler: OK (1.0s)
Part 5 score: 9/9
```

<br/>

## Parte 6: Copy-on-write fork

En esta parte se implementarán las funciones `fork()`, `duppage()` y `pgfault()` en el archivo _lib/fork.c_. Las dos primeras son análogas a `fork_v0()` y `dup_or_share()`; la tercera es el manejador de _page faults_ que se instalará en los procesos que hagan uso de `fork()`.

Se recomienda revisar la explicación de _copy-on-write_ ofrecida en la introducción de la parte 5.

#### Tarea: fork

El cuerpo de la función `fork()` es muy parecido al de `dumbfork()` y `fork_v0()`, vistas anteriormente. Se pide implementar la función `fork()` teniendo en cuenta la siguiente secuencia de tareas:

1.  Instalar, en el padre, la función `pgfault` como manejador de _page faults_. Esto también reservará memoria para su pila de excepciones.
    
2.  Llamar a `sys_exofork()` y manejar el resultado. En el hijo, actualizar como de costumbre la variable global `thisenv`.
    
3.  Reservar memoria para la pila de excepciones del hijo, e instalar su manejador de excepciones.
    
4.  Iterar sobre el espacio de memoria del padre (desde 0 hasta `UTOP`) y, para cada página presente, invocar a la función `duppage()` para mapearla en el hijo. Observaciones:
    
    -   no se debe mapear la región correspondiente a la pila de excepciones; esta región nunca debe ser marcada como _copy-on-write_ pues es la pila donde se manejan las excepciones _copy-on-write_: la región no podría ser manejada sobre sí misma.
        
    -   se pide recorrer el número mínimo de páginas, esto es, no verificar ningún PTE cuyo PDE ya indicó que no hay _page table_ presente (y, desde luego, no volver a verificar ese PDE). Dicho de otra manera, minimizar el número de consultas a _uvpd_ y _uvpt_.
        
5.  Finalizar la ejecución de `fork()` marcando al proceso hijo como `ENV_RUNNABLE`.
    

#### Tarea: duppage

La función `duppage()` conceptualmente similar a `dup_or_share()` pero jamás reserva memoria nueva (no llama a `sys_page_alloc`), ni tampoco la escribe. Así, para las páginas de escritura no creará una copia, sino que la mapeará como solo lectura.

Se pide implementar la función `duppage()` siguiendo la siguiente secuencia:

-   dado un número de página virtual _n_, se debe mapear en el hijo la página física correspondiente en la misma página virtual (usar `sys_page_map`).
    
-   los permisos en el hijo son los de la página original _menos_ `PTE_W`; si y solo si se sacó `PTE_W`, se añade el bit especial `PTE_COW` (ver abajo).
    
-   si los permisos resultantes en el hijo incluyen `PTE_COW`, se debe remapear la página en el padre con estos permisos.

**Sobre el bit PTE\_COW:** Ante un _page fault_, el manejador debe averiguar si el fallo ocurre en una región _copy-on-write_, o si por el contrario se debe a un acceso a memoria incorrecto. En la arquitectura x86, los bits 9-11 de un PTE son ignorados por la MMU, y el sistema operativo los puede usar para sus propósitos. El símbolo `PTE_COW`, con valor 0x800, toma uno de estos bits como mecanismo de discriminación entre errores por acceso incorrecto a memoria, y errores relacionados con _copy-on-write_.

#### Tarea: pgfault

El manejador en _userspace_ `pgfault()` se implementa en dos partes.

En primer lugar, se verifica que la falla recibida se debe a una escritura en una región _copy-on-write_. Así, el manejador llamará a `panic()` en cualquiera de los siguientes casos:

-   el error ocurrió en una dirección no mapeada
-   el error ocurrió por una lectura y no una escritura
-   la página afectada no está marcada como _copy-on-write_

En segundo lugar, se debe crear una copia de la página _copy-on-write_, y mapearla en la misma dirección. Este proceso es similar al realizado por la función `dup_or_share()` para páginas de escritura:

-   se reserva una nueva página en una dirección temporal
-   tras escribir en ella los contenidos apropiados, se mapea en la dirección destino
-   se elimina el mapeo usado en la dirección temporal

**Ayuda para la primera parte:** El error ocurrió por una lectura si el bit `FEC_WR` está a 0 en `utf->utf_err`; la dirección está mapeada si y solo sí el bit `FEC_PR` está a 1. Para verificar `PTE_COW` se debe usar _uvpt_.

___

No hay tests específicos para la parte 6, pero se debe correr `make grade` verificando que todos los tests pasan con esta nueva versión de `fork()`.
