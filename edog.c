#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "edog.h"

#pragma pack(1)
typedef struct {
    int32_t  posx;
    int32_t  posy;
    unsigned angle0 : 9;
    unsigned angle1 : 9;
    unsigned angle2 : 9;
    unsigned angle3 : 9;
    unsigned angle4 : 9;
    unsigned speed  : 8;
    unsigned ctype  : 8;
    unsigned dtype  : 2;
    unsigned isleaf : 1;
} EDBRECORD;
#pragma pack()

static int SPLIT_IDXTAB[4][4] = {
    { 0, 1, 4, 5},
    { 4, 1, 2, 5},
    { 0, 5, 4, 3},
    { 4, 5, 2, 3},
};

#define QUADNODE_ISLEAF(node) (!(node)->child[0] && !(node)->child[1] && !(node)->child[2] && !(node)->child[3])

QUADNODE* quadtree_create(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    QUADNODE *tree  = calloc(1, sizeof(QUADNODE));
    if (tree) {
        tree->left   = left  ;
        tree->top    = top   ;
        tree->right  = right ;
        tree->bottom = bottom;
    }
    return tree;
}

void quadtree_destroy(QUADNODE *tree)
{
    int i;
    if (tree) {
        for (i=0; i<4; i++) {
            if (tree->child[i]) {
                quadtree_destroy(tree->child[i]);
            }
        }
        free(tree);
    }
}

void quadtree_insert(QUADNODE *tree, NODEDATA *data)
{
    int i, j;
    if (!tree) return;
    if (QUADNODE_ISLEAF(tree)) {
        if (!tree->used) {
            tree->used = 1;
            tree->data = *data;
        } else {
            int points[6] = { tree->left, tree->top, tree->right, tree->bottom, (tree->left + tree->right ) / 2, (tree->top  + tree->bottom) / 2 };
            for (i=0; i<4; i++) {
                tree->child[i] = calloc(1, sizeof(QUADNODE));
                if (!tree->child[i]) continue;
                tree->child[i]->left   = points[SPLIT_IDXTAB[i][0]];
                tree->child[i]->top    = points[SPLIT_IDXTAB[i][1]];
                tree->child[i]->right  = points[SPLIT_IDXTAB[i][2]];
                tree->child[i]->bottom = points[SPLIT_IDXTAB[i][3]];
                if (  tree->data.posx >= tree->child[i]->left && tree->data.posx < tree->child[i]->right
                   && tree->data.posy >= tree->child[i]->top  && tree->data.posy < tree->child[i]->bottom ) {
                    tree->child[i]->used = 1;
                    tree->child[i]->data = tree->data;
                    memset(&tree->data, 0, sizeof(tree->data));
                }
                if (  data->posx >= tree->child[i]->left && data->posx < tree->child[i]->right
                   && data->posy >= tree->child[i]->top  && data->posy < tree->child[i]->bottom ) {
                    if (data->posx != tree->child[i]->data.posx || data->posy != tree->child[i]->data.posy) {
                        quadtree_insert(tree->child[i], data);
                    } else {
                        for (j=0; j<5; j++) {
                            if (tree->child[i]->data.angle[j] != -1 && abs(tree->child[i]->data.angle[j] - data->angle[0]) <= 5) {
//                              printf("already insert this angle !\n");
                                break;
                            } else if (tree->child[i]->data.angle[j] == -1) {
//                              printf("insert new angle: %d !\n", data->angle[0]);
                                tree->child[i]->data.angle[j] = data->angle[0];
                                break;
                            }
                        }
                        if (j == 5) {
                            printf("failed to insert node: %10d %10d %3d %3d %3d %3d\n", data->posx, data->posy, data->angle[0], data->speed, data->ctype, data->dtype);
                        }
                    }
                }
            }
        }
    } else {
        for (i=0; i<4; i++) {
            if (!tree->child[i]) continue;
            if (  data->posx >= tree->child[i]->left && data->posx < tree->child[i]->right
               && data->posy >= tree->child[i]->top  && data->posy < tree->child[i]->bottom ) {
                quadtree_insert(tree->child[i], data);
                return;
            }
        }
    }
}

