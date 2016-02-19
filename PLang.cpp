#include <PLang.h>

uint32_t errline;

#define ERROR(C, L) errno = (C); errline = (L);

static inline void trim(char *str) {
    // Right trim
    while ((strlen(str) > 0) && ((str[strlen(str)-1] == ' ') || (str[strlen(str)-1] == '\t') || (str[strlen(str)-1] == '\r') || (str[strlen(str)-1] == '\n'))) {
        str[strlen(str)-1] = 0;
    }

    // Left shift
    while ((strlen(str) > 0) && ((str[0] == ' ') || (str[0] == '\t') || (str[0] == '\r') || (str[0] == '\n'))) {
        for (char *p = str; *p; p++) {
            *p = *(p+1);
        }
    }
}

void PLang::begin() {
}

static inline bool isNumber(const char *c) {
    return (c[0] >= '0' && c[0] <= '9');
}

void PLang::push(op *oc) {
    stack *s = (stack *)malloc(sizeof(stack));
    s->opcode = oc;
    s->next = NULL;
    if (callstack == NULL) {
        callstack = s;
    } else {
        stack *scan = callstack;
        while (scan->next) {
            scan = scan->next;
        }
        scan->next = s;
    }
}

op *PLang::pop() {
    if (callstack == NULL) {
        return NULL;
    }

    if (callstack->next == NULL) {
        op *opcode = callstack->opcode;
        free(callstack);
        callstack = NULL;
        return opcode;
    }

    stack *scan = callstack;;
    stack *last = NULL;
    while (scan->next) {
        last = scan;
        scan = scan->next;
    }
    op *opcode = scan->opcode;
    free(scan);
    if (last == NULL) {
        last->next = NULL;
    }
    return opcode;
}

void PLang::addEvent(uint32_t type, uint32_t source, const char *label, uint32_t line) {
    event *e = (event *)malloc(sizeof(event));
    e->source = source;
    e->type = type;
    e->label = strdup(label);
    e->next = NULL;
    e->current = NULL;
    e->line = line;
    e->last = digitalRead(e->source);
    if (events == NULL) {
        events = e;
    } else {
        event *scan = events;
        while (scan->next) {
            scan = scan->next;
        }
        scan->next = e;
    }
}

void PLang::addOpcode(op *oc) {
    if (program == NULL) {
        program = oc;
    } else {
        op *scan = program;
        while (scan->next) {
            scan = scan->next;
        }
        scan->next = oc;
        // If this happens to have an alternate route set then make that alternate have the
        // next command point to this one as well.  That way IF commands will continue properly.
        // Goto and Call are handled specially elsewhere.
        if (scan->alternate != NULL) {
            scan->alternate->next = oc;
        }
    }
}

variable *PLang::findVariable(char *name) {
    variable *scan = variables;
    while (scan) {
        if (!strcasecmp(scan->name, name)) {
            return scan;
        }
        scan = scan->next;
    }
    return NULL;
}

void PLang::freeop(op *c) {
    if (c->label != NULL) free(c->label);
    if (c->cval1 != NULL) free(c->cval1);
    if (c->cval2 != NULL) free(c->cval2);
    if (c->cval3 != NULL) free(c->cval3);
    if (c->alternate != NULL) freeop(c->alternate);
    free(c);
}

