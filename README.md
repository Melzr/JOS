#### Instalación de software necesario:

    sudo apt install make git gdb seabios clang clang-format libbsd-dev \
         gcc-multilib libc6-dev linux-libc-dev qemu-system-x86

#### Compilación y ejecución

La compilación se realiza mediante `make`. En el directorio obj/kern se puede encontrar:

- kernel — el binario ELF con el kernel
- kernel.asm — assembler asociado al binario

Para correr JOS, se puede usar `make qemu` o `make qemu-nox`.

#### Depurado

El Makefile de JOS incluye dos reglas para correr QEMU junto con GDB. En dos terminales distintas:

    $ make qemu-gdb
    ***
    *** Now run 'make gdb'.
    ***
    qemu-system-i386 ...

y:

    $ make gdb
    gdb -q -ex 'target remote ...' -n -x .gdbinit
    Reading symbols from obj/kern/kernel...done.
    Remote debugging using 127.0.0.1:...
    0x0000fff0 in ?? ()
    (gdb)