QUADNODE* quadtree_find_from_tree(QUADNODE *tree, int32_t x, int32_t y)
{
    int i;
    if (QUADNODE_ISLEAF(tree)) return tree;
    for (i=0; i<4; i++) {
        if (!tree->child[i]) continue;
        if (  x >= tree->child[i]->left && x < tree->child[i]->right
           && y >= tree->child[i]->top  && y < tree->child[i]->bottom ) {
            return quadtree_find_from_tree(tree->child[i], x, y);
        }
    }
    return NULL;
}

#define QUEUE_SIZE  (2 * 1024 * 1024)
static void quadtree_assign_index(QUADNODE *tree)
{
    QUADNODE **queue = malloc(sizeof(QUADNODE*) * QUEUE_SIZE);
    QUADNODE  *node  = NULL;
    int        tail  = 0, head = 0, qnum = 0, index = 0, i;
    if (!queue) return;

    // put first node into queue
    queue[tail & (QUEUE_SIZE - 1)] = tree;
    tail++; qnum++;

    while (qnum > 0) {
        // dequeue
        node = queue[head & (QUEUE_SIZE - 1)];
        head++; qnum--;

        // handle current node
        node->index = index++;

        for (i=0; i<4; i++) {
            if (node->child[i] && (!QUADNODE_ISLEAF(node->child[i]) || node->child[i]->used)) {
                // enqueue
                queue[tail & (QUEUE_SIZE - 1)] = node->child[i];
                tail++; qnum++;
                if (qnum > QUEUE_SIZE) printf("queue size overflow !\n");
            }
        }
    }
    free(queue);
}

static void quadtree_assign_chdidx(QUADNODE *tree, FILE *fp, int bin)
{
    QUADNODE **queue = malloc(sizeof(QUADNODE*) * QUEUE_SIZE);
    QUADNODE  *node  = NULL;
    int        tail  = 0, head = 0, qnum = 0, i;
    if (!queue) return;

    // put first node into queue
    queue[tail & (QUEUE_SIZE - 1)] = tree;
    tail++; qnum++;

    while (qnum > 0) {
        // dequeue
        node = queue[head & (QUEUE_SIZE - 1)];
        head++; qnum--;

        //++ handle current node
        for (i=0; i<4; i++) {
            if (!node->child[i]) continue;
            node->chdidx[i] = node->child[i]->index;
        }
        if (bin) {
            EDBRECORD record;
            record.isleaf = QUADNODE_ISLEAF(node);
            if (record.isleaf) {
                record.posx   = node->data.posx ;
                record.posy   = node->data.posy ;
                record.angle0 = node->data.angle[0];
                record.angle1 = node->data.angle[1];
                record.angle2 = node->data.angle[2];
                record.angle3 = node->data.angle[3];
                record.angle4 = node->data.angle[4];
                record.speed  = node->data.speed;
                record.ctype  = node->data.ctype;
                record.dtype  = node->data.dtype;
            } else {
                memcpy(&record, node->chdidx, sizeof(int32_t) * 4);
            }
            fwrite(&record, sizeof(record), 1, fp);
        } else {
            fprintf(fp, "%7d (%10d %10d %10d %10d) (%10d %10d %3d %3d %3d %3d %3d %3d %3d %3d) (%7d %7d %7d %7d)\r\n",
                node->index, node->left, node->top, node->right, node->bottom, node->data.posx, node->data.posy,
                node->data.angle[0], node->data.angle[1], node->data.angle[2], node->data.angle[3], node->data.angle[4],
                node->data.speed, node->data.ctype, node->data.dtype, node->chdidx[0], node->chdidx[1], node->chdidx[2], node->chdidx[3]);
        }
        //-- handle current node

        for (i=0; i<4; i++) {
            if (node->child[i] && (!QUADNODE_ISLEAF(node->child[i]) || node->child[i]->used)) {
                // enqueue
                queue[tail & (QUEUE_SIZE - 1)] = node->child[i];
                tail++; qnum++;
                if (qnum > QUEUE_SIZE) printf("queue size overflow !\n");
            }
        }
    }
    free(queue);
}

