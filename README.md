## In brief

This project involves a simple program in imitation of GNU bash, plus several handy binary files serving as commands, all of which are fully implemented by C.

## Quick Start

```bash
$ git clone https://github.com/Savkosii/c-shell

$ cd c-shell/src

$ make && make clean

$ ./shell
```

## Features of Shell

### Prompt

Prompt like bash do. The format of the prompt is `[username]@[groupname]:[current working directory][ch]`, where ch is `#` if in root, and `$` otherwise.

What's more, when prompting, the system home directory will be folded into `~`.

### Wildcard path

Support wildcard path.

Example :

```bash
rm foo/* -rf
echo ~/*
cd /hom?
```

### cd

Supports built-in command `cd`, which is equipped to change the current working directory.

Example :

```bash
cd     (equivalent to cd ~)
cd foo
cd ..
cd /../home
cd ~
cd ~/foo
```

Throw exception in the following cases :

```bash
cd foo     (where foo dose not exists)
cd foo     (where foo is not a directory)
cd foo     (where foo is a read-protected directory)
cd foo bar (too much arguments)
cd /*      (too much arguments)
```

### Execute files

Support executing files. The first parameter specifies the path of file, and the others compose the arguments of the program.

Note that although command actually is file in essence, for convenience we distinguish them. If one string does not contain `/`, we regard it as command, otherwise we regard it as the path of file.

Example :

```bash
./ls    -l
/bin/sh -c whoami
```
Throw exception in the following cases :
```bash
foo    (where command foo is not found)
./foo  (where file foo does not exist)
```
### Execute multiple commands

Supports commands delimiters, i.e.  `&&` and `;`. This enables shell to execute multiple commands in one line. 

For example, `cd foo && ls`  will tell the shell to switch the working directory into directory `foo`, and execute command `ls` then.

Note that it is illegal for the line to start with delimiter `;` or `&&`,  or lack non-space char between two same delimiters, which will trigger syntax error.

Fortunately, it is allowed for the line to end with delimiter `;` or `&&`, though in the latter case the commands in line are regarded as incomplete, and thus will cause the shell to read next line from stdin, the act of which will continue until shell gets the complete commands.

### File stream redirect

Supports file stream redirect, with overwrite stream sign `>` or append stream sign `>>`, the string after which specifies the target file.

Take commands `ls > foo`, it will open file `./foo` (which will be created if not existent) before executing command `ls`, and the output of `ls` will be writen to file `./foo` instead of stdout. 

Note that if there is no output to stdout, e.g. ` > foo`, shell will create an empty file then. 

Moreover, if there already exists directory `./foo` or write-protected file `./foo` , shell will not enable the redirect and throw exception then. 

Also be aware that if there is a lack of file path after the redirect stream sign, shell will read one additional line from stdin and you can append the path then.

### Reading multiple lines

Supports reading multiple lines from stdin, with input-lines sign `<<`, the string after which specifies the end-input-flag string.

For example, `cat << foo` will trigger input from stdin, where you can write arbitrary numbers of lines unless you input `foo\n`. 

And like the case where the commands end with redirect stream sign, if there is a lack of end-input-flag string, shell will read one additional line from stdin and you can append it then.

### Pipes

Support commands with pipes. When you split commands with delimiter `|` (its syntax is similar to that of `&&`), the output or input or both related to the commands will be redirected.

Take commands `echo foo | cat -b | cat -e ` , shell will first execute command `echo foo` and redirect its output to stdin, and the command `cat-b` will read from stdin and redirect its output to stdin, and the command `cat -e` will also read from stdin but output to stdout as normal. Thus the output is `1  foo$` .

Note that if you use read-multiple-lines sign in command that equipped with pipes, shell will omit the commands before it, e.g. `ls | cat -e | cat -b << foo | cat -T` , shell will only execute `cat -b << foo | cat -T`.

## Favorable Commands

### cat

