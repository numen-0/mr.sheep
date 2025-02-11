# mr.sheep
```
                                          .-------.               
 _________                                |.      |               
|                                         ||      |               
|  One must not hasten perfection;      .-|_______|-.             
|                                      '.___________.'            
|  even the finest wool takes time        Uuuuuu. |               
                                    |     | . 0 '.'( .  )         
   to weave.                        |  . o`---|--'.   )    )      
                            --------' (     ^()^   )      .  )    
                                       ( .   /\  .       (    )   
                                      (   )  \7 (   ) (     ) )   
                                       GuuUuuuUUuuuUUuuUUuuUUD    
                                        d/       d/  d/    d/     
```

**mr.sheep** is an 8-bit, highly restricted virtual machine, designed to test
the boundaries of a hard environment.

Features:
- 256x8B addressable RAM
- All instructions and values fit into 8-bit signed integers
- VM calls (untested)

## objectives
- [ ] check if `baa` is Turing complete
- [ ] create high-level programming languages that transpile to `wool`, which 
then compiles to `baa` bytecode
- [ ] create esoteric programming languages that transpile to `wool`, which 
then compiles to `baa` bytecode 
- [ ] make `baa` compilable (linux x86)
- [ ] build a `VM` inside the `VM` (brainf*** interpreter doesn't count)

## baa
`baa` is the bytecode used by `mr.sheep`.
features:
- [x] basic input/output capabilities
- [ ] add debugging features
- [ ] multithreading?

### baa: how to use
to transpile a `wool` file into `baa` bytecode, use:
```bash
# Usage: ./mr.sheep <input_baa>
./mr.sheep ./my_code.baa
```

## wool
assembly-like language that directly translates into `baa` bytecode.

features:
- small set of instructions
- instructions directly translate to bytecode
- preprocessor macros (WIP)

to interpret the baa bytecode go [here](#baa-how-to-use).

### wool: how to use
to transpile a `wool` file into `baa` bytecode, use:
```bash
# Usage: ./wool <input_wool> <output_baa>
python ./wool ./examples/wool/hello.wool ./hello.baa
```

## milk (WIP)
simple stack-based language that transpiles directly into `wool`.

## bigBaain
`brainf***` to `wool` transpiler.

features:
- 128 array of memory (due to the mr.sheep memory being 256B)
- Supports all basic [brainf***](https://en.wikipedia.org/wiki/Brainfuck) 
commands and features

### bigBaaain: how to use
to transpile a `brainf***` file into `wool`, use:
```bash
# Usage: python ./src/bigBaaain.py <input_bf> <output_wool>
python ./src/bigBaaain.py ./examples/bigBaaain/helloworld.bf ./helloworld.wool
```

to transpile the wool code go [here](#wool-how-to-use).

## tests
simple test schema:
1. run the program
2. catch output
3. compare it with expected output
4. done

to run the tests, do:
```bash
python ./test.py
```

to update expected output, do:
```bash
python ./test.py -d
```

## license
`TODO`

