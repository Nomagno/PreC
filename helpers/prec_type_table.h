#ifndef _PREC_TYPE_TABLE_H
#define _PREC_TYPE_TABLE_H
#include <stdlib.h>
#include <string.h>
#include "prec_ast.h"

typedef struct TypeEntry *TypeTablePtr;

struct TypeEntry {
    unsigned scope_level; // 0 for global
    char *name;
    struct ConstDeclarationList *regulardata;
    struct ConstDeclarationList *constdata;
    struct TypeEntry *next;
};

static TypeTablePtr new_type_table(void) {
    TypeTablePtr retval = malloc(sizeof(struct TypeEntry));
    retval->name = NULL;
    retval->next = NULL;
    retval->scope_level = 0;
    return retval;
}

static void insert_type(TypeTablePtr table, char *name,
    struct ConstDeclarationList *regulardata,
    struct ConstDeclarationList *constdata,
    unsigned scope_level) {
    TypeTablePtr curr = table->next;
    table->next = new_type_table();
    table->next->name = name;
    table->next->regulardata = regulardata;
    table->next->constdata = constdata;
    table->next->scope_level = scope_level;
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

// No need to cull types based on scope,
// under the reasoning that
// type redefinition is not allowed anyways
// and any constdata will have its scoping done by good old lexical scoping either way
/*
static void types_cull_scope(TypeTablePtr table, unsigned max_level) {
    while (table->next != NULL) {
        if (table->next->scope_level > max_level) {
            // Delete
            TypeTablePtr tmp = table->next->next;
            free(table->next);
            table->next = tmp;
        } else {
            // Advance
            table = table->next;
        }
    }
}
*/


#endif