void quadtree_save_edx(char *file, QUADNODE *tree, int bin)
{
    FILE *fp = fopen(file, "wb");
    if (fp) {
        fprintf(fp, "edog %s file v1.0.0 %10d %10d %10d %10d\r\n", bin ? "edb" : "edt", tree->left, tree->top, tree->right, tree->bottom);
        quadtree_assign_index (tree);
        quadtree_assign_chdidx(tree, fp, bin);
        fclose(fp);
    }
}

static void load_node_from_edt(QUADNODE *node, FILE *fp, int index)
{
    int32_t idx, left, top, right, bottom, posx, posy, angle[5], speed, ctype, dtype, child[4];
    fseek (fp, 66 + index * 145, SEEK_SET); // 66 is the size of file header, and 145 is the line size of edt record
    fscanf(fp, "%d (%d %d %d %d) (%d %d %d %d %d %d %d %d %d %d) (%d %d %d %d)",
          &idx, &left, &top, &right, &bottom, &posx, &posy, &angle[0], &angle[1], &angle[2], &angle[3], &angle[4],
          &speed, &ctype, &dtype, &(child[0]), &(child[1]), &(child[2]), &(child[3]));
    node->left         = left;
    node->top          = top;
    node->right        = right;
    node->bottom       = bottom;
    node->data.posx    = posx;
    node->data.posy    = posy;
    node->data.angle[0]= angle[0];
    node->data.angle[1]= angle[1];
    node->data.angle[2]= angle[2];
    node->data.angle[3]= angle[3];
    node->data.angle[4]= angle[4];
    node->data.speed   = speed;
    node->data.dtype   = dtype;
    node->data.ctype   = ctype;
    node->chdidx[0]    = child[0];
    node->chdidx[1]    = child[1];
    node->chdidx[2]    = child[2];
    node->chdidx[3]    = child[3];
}

static void load_node_from_edb(QUADNODE *node, FILE *fp, int index)
{
    EDBRECORD record;
    fseek(fp, 66 + index * sizeof(EDBRECORD), SEEK_SET); // 66 is the size of file header
    fread(&record, sizeof(record), 1, fp);
    if (record.isleaf) {
        node->data.posx    = record.posx;
        node->data.posy    = record.posy;
        node->data.angle[0]= record.angle0;
        node->data.angle[1]= record.angle1;
        node->data.angle[2]= record.angle2;
        node->data.angle[3]= record.angle3;
        node->data.angle[4]= record.angle4;
        node->data.speed   = record.speed;
        node->data.dtype   = record.dtype;
        node->data.ctype   = record.ctype;
        node->chdidx[0]    = node->chdidx[1] = node->chdidx[2] = node->chdidx[3] = 0;
    } else {
        memcpy(node->chdidx, &record, sizeof(int32_t) * 4);
    }
}

int quadtree_find_from_edx(char *file, int32_t x, int32_t y, NODEDATA *data, int bin)
{
    int32_t  stack[4][5], left, top, right, bottom;
    QUADNODE node;
    FILE    *fp   = fopen(file, "rb");
    int      stop = 0, ret = -1, i;
    char     str[8];
    if (fp) {
        // read header
        fscanf(fp, "%8s %8s %8s %8s %d %d %d %d", str, str, str, str, &left, &top, &right, &bottom);
        stack[stop  ][0] = left;
        stack[stop  ][1] = top;
        stack[stop  ][2] = right;
        stack[stop  ][3] = bottom;
        stack[stop++][4] = 0;
        while (stop-- > 0) {
            if (bin) load_node_from_edb(&node, fp, stack[stop][4]);
            else     load_node_from_edt(&node, fp, stack[stop][4]);
            if (x >= stack[stop][0] && x < stack[stop][2] && y >= stack[stop][1] && y < stack[stop][3]) {
                if (!node.chdidx[0] && !node.chdidx[1] && !node.chdidx[2] && !node.chdidx[3]) {
                    *data = node.data;
                    ret   = 0;
                    break;
                } else {
                    int32_t points[6] = { stack[stop][0], stack[stop][1], stack[stop][2], stack[stop][3], (stack[stop][0] + stack[stop][2]) / 2, (stack[stop][1] + stack[stop][3]) / 2 };
                    for (stop=0,i=0; i<4; i++) {
                        if (node.chdidx[i]) {
                            stack[stop  ][0] = points[SPLIT_IDXTAB[i][0]];
                            stack[stop  ][1] = points[SPLIT_IDXTAB[i][1]];
                            stack[stop  ][2] = points[SPLIT_IDXTAB[i][2]];
                            stack[stop  ][3] = points[SPLIT_IDXTAB[i][3]];
                            stack[stop++][4] = node.chdidx[i];
                        }
                    }
                }
            }
        }
        fclose(fp);
    }
    return ret;
}

