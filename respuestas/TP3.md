TP3: Multitarea con desalojo
========================

env_return
---------

#### Al terminar un proceso su función umain() ¿dónde retoma la ejecución el kernel? Describir la secuencia de llamadas desde que termina umain() hasta que el kernel dispone del proceso.

La ejecución de `umain()` comienza en `libmain()`, que al finalizar llama a `exit()` devolviéndole el control al kernel.

#### ¿En qué cambia la función env_destroy() en este TP, respecto al TP anterior?

En el TP anterior `env_destroy()` destruía al único environment que se encontraba corriendo, pero en este TP podemos tener múltiples environments en diferentes estados, por lo que ahora es necesario invocar al planificador luego de destruir el environment.

sys_yield
---------

#### Leer y estudiar el código del programa user/yield.c. Cambiar la función `i386_init()` para lanzar tres instancias de dicho programa, y mostrar y explicar la salida de `make qemu-nox`.

```console
SMP: CPU 0 found 1 CPU(s)
enabled interrupts: 1 2
[00000000] new env 00001000
[00000000] new env 00001001
[00000000] new env 00001002
Hello, I am environment 00001000.
Hello, I am environment 00001001.
Hello, I am environment 00001002.
Back in environment 00001000, iteration 0.
Back in environment 00001001, iteration 0.
Back in environment 00001002, iteration 0.
Back in environment 00001000, iteration 1.
Back in environment 00001001, iteration 1.
Back in environment 00001002, iteration 1.
Back in environment 00001000, iteration 2.
Back in environment 00001001, iteration 2.
Back in environment 00001002, iteration 2.
Back in environment 00001000, iteration 3.
Back in environment 00001001, iteration 3.
Back in environment 00001002, iteration 3.
Back in environment 00001000, iteration 4.
All done in environment 00001000.
[00001000] exiting gracefully
[00001000] free env 00001000
Back in environment 00001001, iteration 4.
All done in environment 00001001.
[00001001] exiting gracefully
[00001001] free env 00001001
Back in environment 00001002, iteration 4.
All done in environment 00001002.
[00001002] exiting gracefully
[00001002] free env 00001002
No runnable environments in the system!
```

En yield.c se imprime el environment actual, itera 4 veces llamando al planificador e imprimiendo el environment actual, y finalmente imprime que el environment finalizó.

Observamos que empieza ejecutándose el environment 1, en la primera iteración llama al planificador y comienza a ejecutarse el environment 2, que en el primer llamado al planificador comienza a ejecutarse el environment 3.
En cada iteración el orden es el mismo, env 1, env 2 y env 3, esto es porque el planificador busca el siguiente proceso en estado ENV_RUNNABLE para ser ejecutado.
Finalmente, los 3 environments finalizan su ejecución, también en orden.


envid2env
---------

#### ¿Qué ocurre en JOS si un proceso llama a sys_env_destroy(0)?

Al pasarle 0 como envid, se interpreta que se debe destruir el environment actual, por lo que si un proceso llama a `sys_env_destroy(0)` el mismo terminará su ejecución. Por ejemplo, modificando hello.c de la siguiente manera:

```C
void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	sys_env_destroy(0);
	cprintf("i am environment %08x\n", sys_getenvid());
}
```

```console
[00000000] new env 00001000
hello, world
[00001000] exiting gracefully
[00001000] free env 00001000
No runnable environments in the system!
```

Al correrlo observamos que nunca se llega a imprimir el environment, ya que el proceso finaliza con el llamado a `sys_env_destroy(0)`

dumbfork
---------

#### 1. Si una página no es modificable en el padre ¿lo es en el hijo? En otras palabras: ¿se preserva, en el hijo, el flag de solo-lectura en las páginas copiadas?

No se preserva el estado de solo-lectura, a todas las páginas se les asignan los mismos permisos.

#### 2. Mostrar, con código en espacio de usuario, cómo podría dumbfork() verificar si una dirección en el padre es de solo lectura, de tal manera que pudiera pasar como tercer parámetro a duppage() un booleano llamado readonly que indicase si la página es modificable o no:

```C
envid_t dumbfork(void) {
    // ...
    for (addr = UTEXT; addr < end; addr += PGSIZE) {
        bool readonly = true;
	pde_t pde = uvpd[PDX(addr)];
	if (pde & PTE_P) {
		pte_t pte = uvpt[PGNUM(va)];
		if (pte & PTE_W) 
			readonly = false;
	}
        
        duppage(envid, addr, readonly);
    }
    // ...
```

Ayuda: usar las variables globales uvpd y/o uvpt.

#### 3. Supongamos que se desea actualizar el código de duppage() para tener en cuenta el argumento readonly: si este es verdadero, la página copiada no debe ser modificable en el hijo. Es fácil hacerlo realizando una última llamada a sys_page_map() para eliminar el flag PTE_W en el hijo, cuando corresponda:

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

Esta versión del código, no obstante, incrementa las llamadas al sistema que realiza duppage() de tres, a cuatro. Se pide mostrar una versión en el que se implemente la misma funcionalidad readonly, pero sin usar en ningún caso más de tres llamadas al sistema.

