#ifndef _PREC_TYPE_TABLE_H
#define _PREC_TYPE_TABLE_H
#include <stdlib.h>
#include <string.h>
#include "prec_ast.h"

typedef struct TypeEntry *TypeTablePtr;

struct TypeEntry {
    _Bool top_level;
    char *name;
    struct DeclarationList *regulardata;
    struct DeclarationList *constdata;
    struct TypeEntry *next;
};

static TypeTablePtr new_type_table(void) {
    TypeTablePtr retval = malloc(sizeof(struct TypeEntry));
    retval->name = NULL;
    retval->next = NULL;
    retval->top_level = 0;
    return retval;
}

static void insert_type(TypeTablePtr table, char *name,
    struct DeclarationList *regulardata,
    struct DeclarationList *constdata,
    _Bool top_level) {
    TypeTablePtr curr = table->next;
    table->next = new_type_table();
    table->next->name = name;
    table->next->regulardata = regulardata;
    table->next->constdata = constdata;
    table->next->top_level = top_level;
    table->next->next = curr;
}

static TypeTablePtr fetch_type(TypeTablePtr table, char *name) {
    while (table->next != NULL) {
        if (strcmp(name, table->next->name) == 0)
            return table->next;
        table = table->next;
    }
    return NULL;
}

#endif