Read from stream, stdin in default.

Favorable options :

```bash
-b,    --number-nonblank  print number of non-empty output lines
-n,    --number           print number of all output lines
-e,-E, --show-ends        display '$' at the end of each line
-t,-T  --show-tabs        display TAB characters as '^I'
-A     --show-all         equivalent to -ET
```

Example :

```bash
cat                 (read from stdin; use EOF to stop input)
cat -               (read from stdin; use EOF to stop input)
cat foo             (read from file stream)
cat foo bar ... baz (read from arbitrary numbers of file stream)
cat > flag          (read from stdin and redirect output to file "flag")
cat << foo > flag   (read multiple lines from stdin and redirect output to file "flag")
```

Throw exception in the following cases :

```bash
cat foo    (where foo does not exist)
cat foo    (where foo is a directory)
cat foo    (where foo is a read-protected file)
```

### chmod

Change files permission mode bits. You can use number (ranging from 000 to 777) to specify the mode bits or use option `-u=` to do so.

There are three digits in number that serves as mode bits (if not, we will add zero in front of it as complement). The first digit represents the Owner permission, with the second one representing the Group permission, and the third one representing the Others permission. Each digit can be obtained though a linear combination of {4, 2, 1}, where "4" represents "read", and "2" represents "write", and "1" represents "execute".

Favorable option :

```bash
-u=[rwx]
```

Example :

```bash
chmod 0          foo
chmod 000        foo
chmod 777        foo
chmod 775        foo
chmod 77         foo
chmod -u=        foo
chmod -u=rwx     foo
chmod -u= -u=rwx foo 
```

Throw exception in the following cases :

```bash
chmod         (missing operand)
chmod 888 foo (invalid mode)
chmod 74a foo (invalid mode)
chmod -u  foo (missing operand)
```

Note :

You can use command `ls -l` to check the permission information of file

### cp

Copy files or directories.

Favorable option :

```bash
-r   --recursively  copy directories recursively (neccesary when copying directory)
-i   --interactive  before overwrite
```

Example :

```bash
#Suppose that foo is a file and bar is a directory:
cp foo bar 
cp foo bar/foo
cp foo bar/baz      (where bar/baz does not exist)
cp foo baz          (where baz does not exist)

#Suppose that foo is a directory and bar is also a directory:
cp foo bar     -r
cp foo bar/foo -r
cp foo bar/baz -r   (where bar/baz does not exist)
cp foo baz     -r   (where baz does not exist)
cp foo bar     -r   (where foo contains write-protected file or directory)
```

Throw exception in the following cases :

```bash
cp                     (missing operand)
cp foo                 (missing operand)
cp foo bar          -r (where foo or bar does not exists)
cp foo .            -r (is the same file)
cp foo bar          -r (where foo is read-protected)
cp foo bar          -r (where foo contains read-protected file or directory)
cp foo bar/baz/foo  -r (where bar/baz does not exists)
cp foo foo/bar      -r (cannot copy directory into the subdirectory of itself)

#Suppose that foo is a file and bar is a directory
cp foo bar             (where bar is write-protected)
cp foo bar             (where bar contains a directory named "foo")
cp foo bar             (where bar contains a write-protected file named "foo")

#Suppose that foo is a directory
cp foo bar             (-r is not specified)
cp foo bar          -r (where bar is a file)
cp foo bar/barz/bar -r (where bar exists but bar/barz is not a directory)
cp foo bar          -r (where bar contains a write-protected directory named "foo")
cp foo bar          -r (where bar contains a file named "foo")
```

### echo

Output specified path to stdout (does not check whether the path exists)

Example :

```bash
echo     (output a '\n')
echo foo
echo ~
echo /*
echo ~ >> flag
```

### ls

List existent files or entries in directories and output the result to stdout.

Favorable option :

```bash
-a, --all    do not ignore entries starting with '.'
-l           list details of entries
-p,          append '/' to directories
```

