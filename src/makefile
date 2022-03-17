CC := gcc
all: cat chmod cp echo ls mkdir mv pwd realpath rm whoami shell
cat: cat.o api/entry.o
	gcc cat.o api/entry.o -o cat
chmod: chmod.o api/entry.o
	gcc chmod.o api/entry.o -o chmod
cp: cp.o api/entry.o
	gcc cp.o api/entry.o -o cp
echo:
ls: ls.o api/entry.o
	gcc ls.o api/entry.o -o ls
mkdir: mkdir.o api/entry.o
	gcc mkdir.o api/entry.o -o mkdir
mv: mv.o api/entry.o
	gcc mv.o api/entry.o -o mv
pwd:

realpath: realpath.o api/entry.o
	gcc realpath.o api/entry.o -o realpath
rm: rm.o api/entry.o
	gcc rm.o api/entry.o -o rm
shell: shell.o api/entry.o
	gcc shell.o api/entry.o -o shell
whoami:

clean:
	rm *.o api/*.o