```C
void duppage(envid_t dstenv, void *addr, bool readonly) {
    // Código original (simplificado): tres llamadas al sistema.
    sys_page_alloc(dstenv, addr, PTE_P | PTE_U | PTE_W);
    
    int perms = PTE_P | PTE_U;
    if (!readonly) 
    	perms = perm | PTE_W;
    sys_page_map(dstenv, addr, 0, UTEMP, perms);

    memmove(UTEMP, addr, PGSIZE);
    sys_page_unmap(0, UTEMP);
}
```

multicore_init
---------

#### 1. ¿Qué código copia, y a dónde, la siguiente línea de la función boot_aps()?

```C
memmove(code, mpentry_start, mpentry_end - mpentry_start);
```

La función de `boot_aps()` es iniciar los CPUs de tipo APs, que inicialmente están en modo real. Esta función es llamada por el CPU BSP, que ya tiene todo inicializado, y lo que hace en esa línea es copiar un código que sirva como entry point para los CPUs AP. Este es el código que se encuentra en mpentry.S y es copiado en la dirección MPENTRY_PADDR 
(0x00007000). 

#### 2. ¿Para qué se usa la variable global mpentry_kstack? ¿Qué ocurriría si el espacio para este stack se reservara en el archivo kern/mpentry.S, de manera similar a bootstack en el archivo kern/entry.S?

Esta variable global se usa para guardar la dirección del kernel stack del próximo CPU AP a iniciar. No podría reservarse en mpentry.S ya que cada CPU debe tener su propio stack

#### 3. En el archivo kern/mpentry.S se puede leer:

```C
 # We cannot use kern_pgdir yet because we are still
 # running at a low EIP.
 movl $(RELOC(entry_pgdir)), %eax
```

#### ¿Qué valor tendrá el registro %eip cuando se ejecute esa línea? Responder con redondeo a 12 bits, justificando desde qué región de memoria se está ejecutando este código.

Esa línea corresponde al entry ponit de un CPU AP, que como se mencionó anteriormente está en la dirección MPENTRY_PADDR, por lo que el valor de %eip, redondeando a 12 bits será 0x7000.

ipc_recv
---------

#### Un proceso podría intentar enviar el valor númerico -E_INVAL vía ipc_send(). ¿Cómo es posible distinguir si es un error, o no?

```C
envid_t src = -1;
int r = ipc_recv(&src, 0, NULL);

if (r < 0)
  if (src == 0)
    puts("Hubo error.");
  else
    puts("Valor negativo correcto.")
```

sys_ipc_try_send
---------

A diferencia de sys_ipc_recv(), esta función nunca es bloqueante, es decir, el estado del proceso nunca cambia a ENV_NOT_RUNNABLE. Esto significa, entre otras cosas, que desde userspace se hace necesario llamar a sys_ipc_try_send() en un ciclo, hasta que tenga éxito. Sin embargo, es más eficiente (y por tanto deseable) que el kernel marque al proceso como not runnable, ya que no se necesitaría el ciclo en la biblioteca estándar de JOS.

Se pide ahora explicar cómo se podría implementar una función sys_ipc_send() (con los mismos parámetros que sys_ipc_try_send()) que sea bloqueante, es decir, que si un proceso A la usa para enviar un mensaje a B, pero B no está esperando un mensaje, el proceso A sea puesto en estado ENV_NOT_RUNNABLE, y despertado una vez B llame a ipc_recv() (cuya firma no debe ser cambiada).

Es posible que surjan varias alternativas de implementación; para cada una, indicar:

* qué cambios se necesitan en struct Env para la implementación (campos nuevos, y su tipo; campos cambiados, o eliminados, si los hay)
* qué asignaciones de campos se harían en sys_ipc_send()
* qué código se añadiría en sys_ipc_recv()

Responder, para cada diseño propuesto:

* ¿existe posibilidad de deadlock?
* ¿funciona que varios procesos (A₁, A₂, …) puedan enviar a B, y quedar cada uno bloqueado mientras B no consuma su mensaje? ¿en qué orden despertarían?

Una posible implementación es agregar al struct Env un flag bool env_ipc_sending, como se hizo con `sys_ipc_recv()`. 
De esta manera `sys_ipc_recv()` setearía estee flag y en caso de que el receptor no esté esperando recibir, pondría al environment en estado NOT_RUNNABLE.

`sys_ipc_recv` ahora debería recibir el id del environment del que espera recibir algo, verificar si esta flag está activado y de ser así ponerlo nuevamente en estado RUNNABLE y recibir lo que se envió.

No existe la posibilidad de deadlock porque siempre es uno quien espera al otro, el emisor o el receptor, y la única forma de que se esperen al mismo tiempo sería que uno espere recibir y otro espere enviar al mismo tiempo en distintos CPUs, pero al tratarse de syscalls deben adquirir el lock del kernel antes.

En esta implementación A1, A2, ... quedarían bloqueados hasta que B decida recibir de cada uno. Despertarían en el orden en que B lo decida.