#ifdef TEST
int main(void)
{
    QUADNODE *tree = NULL;
    QUADNODE *node = NULL;
    FILE     *fp   = NULL;
    double    posx, posy;
    int       angle, speed, ctype, dtype, findx, findy, ret;
    NODEDATA  data;

    // create quadtreee
    tree = quadtree_create(-180 * 1000000, -90 * 1000000, 180 * 1000000, 90 * 1000000);

    // read data from edog.txt and build quadtree
    fp = fopen("edog.txt", "rb");
    if (fp) {
        while (1) {
//          ret = fscanf(fp, "%lf %lf %d %d %d %d", &posx, &posy, &angle, &speed, &ctype, &dtype);
            ret = fscanf(fp, "%lf %lf %d %d %d %d", &posy, &posx, &angle, &speed, &ctype, &dtype);
            if (ret != 6) break;
            data.posx     = posx * 1000000;
            data.posy     = posy * 1000000;
            data.angle[0] = angle;
            data.angle[1] = -1;
            data.angle[2] = -1;
            data.angle[3] = -1;
            data.angle[4] = -1;
            data.speed    = speed;
            data.ctype    = ctype;
            data.dtype    = dtype;
            quadtree_insert(tree, &data);
        }
        fclose(fp);
    }

    findx = 40.6561759 * 1000000;
    findy = 55.6055057 * 1000000;

    // find data from quadtree
    node = quadtree_find_from_tree(tree, findx, findy);
    if (node) {
        printf("pos  : %lf, %lf\n", (double)node->data.posx / 1000000, (double)node->data.posy / 1000000);
        printf("angle: %d %d %d %d %d\n", node->data.angle[0], node->data.angle[1], node->data.angle[2], node->data.angle[3], node->data.angle[4]);
        printf("speed: %d\n", node->data.speed);
        printf("ctype: %d\n", node->data.ctype);
        printf("dtype: %d\n", node->data.dtype);
        printf("\n");
    }

    // save quadtree to edt file and destroy quadtree
    quadtree_save_edx("edog.edt", tree, 0);
    quadtree_save_edx("edog.edb", tree, 1);
    quadtree_destroy(tree);

    // find data from edt file
    ret = quadtree_find_from_edx("edog.edt", findx, findy, &data, 0);
    if (ret == 0) {
        printf("pos  : %lf, %lf\n", (double)data.posx / 1000000, (double)data.posy / 1000000);
        printf("angle: %d %d %d %d %d\n", node->data.angle[0], node->data.angle[1], node->data.angle[2], node->data.angle[3], node->data.angle[4]);
        printf("speed: %d\n", data.speed);
        printf("ctype: %d\n", data.ctype);
        printf("dtype: %d\n", data.dtype);
        printf("\n");
    }

    // find data from edb file
    ret = quadtree_find_from_edx("edog.edb", findx, findy, &data, 1);
    if (ret == 0) {
        printf("pos  : %lf, %lf\n", (double)data.posx / 1000000, (double)data.posy / 1000000);
        printf("angle: %d %d %d %d %d\n", node->data.angle[0], node->data.angle[1], node->data.angle[2], node->data.angle[3], node->data.angle[4]);
        printf("speed: %d\n", data.speed);
        printf("ctype: %d\n", data.ctype);
        printf("dtype: %d\n", data.dtype);
        printf("\n");
    }
    return 0;
}
#endif

















