#pragma once

#include "cr.h"

struct obj;

struct obj* obj_create_null(void);
struct obj* obj_create_false(void);
struct obj* obj_create_true(void);
struct obj* obj_create_number(double number);
struct obj* obj_create_string(const char* string);
struct obj* obj_create_object(void);
struct obj* obj_create_array(void);
struct obj* obj_create_copy(struct obj* original);
void obj_destroy(struct obj* obj);

bool obj_is_null(struct obj* obj);
bool obj_is_false(struct obj* obj);
bool obj_is_true(struct obj* obj);
bool obj_is_string(struct obj* obj);
bool obj_is_object(struct obj* obj);

double obj_number_get(struct obj* obj);

const char* obj_string_get(struct obj* obj);

struct obj* obj_object_get(struct obj* obj, const char* key);
void obj_object_set(struct obj* obj, const char* key, struct obj* value);

struct obj* obj_array_get(struct obj* obj, uint32_t index);
uint32_t obj_array_get_count(struct obj* obj);
void obj_array_append(struct obj* obj, struct obj* item);

struct cr_str* obj_as_str(struct obj* obj, bool pretty);
void obj_print(struct obj* obj, bool pretty);
