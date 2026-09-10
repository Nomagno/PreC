#ifndef _PREC_SYMBOL_TABLE_H
#define _PREC_SYMBOL_TABLE_H
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "prec_ast.h"

typedef struct Symbol *SymPtr;

struct Symbol {
    bool top_level;
    unsigned scope_level;
    char *name;
    struct Type *type;
    struct Symbol *next;
};

static SymPtr new_symbol_table(void) {
    SymPtr retval = malloc(sizeof(struct Symbol));
    retval->name = NULL;
    retval->next = NULL;
    retval->top_level = false;
    retval->scope_level = 0;
    return retval;
}

static void push_symbol(SymPtr table, char *name, struct Type *type, bool top_level, unsigned scope_level) {
    SymPtr curr = table->next;
    table->next = new_symbol_table();
    table->next->name = name;
    table->next->type = type;
    table->next->top_level = top_level;
    table->next->scope_level = scope_level;
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

static void cull_symbols(SymPtr table, unsigned scope_to_delete) {
    assert(scope_to_delete > 0);
    
    while (table->next != NULL) {
        if (table->next->scope_level == scope_to_delete) {
            SymPtr del = table->next;
            table->next = table->next->next;
            free(del);
        } else {
            table = table->next;
        }
    }
}

/*static SymPtr extract_all_nonzero_scope_symbols(SymPtr table) {
    assert(scope_to_delete > 0);
    
    while (table->next != NULL) {
        if (table->next->scope_level == scope_to_delete) {
            SymPtr del = table->next;
            table->next = table->next->next;
            free(del);
        } else {
            table = table->next;
        }
    }
}*/
#endif
