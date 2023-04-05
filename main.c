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

typedef struct {
    size_t size;
    todo_t* data;
} todo_query_result_t;


typedef enum {
    TODO_VERBOSITY_SHORT,
    TODO_VERBOSITY_LONG
} todo_verbosity_t;

typedef enum {
    RESULT_SUCCESS = 0,
    RESULT_ERROR = 1
} result_t;


grv_str generate_id() {
    u64 id;
    grv_random_bytes(&id, sizeof(id));
    return grv_str_from_u64(id);
}


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

grv_strarr todo_list_get_matching_ids(todo_list_t* list, grv_str* id) {
    grv_strarr result = grv_strarr_new();
    for (int i = 0; i < list->size; i++) {
        todo_t* todo = &list->data[i];
        if (grv_str_starts_with(&todo->id, id)) {
            grv_str match = grv_str_copy(&todo->id);
            grv_strarr_push_back(&result, match);
        }
    }
    return result;
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


result_t write_todo(todo_t* todo) {
    grv_str filename = grv_fs_add_ext(&todo->id, "todo");
    if (!grv_fs_dir_exists(".todo")) {
        grv_fs_mkdir(".todo");
    }   
    grv_str path = grv_fs_make_path(".todo", &filename);
    grv_str_free(&filename);
    FILE* file = fopen(grv_str_cstr(&path), "w");
    grv_str_free(&path);
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", grv_str_cstr(&path));
        return RESULT_ERROR;
    }

    fprintf(file, "title: %s\n", grv_str_cstr(&todo->title));
    fprintf(file, "date: %s\n", grv_str_cstr(&todo->date));
    fprintf(file, "tags: %s\n", grv_str_cstr(&todo->tags));    
    fprintf(file, "priority: %f\n", todo->priority);
    fprintf(file, "status: %s\n", todo->status == TODO_STATUS_OPEN ? "open" : "resolved");
    fprintf(file, "description: %s\n", grv_str_cstr(&todo->description));

    return RESULT_SUCCESS;
}

bool is_cmd(grv_str* arg, char* name, char* sname) {
    return grv_str_eq_cstr(arg, name) || grv_str_eq_cstr(arg, sname);
}


#define CHECK_ARG_AVAILABLE(MSG) \
    if (grv_strarr_size(&args) == 0) { \
        fprintf(stderr, "%s\n", MSG); \
        return RESULT_ERROR; \
    }

#define CHECK_TRUE(COND, MSG) \
    if (!(COND)) { \
        fprintf(stderr, "%s\n", MSG); \
        return RESULT_ERROR; \
    }   

bool is_option(grv_str* arg, char* name, char* sname) {
    grv_str long_name = grv_str_cat_cstr_cstr("--", name);
    grv_str short_name = grv_str_cat_cstr_cstr("-", sname);
    bool result = grv_str_eq(arg, &long_name) || grv_str_eq(arg, &short_name);
    grv_str_free(&long_name);
    grv_str_free(&short_name);
    return result;
}


todo_query_result_t todo_list_find(todo_list_t* list, grv_str* id) {
    todo_query_result_t res = {0};
    for (int i = 0; i < list->size; i++) {
        todo_t* todo = &list->data[i];
        if (grv_str_starts_with(&todo->id, id)) {
        }
    }
}

todo_t* find_todo_by_id(todo_list_t* list, grv_str* id) {
    if (!todo_list_has_id(&list, &id)) {
        fprintf(stderr, "Todo item with id %s does not exist.\n", grv_str_cstr(&id));
        exit(1);
    } else if (!todo_list_is_id_unique) {
        fprintf(stderr, "Todo item with id %s is not unique.\n", grv_str_cstr(&id));
        grv_strarr ids = todo_list_get_matching_ids(&list, &id);
        fprintf(stderr, "Found the following matching todo items: \n");
        for (int i = 0; i < grv_strarr_size(&ids); i++) {
            grv_str* id = grv_strarr_at(&ids, i);
            todo_t* todo = todo_list_get_by_id(&list, id);
            print_todo(&todo, TODO_VERBOSITY_SHORT);
        }
        // prompts the user to choose one of the ids
        printf("Please choose one of the ids: ");


        return RESULT_ERROR;
    }
}


