// PreC version of https://github.com/Nomagno/nomforth/blob/master/core/forth.h

#define Cell u32
extern u32 (&)(Cell& i)
    cell_strlen;
extern u32 (&)(u8& i)
    char_strlen;
extern void (&)(Cell mut & dest, u8& src, u32 len)
    cell_char_memcpy;
extern void (&)(u8 mut & dest, Cell& src, u32 len)
    char_cell_memcpy;

#define MUNIT 1024
#define MEM_MAX 128*MUNIT


#define CodeType (enum codetype)
//32 bits
enum codetype {
    t_unknown_label=0,
    t_nop=1,
    t_primitive=2,
    t_num=3,
    t_reljump=4,
    t_reljumpback=5,
    t_condreljump=6,
    t_condreljumpback=7,
    t_absjump=8,
    t_leavelabel=9,
    t_end=10,
    t_end_notailcall=11,
    t_execute=12
};

#define Ctx (struct ctx)
struct ctx {
    Cell dstack_ptr;
    Cell fstack_ptr;
    Cell flags_ptr;
    Cell yarnball_pos_ptr;
    Cell dict_pos_ptr;
    Cell heap_start;
    Cell inbuf_start;

    Cell parsing_vector_start;
    Cell parsing_vector_length;

    // Input stream:
    // Mutable pointers to mutable memory cell array
    Cell mut &mut input_end;
    Cell mut &mut input_start;
    Cell mut &mut input;

    Cell base_ptr;
    Cell exp_ptr;
    Cell compile_state_ptr;
    Cell program_counter_ptr;

    // Memory, immutable pointer to mutable memory cell array of size MEM_MAX
    // There should probs be another immutable variable to give its size
    // instead of being hardcoded in the future
    Cell mut & m;
};


#define forthFunc (void (&)(Ctx mut &))
#define PrimitiveData (struct primitivedata)
struct primitivedata {
    forthFunc func;
    bool priority;
    bool forbid_tco;
    bool allow_interpret;
    u8& name;
};

#define PRIM_NUM 256
extern PrimitiveData[PRIM_NUM] primTable;

extern void (&)(Ctx mut & c, Cell v)
    dataPush;
extern Cell (&)(Ctx mut & c)
    dataPop;
extern Cell (&)(Ctx mut & c)
    dataPeek;
extern void (&)(Ctx mut & c, Cell v)
    funcPush;
extern Cell (&)(Ctx mut & c)
    funcPop;
extern Cell (&)(Ctx mut & c)
    funcPeek;

extern Cell (&)(Ctx mut & c, Cell& s, u32 name_size)
    addToYarnball;
extern void (&)(Ctx mut & c, Cell& name, u32 name_size, bool p, bool forbid_tco, bool forbid_interpreting, Cell& data, u32 data_size)
    makeWord;
extern Cell (&)(Ctx mut & c, Cell& data, Cell data_size)
    appendWord;
extern Cell (&)(Ctx mut & c, Cell& s, u32 s_size)
    findWord;

extern void (&)(Ctx mut & c, Cell id)
    executePrimitive;
extern void (&)(Ctx mut & c, Cell w)
    executeWord;
// Advances the input stream: immutable pointer to (mutable pointer to memory)
extern i32 (&)(Cell & mut & s, Cell& max, u8 target, bool skip_leading)
    consumeWord;
extern i32 (&)(Ctx mut & c, Cell& l, u32 l_size, bool silent)
    interpret;

extern void (&)(Ctx mut & c)
    init;

extern void (&)(Ctx mut & c)
    repl;

extern void (&)(Ctx mut & c)
    initPrimitives;
extern void (&)(Ctx mut & c)
    initConstantWords;