op *PLang::createOpcode(char *label, char *code, char *params, uint32_t line) {
    op *newop = (op *)malloc(sizeof(op));
    newop->opcode = CMD_NOP;
    newop->next = NULL;
    newop->alternate = NULL;
    newop->ival1 = 0;
    newop->ival2 = 0;
    newop->ival3 = 0;
    newop->cval1 = NULL;
    newop->cval2 = NULL;
    newop->cval3 = NULL;
    newop->vval1 = NULL;
    newop->vval2 = NULL;
    newop->vval3 = NULL;
    if (label != NULL) {
        newop->label = strdup(label);
    } else {
        newop->label = NULL;
    }
    newop->line = line;

    if (!strcasecmp(code, "NOP")) {
        return newop;
    }

    if (!strcasecmp(code, "MODE")) {
        char *pin = strtok(params, " \t");
        char *mode = strtok(NULL, " \t");
        char *extra = strtok(NULL, " \t");

        newop->opcode = CMD_MODE;

        if (pin == NULL || mode == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }

        // Is it a number?
        if (isNumber(pin)) {
            newop->opcode |= PT_L;
            newop->ival1 = atoi(pin);
        } else {
            newop->opcode |= PT_V;
            newop->vval1 = findVariable(pin);
            if (newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
        }

        if (!strcasecmp(mode, "IN")) {
            newop->ival2 = 1;
        } else if (!strcasecmp(mode, "OUT")) {
            newop->ival2 = 0;
        } else {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        if (extra != NULL) {
            if (!strcasecmp(extra, "PULLUP")) {
                newop->ival3 = 1;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "IF")) {
        char *left = strtok(params, " \t");
        char *oper = strtok(NULL, " \t");
        char *right = strtok(NULL, " \t");
        char *altop = strtok(NULL, " \t");
        char *altparm = strtok(NULL, "\0");

        newop->opcode = CMD_IF;

        if (altop == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }

        if (isNumber(left) && isNumber(right)) {
            newop->ival1 = atoi(left);
            newop->ival3 = atoi(right);
            newop->opcode |= PT_LL;
        } else if (isNumber(left) && !isNumber(right)) {
            newop->ival1 = atoi(left);
            newop->vval3 = findVariable(right);
            if (newop->vval3 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
            newop->opcode |= PT_LV;
        } else if (!isNumber(left) && !isNumber(right)) {
            newop->vval1 = findVariable(left);
            if (newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
            newop->vval3 = findVariable(right);
            if (newop->vval3 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
            newop->opcode |= PT_VV;
        } else if (!isNumber(left) && isNumber(right)) {
            newop->vval1 = findVariable(left);
            if (newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
            newop->ival3 = atoi(right);
            newop->opcode |= PT_VL;
        } else {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }

        // Operators
        if (!strcasecmp(oper, "GE")) newop->ival2 = (uint32_t)OP_GE;
        else if (!strcasecmp(oper, "GT")) newop->ival2 = (uint32_t)OP_GT;
        else if (!strcasecmp(oper, "LE")) newop->ival2 = (uint32_t)OP_LE;
        else if (!strcasecmp(oper, "LT")) newop->ival2 = (uint32_t)OP_LT;
        else if (!strcasecmp(oper, "EQ")) newop->ival2 = (uint32_t)OP_EQ;
        else if (!strcasecmp(oper, "READS")) newop->ival2 = (uint32_t)OP_READS;
        else {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }

        op *altcmd = createOpcode(NULL, altop, altparm, line);
        if (altcmd == NULL) {
            freeop(newop);
            return NULL;
        }
        newop->alternate = altcmd;
        return newop;
    }

    if (!strcasecmp(code, "CALL")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        // We can't know where the label is yet, so just store the name. We'll convert in the
        // second pass.
        newop->cval1 = strdup(params);
        newop->opcode = CMD_CALL;
        return newop;
    }

    if (!strcasecmp(code, "GOTO")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        // We can't know where the label is yet, so just store the name. We'll convert in the
        // second pass.
        newop->cval1 = strdup(params);
        newop->opcode = CMD_GOTO;
        return newop;
    }

    if (!strcasecmp(code, "PLAY")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        newop->cval1 = strdup(params);
        newop->opcode = CMD_PLAY;
        return newop;
    }

    if (!strcasecmp(code, "PRINT")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        if(params[0] == '"') { // Is it a string?
            newop->cval1 = strdup(params+1);
            if (newop->cval1[strlen(newop->cval1)-1] == '"') {
                newop->cval1[strlen(newop->cval1)-1] = 0;
            }
        } else if (isNumber(params)) {
            newop->ival1 = atoi(params);
        } else {
            newop->vval1 = findVariable(params);
            if (newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
        }
        newop->opcode = CMD_PRINT;
        return newop;
    }

    if (!strcasecmp(code, "PRINTLN")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        if(params[0] == '"') { // Is it a string?
            newop->cval1 = strdup(params+1);
            if (newop->cval1[strlen(newop->cval1)-1] == '"') {
                newop->cval1[strlen(newop->cval1)-1] = 0;
            }
        } else if (isNumber(params)) {
            newop->ival1 = atoi(params);
        } else {
            newop->vval1 = findVariable(params);
            if (newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
        }
        newop->opcode = CMD_PRINTLN;
        return newop;
    }

    if (!strcasecmp(code, "SET")) {
        char *dest = strtok(params, " \t");
        char *src = strtok(NULL, " \t");
        newop->opcode = CMD_SET;
        if (src == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        newop->vval1 = findVariable(dest);
        if (newop->vval1 == NULL) {
            ERROR(ENOENT, line);
            freeop(newop);
            return NULL;
        }
        if (isNumber(src)) {
            newop->ival2 = atoi(src);
            newop->opcode |= PT_L;
        } else {
            newop->vval2 = findVariable(src);
            newop->opcode |= PT_V;
            if (newop->vval2 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "DISPLAY")) {
        newop->opcode = CMD_DISPLAY;
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        if (isNumber(params)) {
            newop->ival1 = atoi(params);
            newop->opcode |= PT_L;
        } else {
            newop->vval1 = findVariable(params);
            newop->opcode |= PT_V;
            if(newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "DELAY")) {
        newop->opcode = CMD_DELAY;
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        if (isNumber(params)) {
            newop->ival1 = atoi(params);
            newop->opcode |= PT_L;
        } else {
            newop->vval1 = findVariable(params);
            newop->opcode |= PT_V;
            if(newop->vval1 == NULL) {
                ERROR(ENOENT, line);
                freeop(newop);
                return NULL;
            }
        }
        return newop;
    }

    if (!strcasecmp(code, "DEC")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        newop->vval1 = findVariable(params);
        newop->opcode = CMD_DEC;
        if(newop->vval1 == NULL) {
            ERROR(ENOENT, line);
            freeop(newop);
            return NULL;
        }
        return newop;
    }

    if (!strcasecmp(code, "INC")) {
        if (params == NULL) {
            ERROR(EINVAL, line);
            freeop(newop);
            return NULL;
        }
        newop->vval1 = findVariable(params);
        newop->opcode = CMD_INC;
        if(newop->vval1 == NULL) {
            ERROR(ENOENT, line);
            freeop(newop);
            return NULL;
        }
        return newop;
    }

    if (!strcasecmp(code, "RETURN")) {
        newop->opcode = CMD_RETURN;
        return newop;
    }

    ERROR(ESRCH, line);

    return NULL;
}

op *PLang::findLabel(const char *label) {
    op *scan = program;
    while (scan) {
        if (scan->label != NULL) {
            if (!strcasecmp(scan->label, label)) {
                return scan;
            }
        }
        scan = scan->next;
    }
    return NULL;
}

// Scan through looking for labels. Update the alternate pointer to the
// destination of the label.
bool PLang::pass2() {
    op *scan = program;
    while (scan) {
        op *what = scan;
        while ((what->opcode & 0xFFF0) == CMD_IF) {
            what = what->alternate;
        }
        if ((what->opcode & 0xFFF0) == CMD_GOTO) {
            op *lab = findLabel(what->cval1);
            if (lab == NULL) {
                ERROR(ENXIO, what->line);
                return false;
            }
            what->alternate = lab;
            free(what->cval1);
        } else if ((what->opcode & 0xFFF0) == CMD_CALL) {
            op *lab = findLabel(what->cval1);
            if (lab == NULL) {
                ERROR(ENXIO, what->line);
                return false;
            }
            what->alternate = lab;
            free(what->cval1);
        } 
        scan = scan->next;
    }

    event *escan = events;
    while (escan) {
        escan->entry = findLabel(escan->label);
        if (escan->entry == NULL) {
            ERROR(ENXIO, escan->line);
            return false;
        }
        //free(escan->label);
        escan = escan->next;
    }
    return true;
}

bool PLang::parse(char *line, uint32_t lineno) {
    char *label = NULL;
    char *opcode = NULL;

    while (strlen(line) > 0 && line[strlen(line)-1] < ' ') {
        line[strlen(line)-1] = 0;
    }
    if (strlen(line) == 0) {
        return true;
    }

    opcode = strtok(line, " \t");
    if (opcode == NULL) {
        return true;
    }
    if (opcode[0] == '#') {
        return true;
    }
    if (opcode[strlen(opcode)-1] == ':') {
        opcode[strlen(opcode)-1] = 0;
        label = opcode;
        opcode = strtok(NULL, " \t");
    }
    // First the def directive. This isn't really a program instruction
    // so we don't want to create an opcode for it.

    if (!strcasecmp(opcode, "DEF")) {
        variable *var = (variable *)malloc(sizeof(variable));
        char *vname = strtok(NULL, " \t");
        if (vname == NULL) {
            ERROR(EINVAL, lineno);
            return false;
        }
        char *defval = strtok(NULL, " \t");
        int dv;
        if (defval != NULL) {
            dv = atoi(defval);
        } else {
            dv = 0;
        }
    
        var->name = strdup(vname);
        var->value = dv;
        var->next = NULL;
        if (variables == NULL) {
            variables = var;
        } else {
            variable *scan = variables;
            while (scan->next) {
                scan = scan->next;
            }
            scan->next = var;
        }
        return true;        
    }

    // Also the LINK command isn't a real command but an instruction
    // to the language to link a specific function to an event.

    if (!strcasecmp(opcode, "LINK")) {
        char *pin = strtok(NULL, " \t");
        char *type = strtok(NULL, " \t");
        char *label = strtok(NULL, " \t");
        uint32_t ntype = 0;
        uint32_t npin = 0;

        if (!strcasecmp(type, "RISING")) {
            ntype = RISING;
        } else if (!strcasecmp(type, "FALLING")) {
            ntype = FALLING;
        } else if (!strcasecmp(type, "CHANGE")) {
            ntype = CHANGE;
        } else {
            ERROR(ESRCH, lineno);
            return false;
        }

        if (isNumber(pin)) {
            npin = atoi(pin);
        } else {
            variable *v = findVariable(pin);
            if (v == NULL) {
                ERROR(ENOENT, lineno);
                return false;
            }
            npin = v->value;
        }

        addEvent(ntype, npin, label, lineno);
        return true;
    }

    op *newop = createOpcode(label, opcode, strtok(NULL, "\0"), lineno);
    if (newop == NULL) {
        return false;
    }
    addOpcode(newop);
    return true;
}

void PLang::exec(op **where) {
    int vars = (*where)->opcode & 0x000F;
    int32_t left;
    int32_t right;
    int mode = 0;

    // Are we in a delay loop at the moment?
    if (((*where)->opcode & 0xFFF0) == CMD_DELAY) {
        if ((*where)->ival2 > 0) { // This is a delay we have started
            uint32_t dellen = vars & PT_V ? (*where)->vval1->value : (*where)->ival1;
            // Still delaying?
            if ((millis() - (*where)->ival2) < dellen) {
                return;
            } else {
                // Delay finished - move on to the next op-code.
                (*where)->ival2 = 0; // Cancel the delay
                *where = (*where)->next;
                return;
            }
        }
    }

    bool result = false;
    switch ((*where)->opcode & 0xFFF0) {
        case CMD_NOP:
            break;
        case CMD_PLAY:
            break;
        case CMD_PRINT:
            if ((*where)->cval1 != NULL) {
                Serial.print((*where)->cval1);
            } else if ((*where)->vval1 != NULL) {
                Serial.print((*where)->vval1->value);
            } else {
                Serial.print((*where)->ival1);
            }
            break;
        case CMD_PRINTLN:
            if ((*where)->cval1 != NULL) {
                Serial.println((*where)->cval1);
            } else if ((*where)->vval1 != NULL) {
                Serial.println((*where)->vval1->value);
            } else {
                Serial.println((*where)->ival1);
            }
            break;
        case CMD_RETURN:
            *where = pop();
            return;
        case CMD_MODE:
            mode = (*where)->ival2;
            if (mode == 1 && (*where)->ival3 == 1) {
                mode = 2; // INPUT_PULLUP
            }
            switch(mode) {
                case 0: pinMode(vars & PT_V ? (*where)->vval1->value : (*where)->ival1, OUTPUT); break;
                case 1: pinMode(vars & PT_V ? (*where)->vval1->value : (*where)->ival1, INPUT); break;
                case 2: pinMode(vars & PT_V ? (*where)->vval1->value : (*where)->ival1, INPUT_PULLUP); break;
            }
            break;
        case CMD_DISPLAY:
            Serial.println(vars == PT_L ? (*where)->ival1 : (*where)->vval1->value);
//            mvprintw(0, 0, "Display: %04d\n",
//                vars == PT_L ? (*where)->ival1 : (*where)->vval1->value);
            break;
        case CMD_IF:
            // Select the operator
            left = vars & PT_VL ? (*where)->vval1->value : (*where)->ival1;
            right = vars & PT_LV ? (*where)->vval3->value : (*where)->ival3;
            switch ((*where)->ival2) {
                case OP_EQ: result = (left == right); break;
                case OP_GE: result = (left >= right); break;
                case OP_GT: result = (left > right); break;
                case OP_LE: result = (left <= right); break;
                case OP_LT: result = (left < right); break;
                case OP_READS: result = (digitalRead(left) == (int)right); break;
                default:
                    ERROR(EINVAL, (*where)->line);
                    break;
            }
            if (result) {
                *where = (*where)->alternate;
                return;
            }
            break;
        case CMD_SET:
            (*where)->vval1->value = (vars & PT_V) ? (*where)->vval2->value : (*where)->ival2;
            break;
        case CMD_CALL:
            push((*where)->next);
            *where = (*where)->alternate;
            return;
        case CMD_GOTO:
            *where = (*where)->alternate;
            return;
        case CMD_DEC:
            (*where)->vval1->value--;
            break;
        case CMD_INC:
            (*where)->vval1->value++;
            break;
        case CMD_DELAY:
            (*where)->ival2 = millis();
            return;
        default: break;
    }
    *where = (*where)->next;
}

void PLang::run() {
    if (!hasInit) {
        if (init == NULL) {
            init = findLabel("init");
        }
        if (init == NULL) {
            // No INIT
            hasInit = true;
            return;
        }
        exec(&init);
        if (init == NULL) {
            // Last command executed
            hasInit = true;
        }
    } else {
        event *scan = events;
        while (scan) {
            if (scan->current != NULL) {
                exec(&(scan->current));
            } else {
                uint32_t n = digitalRead(scan->source);
                if (n != scan->last) {
                    scan->last = n;
                    if (n == 0 && ((scan->type == FALLING) || (scan->type == CHANGE))) {
                        scan->current = scan->entry;
                    } else if (n == 1 && ((scan->type == RISING) || (scan->type == CHANGE))) {
                        scan->current = scan->entry;
                    }
                }
            }
            scan = scan->next;
        }
    }
}

bool PLang::load(const char *fn) {

    int lineno = 0;
    
    File f = SD.open(fn);

    if (!f) {
        ERROR(ENOENT, 0);
        return false;
    }

    char temp[80];
    int tpos = 0;
    int c;
    while ((c = f.read()) > -1) {
        switch (c) {
            case '\r': break;
            case '\n':
                if (!parse(temp, lineno)) {
                    f.close();
                    return false;
                }
                lineno++;
                tpos = 0;
                temp[0] = 0;
                break;
            default:
                temp[tpos++] = c;
                temp[tpos] = 0;
                break;
        };
    }

    f.close();

    if (!pass2()) { 
        return false;
    }
    return true;
}
