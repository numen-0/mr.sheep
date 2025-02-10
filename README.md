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

## baa
`baa` is the bytecode used by `mr.sheep`.
Objectives:
- basic input/output capabilities
- test Turing completeness
- create high-level programing languages that translate to baa bytecode
- make baa compilable
- add debugging features
- ...

## wool
assembly-like language that directly translates into `baa` bytecode.

Features:
- small set of instructions
- instructions directly translate to bytecode
- preprocessor macros (WIP)

## milk (WIP)
simple stack-based language that transpiles directly into `wool`.

## bigBaain (WIP)
`brainf***` to `wool` transpiler.

## tests
simple test schema:
1. run the program
2. catch output
3. compare it with expected output
4. done

do to run tests
```bash
python ./test.py
```

do to update expected output
```bash
python ./test.py -d
```

## license
`TODO`

