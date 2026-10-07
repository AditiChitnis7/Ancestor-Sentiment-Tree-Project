#include <stdlib.h>
#include <stdio.h>

typedef struct node{
    int id;
    double own;
    double score;
    int nchildren;
    struct Node *first_child;
    struct Node *next_sibling;
}Node;

Node* new_node(int id, double own){
    Node* n = malloc(sizeof(Node));
    n->id=id; n->own=own; n->score=0.0; 
    n->nchildren=0;
    n->first_child=NULL; n->next_sibling=NULL;
    return n;
}

void add_child(Node *parent, Node *child){
    child->next_sibling=parent->first_child;
    parent->first_child=child;
    parent->nchildren++;
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
    printf("%*s%s[%d] own=%.2f score=%.2f\033[0m\n",
           depth * 4, "", col, n->id, n->own, n->score);
    for (Node *c = n->first_child; c; c = c->next_sibling)
        print_tree(c, depth + 1);
}

void free_tree(Node *n){
    Node *c = n->first_child;
    while (c) { Node *next = c->next_sibling; free_tree(c); c = next; }
    free(n);
}

int main(void) {
    Node *root = new_node(1, -0.8);
    Node *a  = new_node(2, 0.6);
    Node *b  = new_node(3, 0.4);
    Node *a1 = new_node(4, 0.9);
    add_child(root, a); add_child(root, b); add_child(a, a1);

    propogate(root, 0.3);
    print_tree(root, 0);
    free_tree(root);
    return 0;
}