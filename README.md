# pyvscc ![license](https://img.shields.io/badge/license-MIT-blue)  <img style="float: right;" src="https://cdn-icons-png.flaticon.com/512/427/427533.png" alt=snake width="48" height="48">

Experimental, no dependencies, python compiler for the x86-64 architecture on linux, utilizing [vscc](https://www.github.com/dmaivel/vscc) for bytecode generation. This project is largely unfinished and is primarly a proof of concept.

## build (cmake)
```bash
git clone --recurse-submodules https://github.com/dmaivel/pyvscc.git
cd pyvscc
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## usage
```
usage: pyvscc [-h] [-i FILE_PATH] [-e ENTRY_POINT] [-m SIZE] [-s SIZE] [-u]

options:
    -h:                  display help information
    -i [FILE_PATH]:      input file path
    -e [ENTRY_POINT]:    specify entry function (if not specified, searches for any function containing 'main')
    -m [SIZE]:           max amount of bytes program may allocate (default: 4098 bytes)
    -s [SIZE]:           amount of bytes variables/functions with an unspecified type take up (default: 8 bytes)
    -u                   unsafe flag, ignores errors, executes first function if main not specified/found
```

## features
### demo
Currently, this compiler is able to compile and execute the following example effortlessly (tabs must be U+0009):
```python
def play(y):
	if y == 3:
		return 11
	return 10

def main():
	x = play(4)
	if x != 11:
		print('hello world!\n')
	return x
```
As for more "complex" demoes, many more features must first be implemented.
### explicit types
The following explicit types may be used: `byte`, `word`, `dword`, and `qword`, each representing a sigle byte, two bytes, four bytes, and eight byte values respectively.
```python
def main(x: qword) -> dword:
    # ...
```