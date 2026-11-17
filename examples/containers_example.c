// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock.h>
#include <bedrock/dynmap.h>
#include <bedrock/dynvec.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct Person {
    const char *name;
    int score;
} Person;

static void free_person(void *element)
{
    free(element);
}

DEFINE_STRING_KEY_MAP(PersonMap, Person)
DEFINE_INT_KEY_MAP(ScoreMap, Person)

static Person *make_person(const char *name, const int score)
{
    Person *person = malloc(sizeof(*person));
    if (!person) {
        return NULL;
    }
    person->name = name;
    person->score = score;
    return person;
}

int main(void)
{
    DynVec *people = dynvec_new(free_person);
    PersonMap by_name;
    ScoreMap by_score;
    size_t i;

    printf("Containers — Bedrock v%s\n", BEDROCK_VERSION);
    if (!people) {
        fprintf(stderr, "dynvec_new failed\n");
        return 1;
    }

    Person *ada_person = make_person("Ada", 10);
    Person *lin = make_person("Lin", 20);
    Person *grace = make_person("Grace", 30);
    Person *edsger = make_person("Edsger", 40);

    if (!ada_person || !lin || !grace || !edsger) {
        free(ada_person);
        free(lin);
        free(grace);
        free(edsger);
        dynvec_free(people);
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    dynvec_push(people, ada_person);
    dynvec_push(people, lin);
    dynvec_push(people, grace);
    dynvec_insert(people, 1, edsger);

    printf("dynvec (%zu people):\n", dynvec_length(people));
    for (i = 0; i < dynvec_length(people); ++i) {
        const Person *person = dynvec_at(people, i);
        printf("  %zu. %s %d\n", i, person->name, person->score);
    }

    PersonMap_init(&by_name, 8);
    for (i = 0; i < dynvec_length(people); ++i) {
        Person *person = dynvec_at(people, i);
        if (PersonMap_insert(&by_name, person->name, person) != 1) {
            PersonMap_destroy(&by_name);
            dynvec_free(people);
            fprintf(stderr, "dynmap insert failed\n");
            return 1;
        }
    }

    Person *ada = PersonMap_get(&by_name, "Ada");
    if (!ada) {
        PersonMap_destroy(&by_name);
        dynvec_free(people);
        fprintf(stderr, "Ada was not found\n");
        return 1;
    }
    ada->score = 15;
    printf("dynmap Ada -> %d (vector index %zu)\n", ada->score, dynvec_index_of(people, ada));

    if (PersonMap_delete(&by_name, "Lin") != 1 || PersonMap_get(&by_name, "Lin") != NULL) {
        PersonMap_destroy(&by_name);
        dynvec_free(people);
        fprintf(stderr, "failed to remove Lin\n");
        return 1;
    }
    printf("removed Lin from the map; vector still has %zu people\n", dynvec_length(people));

    ScoreMap_init(&by_score, 8);
    for (i = 0; i < dynvec_length(people); ++i) {
        Person *person = dynvec_at(people, i);
        if (ScoreMap_insert(&by_score, person->score, person) != 1) {
            ScoreMap_destroy(&by_score);
            PersonMap_destroy(&by_name);
            dynvec_free(people);
            fprintf(stderr, "score map insert failed\n");
            return 1;
        }
    }
    {
        Person *found = ScoreMap_get(&by_score, 40);
        if (!found) {
            ScoreMap_destroy(&by_score);
            PersonMap_destroy(&by_name);
            dynvec_free(people);
            fprintf(stderr, "score 40 was not found\n");
            return 1;
        }

        printf("dynmap score 40 -> %s\n", found->name);
        if (ScoreMap_delete(&by_score, 20) != 1 || ScoreMap_get(&by_score, 20) != NULL) {
            ScoreMap_destroy(&by_score);
            PersonMap_destroy(&by_name);
            dynvec_free(people);
            fprintf(stderr, "failed to remove score 20\n");
            return 1;
        }
        printf("removed score 20 from the int-key map\n");
    }

    ScoreMap_destroy(&by_score);
    PersonMap_destroy(&by_name);
    dynvec_free(people);
    return 0;
}
