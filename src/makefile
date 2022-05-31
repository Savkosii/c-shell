CC := gcc
all: commands/cat commands/chmod commands/cp commands/echo commands/ls commands/mkdir commands/mv commands/pwd commands/realpath commands/rm commands/whoami shell-core/shell
commands/cat: commands/cat.o api/entry.o
	gcc commands/cat.o api/entry.o -o commands/cat
commands/chmod: commands/chmod.o api/entry.o
	gcc commands/chmod.o api/entry.o -o commands/chmod
commands/cp: commands/cp.o api/entry.o
	gcc commands/cp.o api/entry.o -o commands/cp
commands/echo: commands/echo.o
	gcc commands/echo.o -o commands/echo
commands/ls: commands/ls.o api/entry.o
	gcc commands/ls.o api/entry.o -o commands/ls
commands/mkdir: commands/mkdir.o api/entry.o
	gcc commands/mkdir.o api/entry.o -o commands/mkdir
commands/mv: commands/mv.o api/entry.o
	gcc commands/mv.o api/entry.o -o commands/mv
commands/pwd: commands/pwd.o
	gcc commands/pwd.o -o commands/pwd
commands/realpath: commands/realpath.o api/entry.o
	gcc commands/realpath.o api/entry.o -o commands/realpath
commands/rm: commands/rm.o api/entry.o
	gcc commands/rm.o api/entry.o -o commands/rm
shell-core/shell: shell-core/shell.o api/entry.o
	gcc shell-core/shell.o api/entry.o -o shell-core/shell
commands/whoami: commands/whoami.o
	gcc commands/whoami.o -o commands/whoami
clean:
	rm commands/*.o api/*.o shell-core/*.o
