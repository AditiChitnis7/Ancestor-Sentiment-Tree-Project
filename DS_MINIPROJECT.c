#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ID_LEN     16
#define TABLE_SIZE 100003            /* prime, fewer collisions */
typedef struct Node {
    char id[ID_LEN];
    char parent[ID_LEN];             /* only used while loading */
    double own, score;
    int nchildren;
    struct Node *first_child, *next_sibling;
} Node;
typedef struct Entry {
    Node *node;
    struct Entry *next;
} Entry;

Node *new_node(const char *id, double own) {
    Node *n = calloc(1, sizeof(Node));   /* zeroes pointers and counts */
    strncpy(n->id, id, ID_LEN - 1);
    n->own = own;
    return n;
}

void add_child(Node *parent, Node *child){
    child->next_sibling=parent->first_child;
    parent->first_child=child;
    parent->nchildren++;
}

static Entry *table[TABLE_SIZE];

static unsigned long hash(const char *s) {          /* djb2 */
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++)) h = h * 33 + c;
    return h % TABLE_SIZE;
}

Node *table_get(const char *id) {
    for (Entry *e = table[hash(id)]; e; e = e->next)
        if (strcmp(e->node->id, id) == 0) return e->node;
    return NULL;
}

void table_put(Node *n) {
    unsigned long h = hash(n->id);
    Entry *e = malloc(sizeof(Entry));
    e->node = n; e->next = table[h]; table[h] = e;
}

void table_free(void) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *e = table[i];
        while (e) { Entry *nx = e->next; free(e); e = nx; }
        table[i] = NULL;
    }
}

/* ---- storage for loaded nodes ---- */
static Node **all_nodes = NULL;
static int n_nodes = 0, cap_nodes = 0;

static void remember(Node *n) {
    if (n_nodes == cap_nodes) {
        cap_nodes = cap_nodes ? cap_nodes * 2 : 1024;
        all_nodes = realloc(all_nodes, cap_nodes * sizeof(Node *));
    }
    all_nodes[n_nodes++] = n;
}

/* split "id,parent,score"; strtok is unsuitable (skips empty fields) */
static int parse_line(char *line, char *id, char *parent, double *score) {
    char *c1 = strchr(line, ',');
    if (!c1) return 0;
    char *c2 = strchr(c1 + 1, ',');
    if (!c2) return 0;
    *c1 = '\0'; *c2 = '\0';
    if (strlen(line) >= ID_LEN || strlen(c1 + 1) >= ID_LEN) return 0;
    strcpy(id, line);
    strcpy(parent, c1 + 1);
    char *end;
    *score = strtod(c2 + 1, &end);      /* also copes with Windows \r\n */
    return end != c2 + 1;
}

/* returns number of roots written into *roots_out, or -1 on error */
int load_csv(const char *path, Node ***roots_out) {
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }

    char line[256];
    if (!fgets(line, sizeof line, fp)) {      /* empty file */
    fclose(fp);
    *roots_out = NULL;
    return 0;
}

    /* pass 1: create every node, so parents exist before linking */
    int bad = 0, dup = 0;
    while (fgets(line, sizeof line, fp)) {
        char id[ID_LEN], parent[ID_LEN];
        double score;
        if (!parse_line(line, id, parent, &score)) { bad++; continue; }
        if (table_get(id)) { dup++; continue; }
        Node *n = new_node(id, score);
        strcpy(n->parent, parent);
        table_put(n);
        remember(n);
    }
    fclose(fp);

    /* pass 2: link children to parents, collect roots */
    Node **roots = malloc((n_nodes ? n_nodes : 1) * sizeof(Node *));
    int n_roots = 0, orphans = 0;
    for (int i = 0; i < n_nodes; i++) {
        Node *n = all_nodes[i];
        if (n->parent[0] == '\0') {
            roots[n_roots++] = n;
        } else {
            Node *p = table_get(n->parent);
            if (p && p != n) add_child(p, n);
            else orphans++;                          /* parent missing */
        }
    }
    fprintf(stderr, "loaded %d nodes, %d roots, %d orphans, %d bad rows, %d duplicates\n",
            n_nodes, n_roots, orphans, bad, dup);
    *roots_out = roots;
    return n_roots;
}

/* post order: chilren first, then the parnet */
double propogate(Node *n, double w){
    double sum=0.0;
    for(Node *c = n->first_child; c; c=c->next_sibling)
    sum+=propogate(c,w);
    if(n->nchildren==0)
    n->score = n->own;
    else
    n->score = w*n->own + (1.0-w)*(sum/n->nchildren);
    return n->score;
}

void print_tree(Node *n,int depth){
    const char *col = n->score > 0.1 ? "\033[32m":n->score <-0.1 ? "\033[31m":"\033[33m";
    printf("%*s%s[%s] own=%.2f score=%.2f\033[0m\n",
           depth * 4, "", col, n->id, n->own, n->score);
    for (Node *c = n->first_child; c; c = c->next_sibling)
        print_tree(c, depth + 1);
}

/*void free_tree(Node *n){
    Node *c = n->first_child;
    while (c) { Node *next = c->next_sibling; free_tree(c); c = next; }
    free(n);
}*/
static void sum_tree(Node *n, double *sum, int *count) {
    *sum += n->own;
    (*count)++;
    for (Node *c = n->first_child; c; c = c->next_sibling)
        sum_tree(c, sum, count);
}

void print_table(Node **roots, int n, double w) {
    printf("root_id,size,root_only,flat_mean,propagated\n");
    for (int i = 0; i < n; i++) {
        double sum = 0.0;
        int count = 0;
        sum_tree(roots[i], &sum, &count);
        double prop = propogate(roots[i], w);
        printf("%s,%d,%.4f,%.4f,%.4f\n",
               roots[i]->id, count, roots[i]->own, sum / count, prop);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s file.csv\n", argv[0]); return 1; }
    Node **roots;
    int n = load_csv(argv[1], &roots);
    if (n < 0) return 1;
        if (argc > 2 && strcmp(argv[2], "table") == 0) {
        print_table(roots, n, 0.3);
    } else {
        for (int i = 0; i < n; i++) {
            propogate(roots[i], 0.3);
            print_tree(roots[i], 0);
        }
    }
    for (int i = 0; i < n_nodes; i++) free(all_nodes[i]);
    free(all_nodes); free(roots); table_free();
    return 0;
}

