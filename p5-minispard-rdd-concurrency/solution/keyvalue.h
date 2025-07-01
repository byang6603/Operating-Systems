#ifndef __keyvalue_h__
#define __keyvalue_h__

typedef struct{
    char *key;
    void *value;
}KeyValue;

KeyValue* key_value_create(const char* key, void* value);
char *key_value_get_key(KeyValue *kv);
void *key_value_get_value(KeyValue *kv);
void key_value_set_value(KeyValue *kv, void* value);
void key_value_free(KeyValue *kv);
 


#endif