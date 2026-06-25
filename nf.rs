// PreC version of https://github.com/Nomagno/nomforth/blob/master/core/forth.h

#define Cell u32
extern u32 (&)(Cell& i)
    cell_strlen;
extern u32 (&)(u8& i)
    char_strlen;
extern void (&)(mut Cell& dest, u8& src, u32 len)
    cell_char_memcpy;
extern void (&)(mut u8& dest, Cell& src, u32 len)
    char_cell_memcpy;

#define MUNIT 1024
#define MEM_MAX 128*MUNIT


#define CodeType enum codetype
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

#define Ctx struct ctx
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
    mut Cell mut& input_end;
    mut Cell mut& input_start;
    mut Cell mut& input;

    Cell base_ptr;
    Cell exp_ptr;
    Cell compile_state_ptr;
    Cell program_counter_ptr;

    // Memory, immutable pointer to mutable memory cell array of size MEM_MAX
    // There should probs be another immutable variable to give its size
    // instead of being hardcoded in the future
    mut Cell& m;
};


#define forthFunc void (&)(mut Ctx&)
#define PrimitiveData struct primitivedata
struct primitivedata {
    forthFunc func;
    bool priority;
    bool forbid_tco;
    bool allow_interpret;
    u8& name;
};

#define PRIM_NUM 256
extern PrimitiveData[PRIM_NUM] primTable;

extern void (&)(mut Ctx& c, Cell v)
    dataPush;
extern Cell (&)(mut Ctx& c)
    dataPop;
extern Cell (&)(mut Ctx& c)
    dataPeek;
extern void (&)(mut Ctx& c, Cell v)
    funcPush;
extern Cell (&)(mut Ctx& c)
    funcPop;
extern Cell (&)(mut Ctx& c)
    funcPeek;

extern Cell (&)(mut Ctx& c, Cell& s, u32 name_size)
    addToYarnball;
extern void (&)(mut Ctx& c, Cell& name, u32 name_size, bool p, bool forbid_tco, bool forbid_interpreting, Cell& data, u32 data_size)
    makeWord;
extern Cell (&)(mut Ctx& c, Cell& data, Cell data_size)
    appendWord;
extern Cell (&)(mut Ctx& c, Cell& s, u32 s_size)
    findWord;

extern void (&)(mut Ctx& c, Cell id)
    executePrimitive;
extern void (&)(mut Ctx& c, Cell w)
    executeWord;
// Advances the input stream: immutable pointer to (mutable pointer to memory)
extern i32 (&)(Cell mut& & s, Cell& max, u8 target, bool skip_leading)
    consumeWord;
extern i32 (&)(mut Ctx& c, Cell& l, u32 l_size, bool silent)
    interpret;

extern void (&)(mut Ctx& c)
    init;

extern void (&)(mut Ctx& c)
    repl;

extern void (&)(mut Ctx& c)
    initPrimitives;
extern void (&)(mut Ctx& c)
    initConstantWords;