Example :

```bash
ls             (list '.' in defalt)
ls -l  .
ls -la ..
ls -l  foo     (where foo is a directory)
ls -l  foo     (where foo is a file)
ls -l  foo bar (where foo is a file a bar is a directory)
ls -l  *
```

Throw exception in the following cases :

```bash
ls foo  (where foo does not exists)
ls foo  (where foo is read-protected file)
ls foo  (where foo is read-protected directory)
```

### mkdir

Make directory.

Favorable option :

```bash
-m=, --mode=     set file mode (as in chmod)
-p,  --parents   make parent directories as needed
```

Example :

```bash
mkdir foo               (equivalent to -m=rwx)
mkdir foo -m=rx
mkdir foo -m=777
mkdir foo -m=0
mkdir foo -m=           (equivalent to -m=0)
mkdir foo/bar/baz       (where foo/bar exists)
mkdir foo/bar/baz -p    (where foo/bar does not exists)
mkdir foo/bar/baz -m=w -p
```

Throw exception in the following cases :

```bash
mkdir                (missing operand)
mkdir foo            (where foo already exists)
mkdir foo/bar/baz    (where foo/bar does not exist)
mkdir foo/bar/baz -p (where foo/bar is a write-protected directory)
mkdir foo/bar/baz -p (where foo/bar/baz already exists)
mkdir foo/bar/baz -p (where foo/bar is a file)
```

### mv

Change files or directories path (filename can be changed at the same time)

Favorable option :

```bash
-i, --interactive     prompt before overwrite
-f, --force           do not prompt before overwriting
```

Example :

```bash
#Suppose that bar is a directory:
mv foo bar 
mv foo bar/foo
mv foo bar/baz  (where bar/baz does not exist)
mv foo baz      (where baz does not exist)
mv foo bar      (where bar contains a empty directory named "foo")
```

Throw exception in the following cases :

```bash
mv                 (missing operand)
mv foo             (missing operand)
mv foo bar         (where foo or bar does not exists)
mv foo bar         (where foo is write-protected)
mv foo bar         (where bar is write-protected)
mv foo .           (is the same file)
mv foo bar/baz/foo (where bar/barz does not exists)
mv ./ ./foo        (cannot move directory into the subdirectory of itself)
mv ../ ./foo       (cannot move directory into the subdirectory of itself)

#Suppose that foo is a file and bar is a directory
mv foo bar         (where bar is a directory)
mv foo bar         (where bar contains a directory named "foo")
mv foo bar         (where bar contains a write-protected file named "foo")

#Suppose that foo is a directory
mv foo bar         (where bar is not a directory)
mv foo bar         (where bar contains a non-empty directory named "foo")
mv foo bar         (where bar contains a write-protected directory named "foo")
```

### pwd

Print the current working directory (in realpath) to stdout.

### realpath

Get the realpath of specified path and output it to stdout. In default, there is no need for the path to exist.

Favorable option :

```bash
-e, --canonicalize-existing  the path must exist
-m, --canonicalize-missing   overrding -e
```

Example :

```bash
realpath /*
realpath ~
```

### rm

Remove files or directory.

Favorable option :

```bash
-d  -dir              remove empty directory
-r, --recursive       remove directories recursively
-f, --force           ignore nonexistent files and arguments, never prompt
-i                    prompt before every removal
```

Example :

```bash
rm    foo   (where foo is a file)
rm -d foo   (where foo is an empty directory)
rm -r foo   (where foo is a non-empty directory)
rm    foo/* (where foo is a non-empty directory containing only files)
```

Throw exception in the following cases :

```bash
rm  .   -r (cannot remove directory from the subdirectory of itself)
rm  ../ -r (cannot remove directory from the subdirectory of itself)
rm  foo -r (where foo is write-protected)
rm  foo -r (where foo contains write-protected file or directory)
```

### whoami

Print the username to stdout.