result_t create_todo(grv_strarr args) {
    todo_t todo = {};
    todo.id = grv_str_new(grv_util_uuid());
    todo.status = TODO_STATUS_OPEN;
    todo.priority = 0.5f;
    todo.date = grv_str_new(grv_util_date());

    while (grv_strarr_size(&args)) {
        grv_str arg = grv_strarr_pop_front(&args);
        if (grv_str_eq_cstr(&arg, "--title") || grv_str_eq_cstr(&arg, "-t")) {
            CHECK_ARG_AVAILABLE("Missing title for --title argument");
            grv_str title = grv_strarr_pop_front(&args);
            todo.title = title;
        } else if (grv_str_eq_cstr(&arg, "--tags") || grv_str_eq_cstr(&arg, "-g")) {    
            CHECK_ARG_AVAILABLE("Missing tags for --tags argument");
            grv_str tags = grv_strarr_pop_front(&args);
            todo.tags = tags;
        } else if (grv_str_eq_cstr(&arg, "--description") || grv_str_eq_cstr(&arg, "-d")) {
            CHECK_ARG_AVAILABLE("Missing description for --description argument");
            grv_str description = grv_strarr_pop_front(&args);
            todo.description = description;
        } else if (grv_str_eq_cstr(&arg, "--priority") || grv_str_eq_cstr(&arg, "-p")) {
            CHECK_ARG_AVAILABLE("Missing priority for --priority argument");
            grv_str priority = grv_strarr_pop_front(&args);
            CHECK_TRUE(grv_str_is_float(&priority), "Priority must be a float");
            todo.priority = grv_str_to_f32(&priority);
            grv_str_free(&priority);
        } else if (grv_str_starts_with_cstr(&arg, "--")) {
            fprintf(stderr, "Unknown option: %s\n", grv_str_cstr(&arg));
            return RESULT_ERROR;
        } else {
            CHECK_ARG_AVAILABLE("Please specify a title for the todo item.");
            grv_str title = grv_strarr_pop_front(&args);
            todo.title = title;
        }
    }

    write_todo(&todo);

    return RESULT_SUCCESS;
}

result_t resolve_todo(grv_strarr args) {
    CHECK_ARG_AVAILABLE("Please specify a todo id to resolve.");
    grv_str id = grv_strarr_pop_front(&args);
    todo_list_t list = read_todo_list();    
    
    if (!todo_list_has_id(&list, &id)) {
        fprintf(stderr, "Todo item with id %s does not exist.\n", grv_str_cstr(&id));
        return RESULT_ERROR;
    } else if (!todo_list_is_id_unique) {
        fprintf(stderr, "Todo item with id %s is not unique.\n", grv_str_cstr(&id));
        grv_strarr ids = todo_list_get_matching_ids(&list, &id);
        fprintf(stderr, "Found the following matching todo items: \n");
        for (int i = 0; i < grv_strarr_size(&ids); i++) {
            grv_str* id = grv_strarr_at(&ids, i);
            todo_t* todo = todo_list_get_by_id(&list, id);
            print_todo(&todo, TODO_VERBOSITY_SHORT);
        }
        return RESULT_ERROR;
    }

    todo_t* todo = todo_list_get_by_id(&list, &id);

    if (todo->status == TODO_STATUS_RESOLVED) {
        fprintf(stderr, "Todo item is already resolved.\n");
        return RESULT_ERROR;
    }
    todo->status = TODO_STATUS_RESOLVED;
    write_todo(&todo);
    return RESULT_SUCCESS;
}

