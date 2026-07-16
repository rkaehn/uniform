#pragma once

#include "cr.h"

struct prim;

struct prim* prim_create_null(struct cr_mem* mem);
struct prim* prim_create_false(struct cr_mem* mem);
struct prim* prim_create_true(struct cr_mem* mem);
struct prim* prim_create_number(struct cr_mem* mem, double number);
struct prim* prim_create_string(struct cr_mem* mem, const char* string);
struct prim* prim_create_object(struct cr_mem* mem);
struct prim* prim_create_array(struct cr_mem* mem);
struct prim* prim_create_copy(struct cr_mem* mem, struct prim* original);
void prim_destroy(struct prim* prim);

bool prim_is_null(struct prim* prim);
bool prim_is_false(struct prim* prim);
bool prim_is_true(struct prim* prim);
bool prim_is_string(struct prim* prim);
bool prim_is_object(struct prim* prim);

double prim_number_get(struct prim* prim);

const char* prim_string_get(struct prim* prim);

struct prim* prim_object_get(struct prim* prim, const char* key);
void prim_object_set(struct prim* prim, const char* key, struct prim* value);

struct prim* prim_array_get(struct prim* prim, uint32_t index);
uint32_t prim_array_get_count(struct prim* prim);
void prim_array_append(struct prim* prim, struct prim* item);

struct cr_str* prim_as_str(struct cr_mem* mem, struct prim* prim, bool pretty);
void prim_print(struct prim* prim, bool pretty);
