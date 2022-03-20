TP1: Memoria virtual en JOS
===========================

boot_alloc_pos
--------------

> Un cálculo manual de la primera dirección de memoria que devolverá boot_alloc() tras el arranque. Se puede calcular a partir del binario compilado (obj/kern/kernel), usando los comandos readelf y/o nm y operaciones matemáticas.

Utilizando el comando nm podemos inspeccioonar el binario compilado del kernel y obtener los símbolos. Podemos pasarle el parámetro -n para obtenerlos ordenados y así obtener el símbolo más alto en memoria del kernel.

```shell
$ nm -n obj/kern/kernel
0010000c T _start
f010000c T entry
...
f011796c B pages
f0117970 B end
```

El símbolo más alto en memoria es el de end, que es el que se utiliza luego en la función de `boot_alloc` para obtener la primera dirección de la siguiente página disponible. Con este dato se puede calcular la siguiente posición en memoria múltiplo de PGSIZE, que está definida en 4096, que en hexadecimal sería el equivalente a 1000, por ende la primera dirección que debería devolver `boot_alloc` debería ser la siguiente terminada en múltiplos de 1000, `0xf0118000`


> Una sesión de GDB en la que, poniendo un breakpoint en la función boot_alloc(), se muestre el valor devuelto en esa primera llamada, usando el comando GDB finish.

```shell
$ make gdb
gdb -q -s obj/kern/kernel -ex 'target remote 127.0.0.1:26000' -n -x .gdbinit
Reading symbols from obj/kern/kernel...done.
Remote debugging using 127.0.0.1:26000
warning: No executable has been specified and target does not support
determining executable automatically.  Try using the "file" command.
0x0000fff0 in ?? ()
(gdb) break boot_alloc
Breakpoint 1 at 0xf0100a61: file kern/pmap.c, line 89.
(gdb) continue
Continuing.
The target architecture is assumed to be i386
=> 0xf0100a61 <boot_alloc>:     mov    %eax,%edx

Breakpoint 1, boot_alloc (n=981) at kern/pmap.c:89
89      {
(gdb) finish
Run till exit from #0  boot_alloc (n=981) at kern/pmap.c:89
Could not fetch register "orig_eax"; remote failure reply 'E14'
(gdb) p/x $eax
$1 = 0xf0118000
```

page_alloc
----------

> ¿En qué se diferencia page2pa() de page2kva()?

page2pa se encarga de traducir páginas (que moodelamos como un struct PageInfo) a direcciones físicas de memoria, mientras que page2kva se encarga de traducir las mismas páginas a direcciones de memoria virtual del kernel.


map_region_large
----------------

> ¿Cuánta memoria se ahorró de este modo? (en KiB)

Usando large pages podemos mapear 4MiB de un espacio de direcciones únicamente con una entrada en la page directory (una PDE), es decir que no necesitamos la page table intermedia. Esta page table intermedia (PTE) tiene 1024 entradas de 4 bytes, lo que sería el equivalente a ahorrarnos 4096 bytes o 4KiB.


> ¿Es una cantidad fija, o depende de la memoria física de la computadora?

En el caso de JOS es una cantidad fija ya que depende del tamaño de cada página de la page table y este es fijo, de 4KiB.

