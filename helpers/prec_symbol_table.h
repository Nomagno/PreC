#ifndef _PREC_SYMBOL_TABLE_H
#define _PREC_SYMBOL_TABLE_H
#include <stdlib.h>
#include <string.h>
#include "prec_ast.h"

typedef struct Symbol *SymPtr;

struct Symbol {
    unsigned scope_level; // 0 for global
    char *name;
    struct Type *type;
    struct Symbol *next;
};

static SymPtr new_symbol_table(void) {
    SymPtr retval = malloc(sizeof(struct Symbol));
    retval->name = NULL;
    retval->next = NULL;
    retval->scope_level = 0;
    return retval;
}

static void insert_symbol(SymPtr table, char *name, struct Type *type, unsigned scope_level) {
    SymPtr curr = table->next;
    table->next = new_symbol_table();
    table->next->name = name;
    table->next->type = type;
    table->next->scope_level = scope_level;
    table->next->next = curr;
}

static struct Type *fetch_symbol_type(SymPtr table, char *name) {
    while (table->next != NULL) {
        if (strcmp(name, table->next->name) == 0)
            return table->next->type;
        table = table->next;
    }
    return NULL;
}

static void symbols_cull_scope(SymPtr table, unsigned max_level) {
    while (table->next != NULL) {
        if (table->next->scope_level > max_level) {
            // Delete
            SymPtr tmp = table->next->next;
            free(table->next);
            table->next = tmp;
        } else {
            // Advance
            table = table->next;
        }
    }
}


#endif
