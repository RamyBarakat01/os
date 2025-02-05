#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>

#define MAX_WORD_LENGTH 100
#define HASH_TABLE_SIZE 1000

// Structure to store word counts
typedef struct WordCount {
    char word[MAX_WORD_LENGTH];
    int count;
    struct WordCount* next;
} WordCount;

// Hash table for storing word counts
WordCount* hash_table[HASH_TABLE_SIZE];

// Mutex for thread-safe access to the hash table
pthread_mutex_t mutex;

// Simple hash function for strings (case-insensitive)
unsigned int hash(const char* str) {
    unsigned int hash_val = 0;
    while (*str) {
        hash_val = (hash_val * 31) + *str++;
    }
    return hash_val % HASH_TABLE_SIZE;
}

// Insert or update the word count in the hash table
void insert_word_count(const char* word) {
    unsigned int index = hash(word);
    pthread_mutex_lock(&mutex);
    WordCount* entry = hash_table[index];

    // Check if the word already exists in the hash table
    while (entry != NULL) {
        if (strcmp(entry->word, word) == 0) {
            entry->count++;
            pthread_mutex_unlock(&mutex);
            return;
        }
        entry = entry->next;
    }

    // If the word doesn't exist, add a new entry
    WordCount* new_entry = (WordCount*)malloc(sizeof(WordCount));
    strcpy(new_entry->word, word);
    new_entry->count = 1;
    new_entry->next = hash_table[index];
    hash_table[index] = new_entry;
    pthread_mutex_unlock(&mutex);
}

// Function to count words in a segment of the file
void* count_words_in_segment(void* arg) {
    FILE* file = (FILE*)arg;
    char word[MAX_WORD_LENGTH];
    
    // Read words from the file and count them
    while (fscanf(file, "%s", word) != EOF) {
        // Convert the word to lowercase for case-insensitive counting
        for (int i = 0; word[i]; i++) {
            word[i] = tolower(word[i]);
        }
        insert_word_count(word);
    }
    return NULL;
}

// Function to split the file into N parts and process each part in a thread
void split_file(const char* filename, int num_threads) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    long segment_size = file_size / num_threads;

    pthread_t threads[num_threads];
    FILE* thread_files[num_threads];

    // Create threads and assign them different parts of the file
    for (int i = 0; i < num_threads; i++) {
        long start_pos = i * segment_size;
        long end_pos = (i == num_threads - 1) ? file_size : (i + 1) * segment_size;

        // Create a temporary file for this segment
        thread_files[i] = tmpfile();
        if (thread_files[i] == NULL) {
            perror("Error creating temporary file");
            exit(1);
        }

        // Rewind the main file and copy the segment to the temporary file
        rewind(file);
        fseek(file, start_pos, SEEK_SET);

        char buffer[1024];
        while (ftell(file) < end_pos && fgets(buffer, sizeof(buffer), file)) {
            fputs(buffer, thread_files[i]);
        }

        // Rewind the temporary file for reading
        rewind(thread_files[i]);

        // Create a thread to process this segment
        pthread_create(&threads[i], NULL, count_words_in_segment, (void*)thread_files[i]);
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        fclose(thread_files[i]);
    }

    fclose(file);
}

// Function to print the word counts
void print_word_counts() {
    printf("Word Counts:\n");
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        WordCount* entry = hash_table[i];
        while (entry != NULL) {
            printf("%s: %d\n", entry->word, entry->count);
            entry = entry->next;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <num_threads>\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];
    int num_threads = atoi(argv[2]);

    // Initialize the mutex for thread safety
    pthread_mutex_init(&mutex, NULL);

    // Split the file into chunks and start counting
    split_file(filename, num_threads);

    // Print the final word counts
    print_word_counts();

    // Clean up
    pthread_mutex_destroy(&mutex);

    return 0;
}

