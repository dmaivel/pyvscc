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
usage: pyvscc [-h] [-i FILE_PATH] [-e ENTRY_POINT] [-m SIZE] [-s SIZE] [-o] [-p] [-u]

options:
    -h                   display help information
    -i [FILE_PATH]       input file path
    -e [ENTRY_POINT]     specify entry function (if not specified, searches for any function containing 'main')
    -m [SIZE]            max amount of bytes program may allocate (default: 4098 bytes)
    -s [SIZE]            amount of bytes variables/functions with an unspecified type take up (default: 8 bytes)
    -o                   enable optimizations
    -p                   print performance information
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
<details>
<summary>Disassembly of compiled output</summary>
	
```assembly
; pyimpl_strlen(i64)
0:  55                      push   rbp
1:  48 89 e5                mov    rbp,rsp
4:  48 83 ec 11             sub    rsp,0x11
8:  48 89 7d f8             mov    QWORD PTR [rbp-0x8],rdi
c:  48 c7 45 f0 00 00 00    mov    QWORD PTR [rbp-0x10],0x0
13: 00
14: 48 8b 45 f8             mov    rax,QWORD PTR [rbp-0x8]
18: 8a 00                   mov    al,BYTE PTR [rax]
1a: 88 45 ef                mov    BYTE PTR [rbp-0x11],al
1d: 80 7d ef 00             cmp    BYTE PTR [rbp-0x11],0x0
21: 0f 84 0f 00 00 00       je     0x36
27: 48 83 45 f8 01          add    QWORD PTR [rbp-0x8],0x1
2c: 48 83 45 f0 01          add    QWORD PTR [rbp-0x10],0x1
31: e9 de ff ff ff          jmp    0x14
36: 48 8b 45 f0             mov    rax,QWORD PTR [rbp-0x10]
3a: 48 83 c4 11             add    rsp,0x11
3e: 5d                      pop    rbp
3f: c3                      ret
; pyimpl_print_str(i64)
40: 55                      push   rbp
41: 48 89 e5                mov    rbp,rsp
44: 48 83 ec 10             sub    rsp,0x10
48: 48 89 7d f8             mov    QWORD PTR [rbp-0x8],rdi
4c: 48 8b 7d f8             mov    rdi,QWORD PTR [rbp-0x8]
50: e8 ab ff ff ff          call   0x0
55: 48 89 45 f0             mov    QWORD PTR [rbp-0x10],rax
59: 48 c7 c7 01 00 00 00    mov    rdi,0x1
60: 48 8b 75 f8             mov    rsi,QWORD PTR [rbp-0x8]
64: 48 8b 55 f0             mov    rdx,QWORD PTR [rbp-0x10]
68: 48 c7 c0 01 00 00 00    mov    rax,0x1
6f: 0f 05                   syscall
71: 48 8b 45 f0             mov    rax,QWORD PTR [rbp-0x10]
75: 48 83 c4 10             add    rsp,0x10
79: 5d                      pop    rbp
7a: c3                      ret
; play(i64)
7b: 55                      push   rbp
7c: 48 89 e5                mov    rbp,rsp
7f: 48 83 ec 08             sub    rsp,0x8
83: 48 89 7d f8             mov    QWORD PTR [rbp-0x8],rdi
87: 48 83 7d f8 03          cmp    QWORD PTR [rbp-0x8],0x3
8c: 0f 85 0a 00 00 00       jne    0x9c
92: b8 0b 00 00 00          mov    eax,0xb
97: e9 05 00 00 00          jmp    0xa1
9c: b8 0a 00 00 00          mov    eax,0xa
a1: 48 83 c4 08             add    rsp,0x8
a5: 5d                      pop    rbp
a6: c3                      ret
; main()
a7: 55                      push   rbp
a8: 48 89 e5                mov    rbp,rsp
ab: 48 83 ec 18             sub    rsp,0x18
af: 48 c7 c7 04 00 00 00    mov    rdi,0x4
b6: e8 c0 ff ff ff          call   0x7b
bb: 48 89 45 f8             mov    QWORD PTR [rbp-0x8],rax
bf: 48 83 7d f8 0b          cmp    QWORD PTR [rbp-0x8],0xb
c4: 0f 84 18 00 00 00       je     0xe2
ca: 48 8d 05 2b 00 00 00    lea    rax,[rip+0x2b]        # 0xfc
d1: 48 89 45 f0             mov    QWORD PTR [rbp-0x10],rax
d5: 48 8b 7d f0             mov    rdi,QWORD PTR [rbp-0x10]
d9: e8 62 ff ff ff          call   0x40
de: 48 89 45 e8             mov    QWORD PTR [rbp-0x18],rax
e2: 48 8b 45 f8             mov    rax,QWORD PTR [rbp-0x8]
e6: 48 83 c4 18             add    rsp,0x18
ea: 5d                      pop    rbp
eb: c3                      ret
...
```
	
</details>

### explicit types
The following explicit types may be used: `byte`, `word`, `dword`, and `qword`, each representing a sigle byte, two bytes, four bytes, and eight byte values respectively.
```python
def main(x: qword) -> dword:
    # ...
```