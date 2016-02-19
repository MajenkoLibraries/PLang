#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <errno_compat.h>
#include <SD.h>

extern uint32_t errline;

typedef enum {
    OP_READS,
    OP_EQ,
    OP_GE,
    OP_GT,
    OP_LE,
    OP_LT
} OPERATOR;

struct variable;

struct op {
    char *label;
    uint32_t opcode;
    struct op *next;
    struct op *alternate;
    int32_t ival1;
    int32_t ival2;
    int32_t ival3;
    char *cval1;
    char *cval2;
    char *cval3;
    struct variable *vval1;
    struct variable *vval2;
    struct variable *vval3;
    uint32_t line;
};

typedef struct op op;

struct event {
    uint32_t type;
    int32_t source;
    char *label;
    op *entry;
    op *current;
    uint32_t last;
    struct event *next;
    uint32_t line;
};

typedef struct event event;

struct variable {
    char *name;
    int32_t value;
    struct variable *next;
};

typedef struct variable variable;

struct stack {
    op *opcode;
    struct stack *next;
};

typedef struct stack stack;

const uint32_t    PT_L           = 0x0000;
const uint32_t    PT_V           = 0x0001;
const uint32_t    PT_LL          = 0x0000;
const uint32_t    PT_LV          = 0x0001;
const uint32_t    PT_VL          = 0x0002;
const uint32_t    PT_VV          = 0x0003;

const uint32_t    CMD_NOP         = 0x0000;
const uint32_t    CMD_MODE        = 0x0010;
const uint32_t    CMD_CALL        = 0x0020;
const uint32_t    CMD_GOTO        = 0x0030;
const uint32_t    CMD_RETURN      = 0x0040;
const uint32_t    CMD_SET         = 0x0050;
const uint32_t    CMD_DISPLAY     = 0x0060;
const uint32_t    CMD_DELAY       = 0x0070;
const uint32_t    CMD_DEC         = 0x0080;
const uint32_t    CMD_INC         = 0x0090;
const uint32_t    CMD_IF          = 0x00A0;
const uint32_t    CMD_PLAY        = 0x00B0;
const uint32_t    CMD_PRINT       = 0x00C0;
const uint32_t    CMD_PRINTLN     = 0x00D0;


class PLang {
    private:
        op *program = NULL;
        variable *variables = NULL;
        event *events = NULL;

        stack *callstack = NULL;

        void push(op *oc);
        op *pop();
        void addEvent(uint32_t type, uint32_t source, const char *label, uint32_t line);
        void addOpcode(op *oc);
        variable *findVariable(char *name);
        void freeop(op *c);
        op *createOpcode(char *label, char *code, char *params, uint32_t line);
        op *findLabel(const char *label);
        void exec(op **where);

        bool hasInit = false;
        op *init = NULL;

    public:
        void begin();
        void run();
        bool load(const char *fn);
        bool pass2();
        bool parse(char *line, uint32_t lineno);
};
