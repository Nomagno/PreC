#ifndef _PREC_SYMBOL_TABLE_H
#define _PREC_SYMBOL_TABLE_H
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "prec_ast.h"

typedef struct Symbol *SymPtr;

struct Symbol {
    bool is_global;
    char *name;
    struct Type *type;
    struct Symbol *next;
};

static SymPtr new_symbol_table(void) {
    SymPtr retval = malloc(sizeof(struct Symbol));
    retval->name = NULL;
    retval->next = NULL;
    retval->is_global = false;
    return retval;
}

static void push_symbol(SymPtr table, char *name, struct Type *type, bool is_global) {
    SymPtr curr = table->next;
    table->next = new_symbol_table();
    table->next->name = name;
    table->next->type = type;
    table->next->is_global = is_global;
    table->next->next = curr;
}

static struct Type *fetch_symbol_type(SymPtr table, char *name) {
    while (table->next != NULL) {
        if (table->next->name != NULL
            && strcmp(name, table->next->name) == 0)
            return table->next->type;
        table = table->next;
    }
    return NULL;
}

#endif
