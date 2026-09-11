#ifndef _PREC_TYPE_TABLE_H
#define _PREC_TYPE_TABLE_H
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "prec_ast.h"

typedef struct TypeEntry *TypeTablePtr;

struct TypeEntry {
    bool top_level;
    unsigned scope_level;
    char *name;
    struct DeclarationList *regulardata;
    struct DeclarationList *constdata;
    struct TypeEntry *next;
};

static TypeTablePtr new_type_table(void) {
    TypeTablePtr retval = malloc(sizeof(struct TypeEntry));
    retval->name = NULL;
    retval->next = NULL;
    retval->top_level = false;
    retval->scope_level = 0;
    return retval;
}

static void push_type(TypeTablePtr table, char *name,
    struct DeclarationList *regulardata,
    struct DeclarationList *constdata,
    bool top_level, unsigned scope_level) {
    TypeTablePtr curr = table->next;
    table->next = new_type_table();
    table->next->name = name;
    table->next->regulardata = regulardata;
    table->next->constdata = constdata;
    table->next->top_level = top_level;
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

static void cull_types(TypeTablePtr table, unsigned scope_to_delete) {
    assert(scope_to_delete > 0);
    
    while (table->next != NULL) {
        if (table->next->scope_level == scope_to_delete) {
            TypeTablePtr del = table->next;
            table->next = table->next->next;
            free(del);
        } else {
            if (table->next->constdata != NULL) {
                REWIND_LIST(table->next->constdata);
                while (table->next->constdata->scope_level == scope_to_delete) {
                    table->next->constdata = table->next->constdata->next;
                    table->next->constdata->prev = NULL;
                }
                while (table->next->constdata->next != NULL) {
                    if (table->next->constdata->next->scope_level
                        == scope_to_delete) {
                        table->next->constdata->next = table->next->constdata->next->next;
                        if (table->next->constdata->next != NULL)
                            table->next->constdata->next->prev = table->next->constdata;
                    } else {
                        table->next->constdata = table->next->constdata->next;
                    }
                }
            }

            table = table->next;
        }
    }
}
#endif
