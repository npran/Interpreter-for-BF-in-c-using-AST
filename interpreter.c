#include <stdio.h>
#include <stdlib.h>

#define TAPE_SIZE 65535
#define MAX_LOOP_DEPTH 512

// AST node types
typedef enum {
    NODE_INC_PTR,
    NODE_DEC_PTR,
    NODE_INC_VAL,
    NODE_DEC_VAL,
    NODE_OUT,
    NODE_IN,
    NODE_LOOP
} NodeType;

// AST node
typedef struct Node {
    NodeType type;
    struct Node *child;
    struct Node *next;
} Node;

// Create a new AST node
static Node* new_node(NodeType type) {
    Node *n = (Node*)malloc(sizeof(Node));
    if (!n) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    n->type = type;
    n->child = NULL;
    n->next = NULL;
    return n;
}

// Free AST recursively
static void free_tree(Node *node) {
    while (node) {
        if (node->child)
            free_tree(node->child);
        Node *next = node->next;
        free(node);
        node = next;
    }
}

// Parse source file into AST
Node* compile_tree(FILE *fp) {
    Node *root = NULL;
    Node *last_top = NULL;

    Node *loop_stack[MAX_LOOP_DEPTH];
    Node *last_at_depth[MAX_LOOP_DEPTH + 1];

    int depth = 0;
    int c;

    for (int i = 0; i <= MAX_LOOP_DEPTH; i++)
        last_at_depth[i] = NULL;

    while ((c = getc(fp)) != EOF) {
        Node *n = NULL;

        switch (c) {
        case '>': n = new_node(NODE_INC_PTR); break;
        case '<': n = new_node(NODE_DEC_PTR); break;
        case '+': n = new_node(NODE_INC_VAL); break;
        case '-': n = new_node(NODE_DEC_VAL); break;
        case '.': n = new_node(NODE_OUT);     break;
        case ',': n = new_node(NODE_IN);      break;

        case '[':
            if (depth >= MAX_LOOP_DEPTH) {
                fprintf(stderr, "Error: loop nesting too deep\n");
                free_tree(root);
                return NULL;
            }

            n = new_node(NODE_LOOP);

            if (depth == 0) {
                if (!root) root = n;
                else last_top->next = n;
                last_top = n;
                last_at_depth[0] = n;
            } else {
                Node *parent = loop_stack[depth - 1];
                if (!parent->child)
                    parent->child = n;
                else
                    last_at_depth[depth]->next = n;
                last_at_depth[depth] = n;
            }

            loop_stack[depth] = n;
            depth++;
            last_at_depth[depth] = NULL;
            continue;

        case ']':
            if (depth == 0) {
                fprintf(stderr, "Syntax error: unmatched ']'\n");
                free_tree(root);
                return NULL;
            }
            depth--;
            continue;

        default:
            continue; // ignoring non-BF characters acts as comments
        }

        // Attach normal instruction node
        if (depth == 0) {
            if (!root) root = n;
            else last_top->next = n;
            last_top = n;
            last_at_depth[0] = n;
        } else {
            Node *parent = loop_stack[depth - 1];
            if (!parent->child)
                parent->child = n;
            else
                last_at_depth[depth]->next = n;
            last_at_depth[depth] = n;
        }
    }

    if (depth != 0) {
        fprintf(stderr, "Syntax error: unmatched '['\n");
        free_tree(root);
        return NULL;
    }

    return root;
}

// Execute AST
void execute_tree(Node *node, unsigned char *data, unsigned int *ptr) {
    while (node) {
        switch (node->type) {
        case NODE_INC_PTR:
            *ptr = (*ptr + 1) % TAPE_SIZE;
            break;

        case NODE_DEC_PTR:
            *ptr = (*ptr + TAPE_SIZE - 1) % TAPE_SIZE;
            break;

        case NODE_INC_VAL:
            data[*ptr]++;
            break;

        case NODE_DEC_VAL:
            data[*ptr]--;
            break;

        case NODE_OUT:
            putchar(data[*ptr]);
            break;

        case NODE_IN: {
            int ch = getchar();
            data[*ptr] = (ch == EOF) ? 0 : (unsigned char)ch;
            break;
        }

        case NODE_LOOP:
            while (data[*ptr])
                execute_tree(node->child, data, ptr);
            break;
        }

        node = node->next;
    }
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    Node *program = compile_tree(fp);
    fclose(fp);

    if (!program)
        return 1;

    unsigned char *data = (unsigned char*)calloc(TAPE_SIZE, 1);
    if (!data) {
        fprintf(stderr, "Memory allocation failed for data tape.\n");
        free_tree(program);
        return 1;
    }

    unsigned int ptr = 0;
    execute_tree(program, data, &ptr);

    free(data);
    free_tree(program);
    return 0;
}