result_t list_todos(grv_strarr args) {
    todo_list_t list = read_todo_list();
    todo_verbosity_t verbosity = TODO_VERBOSITY_SHORT;
    bool show_resolved = false;
    grv_str id = {};
    while (grv_strarr_size(&args)) {
        grv_str arg = grv_strarr_pop_front(&args);
        if (is_option(&arg, "verbose", "v")) {
            verbosity = TODO_VERBOSITY_LONG;
        } else if (is_option(&arg, "resolved", "r")) {
            show_resolved = true;
        } else {
            id = grv_str_copy(arg);
        }
    }
    
    if (grv_str_size(&id) > 0) {
    }


    print_todo_list(&list, verbosity, show_resolved);
    return RESULT_SUCCESS;
}

result_t edit_todo(grv_strarr args) {
    CHECK_ARG_AVAILABLE("Please specify a todo id to edit.");
    grv_str id = grv_strarr_pop_front(&args);
    todo_list_t list = read_todo_list();    

    if (!todo_list_has_id(&list, &id)) {
        fprintf(stderr, "Todo item with id %s does not exist.\n", grv_str_cstr(&id));
        return RESULT_ERROR;
    } else if (!todo_list_is_id_unique) {
        fprintf(stderr, "Todo item with id %s is not unique.\n", grv_str_cstr(&id));
        grv_strarr ids = todo_list_get_matching_ids(&list, &id);
        fprintf(stderr, "Found the following matching todo items: \n");
        for (int i = 0; i < grv_strarr_size(&ids); i++) {
            grv_str* id = grv_strarr_at(&ids, i);
            todo_t* todo = todo_list_get_by_id(&list, id);
            print_todo(&todo, TODO_VERBOSITY_SHORT);
        }
        return RESULT_ERROR;
    }

    todo_t* todo = todo_list_get_by_id(&list, &id);
    grv_str title = grv_str_copy(todo->title);
    grv_str description = grv_str_copy(todo->description);
    grv_str tags = grv_str_copy(todo->tags);
    f32 priority = todo->priority;
    while (grv_strarr_size(&args)) {
        grv_str arg = grv_strarr_pop_front(&args);
        if (grv_str_eq_cstr(&arg, "--title") || grv_str_eq_cstr(&arg, "-t")) {
            CHECK_ARG_AVAILABLE("Missing title for --title argument");
            grv_str title = grv_strarr_pop_front(&args);
            todo->title = title;
        } else if (grv_str_eq_cstr(&arg, "--tags") || grv_str_eq_cstr(&arg, "-g")) {    
            CHECK_ARG_AVAILABLE("Missing tags for --tags argument");
            grv_str tags = grv_strarr_pop_front(&args);
            todo->tags = tags;
        } else if (grv_str_eq_cstr(&arg, "--description") || grv_str_eq_cstr(&arg, "-d")) {
            CHECK_ARG_AVAILABLE("Missing description for --description argument");
            gr
}

int main(int argc, char** argv)
{
    grv_strarr args = grv_strarr_from_cstr_array(argv, argc);
    grv_strarr_remove_front(&args);
    
    if (grv_strarr_size(&args) == 0) {
        todo_list_t list = read_todo_list();
        print_todo_list(&list, TODO_VERBOSITY_SHORT, true);
    } else {
        grv_str cmd = grv_strarr_pop_front(&args);

        if (is_cmd(&cmd, "create", "c")) {
            create_todo(args);   
        } else if (is_cmd, "resolve", "r") {
            resolve_todo(args);
        } else if (is_cmd, "list", "l") {
            list_todos(args);
        } else if (is_cmd, "edit", "e") {
            edit_todo(args);
        } else if (is_cmd, "delete", "d") {
            delete_todo(args);
        } else if (is_cmd, "help", "h") {
            print_help();
        } else {
            fprintf(stderr, "Unknown command: %s\n", grv_str_cstr(&cmd));
        }
    }
}
