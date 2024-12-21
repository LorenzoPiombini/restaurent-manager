#ifndef INDEX_H
#define INDEX_H


unsigned char put_data_index_nr(int index_pos, HashTable *ht,char* key);
unsigned char clean_index_nr(int index_n, HashTable* ht);
unsigned char tip_indexing(HashTable *ht, char* key);
unsigned char timecard_indexing();




#endif
