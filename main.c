#include <stdio.h>
#include <stdlib.h>
#include "grv.h"

typedef enum {
    TODO_STATUS_OPEN,
    TODO_STATUS_RESOLVED
} todo_status_t;

typedef struct {
    grv_str id;
    grv_str title;
    grv_str tags;
    grv_str date;
    float priority;
    todo_status_t status;
    grv_str description; 
} todo_t;

typedef struct {
    size_t capacity;
    size_t size;
    todo_t* data;
} todo_list_t;

typedef enum {
    TODO_VERBOSITY_SHORT,
    TODO_VERBOSITY_LONG
} todo_verbosity_t;

todo_t* read_todo(grv_str* filename) {
    grv_str content = grv_fs_read_file(filename);
    todo_t* result = calloc(1, sizeof(todo_t));
    grv_str basename = grv_fs_basename(filename); 
    result->id = grv_fs_stem(&basename);
    grv_str_free(&basename);
    grv_strarr lines = grv_str_split(&content, "\n");    
    int count = 0;
    while (grv_strarr_size(&lines)) {
        grv_str* line = grv_strarr_front(&lines);
        count++; 
        if (grv_str_is_empty(line)) {
            // end of header section
            grv_strarr_remove_front(&lines);
            result->description = grv_strarr_join(&lines, "\n");
            break;
        }

        if (grv_str_contains_cstr(line, ":")) {
            grv_str key = grv_str_split_head_from_front(line, ":");
            grv_str value = grv_strarr_pop_front(&lines);
            grv_str_lstrip(&value);  
        
            if (grv_str_eq_cstr(&key, "title")) {
                result->title = value;
            } else if (grv_str_eq_cstr(&key, "tags")) {
                result->tags = value;
            } else if (grv_str_eq_cstr(&key, "date")) {
                result->date = value;
            } else if (grv_str_eq_cstr(&key, "priority")) {
                result->priority = grv_str_to_f32(&value);
                grv_str_free(&value);
            } else if (grv_str_eq_cstr(&key, "status")) {
                if (grv_str_eq_cstr(&value, "open")) {
                    result->status = TODO_STATUS_OPEN;
                } else if (grv_str_eq_cstr(&value, "resolved")) {
                    result->status = TODO_STATUS_RESOLVED;
                }
                grv_str_free(&value);
            } else {
                fprintf(stderr, "Skipping unknown key: %s in line %d\n", grv_str_cstr(&key), count);
                grv_str_free(&value);
            }

            grv_str_free(&key);
        } else {
            fprintf(stderr, "Skipping line: %s\n", grv_str_cstr(line));
            grv_strarr_remove_front(&lines);
        }
    }
    grv_strarr_free(&lines);

    return result;
}

todo_list_t todo_list_new() {
    todo_list_t list = {};
    list.capacity = 32;
    list.size = 0;
    list.data = calloc(list.capacity, sizeof(todo_t)); 
    return list;
}

void todo_list_push(todo_list_t* list, todo_t* todo) {
    if (list->size == list->capacity) {
        list->capacity *= 2;
        list->data = realloc(list->data, list->capacity * sizeof(todo_t));
    }
    list->data[list->size++] = *todo;
}

bool todo_list_is_id_unique(todo_list_t* list, grv_str* id) {
    int num_matches = 0;
    for (int i = 0; i < list->size; i++) {
        todo_t* todo = &list->data[i];
        if (grv_str_starts_with(&todo->id, id)) {
            num_matches++;
        }
    }

    return num_matches == 1;
}

todo_t* todo_list_get(todo_list_t* list, grv_str* id) {
    for (int i = 0; i < list->size; i++) {
        todo_t* todo = &list->data[i];
        if (grv_str_eq(&todo->id, id)) {
            return todo;
        }
    }
    return NULL;
}

todo_list_t read_todo_list() {
    todo_list_t result = todo_list_new();
    grv_str pattern = grv_str_new(".todo/*.todo");
    grv_strarr files = grv_util_glob(&pattern);
    grv_str_free(&pattern);
    while (grv_strarr_size(&files)) {
        grv_str filename = grv_strarr_pop_front(&files);
        todo_t* todo = read_todo(&filename);
        todo_list_push(&result, todo);
        grv_str_free(&filename);
    }
    grv_strarr_free(&files);
    return result;   
}

grv_str todo_get_short_id(todo_t* todo) {
    return grv_str_substr(&todo->id, 0, 6);
}

void print_todo(todo_t* todo, todo_verbosity_t verbosity) {
    if (verbosity == TODO_VERBOSITY_SHORT) {
        grv_str short_id = todo_get_short_id(todo);
        printf("%s  %s\n", grv_str_cstr(&short_id), grv_str_cstr(&todo->title));
        return;
    }    

    printf("ID: %s\n", grv_str_cstr(&todo->id));
    printf("title: %s\n", grv_str_cstr(&todo->title));
    printf("tags: %s\n", grv_str_cstr(&todo->tags));
    printf("date: %s\n", grv_str_cstr(&todo->date));
    printf("priority: %f\n", todo->priority);
    printf("status: %s\n", todo->status == TODO_STATUS_OPEN ? "open" : "resolved");
    printf("description: %s\n", grv_str_cstr(&todo->description));
    printf("\n");
}

void print_todo_list(todo_list_t* list, todo_verbosity_t verbosity, bool only_open) {
    for (int i = 0; i < list->size; i++) {
        todo_t* todo = &list->data[i];
        if (todo->status == TODO_STATUS_OPEN || !only_open) {
            print_todo(todo, verbosity);
        }
    }
}

int main(void)
{
    todo_list_t list = read_todo_list();
    print_todo_list(&list, TODO_VERBOSITY_SHORT, true);
}
