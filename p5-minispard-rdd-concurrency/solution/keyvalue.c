#include "keyvalue.h"
#include <stdlib.h>
#include <string.h>

KeyValue* key_value_create(const char* key, void* value){
    KeyValue *kv = malloc(sizeof(KeyValue));
    if(kv == NULL){
        return NULL;
    }

    kv->key = strdup(key);
    if(kv->key == NULL){
        free(kv);
        return NULL;
    }

    kv->value = value;
    return kv;
}

char* key_value_get_key(KeyValue* kv){
    if(kv == NULL){
        return NULL;
    }
    return kv->key;
}

void* key_value_get_value(KeyValue *kv){
    if(kv == NULL){
        return NULL;
    }
    return kv->value;
}

void key_value_set_value(KeyValue *kv, void* value){
    if(kv == NULL){
        return;
    }
    kv->value = value;
    return;
}

void key_value_free(KeyValue *kv){
    if(kv == NULL){
        return;
    }
    if(kv->key != NULL){
        free(kv->key);
    }
    free(kv);
    return;
}

