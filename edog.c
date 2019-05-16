#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int32_t  posx;
    int32_t  posy;
    uint16_t angle;
    uint8_t  speed;
    uint8_t  ctype;
    uint8_t  dtype;
} NODEDATA;

typedef struct tagQUADNODE {
    int32_t  left;
    int32_t  top;
    int32_t  right;
    int32_t  bottom;
    uint8_t  used;
    NODEDATA data;
    int32_t  index;
    int32_t  chdidx[4];
    struct tagQUADNODE *child[4];
} QUADNODE;

#define QUADNODE_ISLEAF(node)  (!(node)->child[0] && !(node)->child[1] && !(node)->child[2] && !(node)->child[3])

QUADNODE* quadtree_create(int left, int top, int right, int bottom)
{
    QUADNODE *root  = calloc(1, sizeof(QUADNODE));
    if (root) {
        root->left   = left  ;
        root->top    = top   ;
        root->right  = right ;
        root->bottom = bottom;
    }
    return root;
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
    int i;
    if (!tree) return;

    if (!QUADNODE_ISLEAF(tree)) {
        for (i=0; i<4; i++) {
            if (!tree->child[i]) continue;
            if (  data->posx >= tree->child[i]->left && data->posx < tree->child[i]->right
               && data->posy >= tree->child[i]->top  && data->posy < tree->child[i]->bottom ) {
                quadtree_insert(tree->child[i], data);
                break;
            }
        }
    } else if (tree->used) {
        int32_t cx = (tree->left + tree->right ) / 2;
        int32_t cy = (tree->top  + tree->bottom) / 2;
        for (i=0; i<4; i++) {
            tree->child[i] = calloc(1, sizeof(QUADNODE));
            if (!tree->child[i]) continue;
            switch (i) {
            case 0:
                tree->child[i]->left   = tree->left;
                tree->child[i]->top    = tree->top;
                tree->child[i]->right  = cx;
                tree->child[i]->bottom = cy;
                break;
            case 1:
                tree->child[i]->left   = cx;
                tree->child[i]->top    = tree->top;
                tree->child[i]->right  = tree->right;
                tree->child[i]->bottom = cy;
                break;
            case 2:
                tree->child[i]->left   = tree->left;
                tree->child[i]->top    = cy;
                tree->child[i]->right  = cx;
                tree->child[i]->bottom = tree->bottom;
                break;
            case 3:
                tree->child[i]->left   = cx;
                tree->child[i]->top    = cy;
                tree->child[i]->right  = tree->right;
                tree->child[i]->bottom = tree->bottom;
                break;
            }
            if (  tree->data.posx >= tree->child[i]->left && tree->data.posx < tree->child[i]->right
               && tree->data.posy >= tree->child[i]->top  && tree->data.posy < tree->child[i]->bottom ) {
                quadtree_insert(tree->child[i], &tree->data);
                memset(&tree->data, 0, sizeof(tree->data));
            }
            if (  data->posx >= tree->child[i]->left && data->posx < tree->child[i]->right
               && data->posy >= tree->child[i]->top  && data->posy < tree->child[i]->bottom ) {
                quadtree_insert(tree->child[i], data);
            }
        }
    } else {
        tree->used = 1;
        tree->data = *data;
    }
}

QUADNODE* quadtree_find_from_tree(QUADNODE *tree, int32_t x, int32_t y)
{
    int i;
    if (QUADNODE_ISLEAF(tree)) {
        return tree;
    }
    for (i=0; i<4; i++) {
        if (!tree->child[i]) continue;
        if (  x >= tree->child[i]->left && x < tree->child[i]->right
           && y >= tree->child[i]->top  && y < tree->child[i]->bottom ) {
            return quadtree_find_from_tree(tree->child[i], x, y);
        }
    }
    return NULL;
}

static void quadtree_assign_index(QUADNODE *tree, int32_t *index)
{
    int i;
    if (tree && (!QUADNODE_ISLEAF(tree) || tree->used)) {
        tree->index = (*index)++;
        for (i=0; i<4; i++) {
            if (!tree->child[i]) continue;
            quadtree_assign_index(tree->child[i], index);
        }
    }
}

static void quadtree_assign_chdidx(QUADNODE *tree, FILE *fp)
{
    int i;
    if (tree && (!QUADNODE_ISLEAF(tree) || tree->used)) {
        for (i=0; i<4; i++) {
            if (!tree->child[i]) continue;
            tree->chdidx[i] = tree->child[i]->index;
        }
        if (fp) {
            fprintf(fp, "%7d (%11d %11d %11d %11d) (%11d %11d %3d %3d %3d %3d) (%8d %8d %8d %8d)\r\n",
                tree->index, tree->left, tree->top, tree->right, tree->bottom,
                tree->data.posx, tree->data.posy, tree->data.angle, tree->data.speed, tree->data.ctype, tree->data.dtype,
                tree->chdidx[0], tree->chdidx[1], tree->chdidx[2], tree->chdidx[3]);
        }
        for (i=0; i<4; i++) {
            if (!tree->child[i]) continue;
            quadtree_assign_chdidx(tree->child[i], fp);
        }
    }
}

static void load_node_from_dbt(QUADNODE *node, FILE *fp, int index)
{
    int32_t idx, left, top, right, bottom, posx, posy, angle, speed, ctype, dtype, child[4];
    fseek (fp, index * 139, SEEK_SET);
    fscanf(fp, "%d (%d %d %d %d) (%d %d %d %d %d %d) (%d %d %d %d)",
          &idx, &left, &top, &right, &bottom, &posx, &posy, &angle, &speed, &dtype, &ctype,
          &(child[0]), &(child[1]), &(child[2]), &(child[3]));
    node->left      = left;
    node->top       = top;
    node->right     = right;
    node->bottom    = bottom;
    node->data.posx = posx;
    node->data.posy = posy;
    node->data.angle= angle;
    node->data.speed= speed;
    node->data.dtype= dtype;
    node->data.ctype= ctype;
    node->chdidx[0] = child[0];
    node->chdidx[1] = child[1];
    node->chdidx[2] = child[2];
    node->chdidx[3] = child[3];
}

int quadtree_find_from_dbt(char *file, int32_t x, int32_t y, NODEDATA *data)
{
    int32_t  stack[4] = {};
    QUADNODE node     = {};
    FILE    *fp       = fopen(file, "rb");
    int      top = 0, ret = -1, i;
    if (fp) {
        stack[top++] = 0;
        while (top > 0) {
            load_node_from_dbt(&node, fp, stack[--top]);
            if (x >= node.left && x < node.right && y >= node.top && y < node.bottom) {
                if (!node.chdidx[0] && !node.chdidx[1] && !node.chdidx[2] && !node.chdidx[3]) {
                    *data = node.data;
                    ret   = 0;
                    break;
                } else {
                    for (top=0,i=0; i<4; i++) {
                        if (node.chdidx[i]) {
                            stack[top++] = node.chdidx[i];
                        }
                    }
                }
            }
        }
        fclose(fp);
    }
    return ret;
}

void quadtree_save_dbt(char *file, QUADNODE *tree)
{
    FILE *fp = fopen(file, "wb");
    int32_t index = 0;
    quadtree_assign_index (tree, &index);
    quadtree_assign_chdidx(tree, fp    );
    if (fp) fclose(fp);
}

int main(void)
{
    QUADNODE *tree = NULL;
    QUADNODE *node = NULL;
    FILE     *fp   = NULL;
    double    posx, posy;
    int       angle, speed, ctype, dtype, ret;
    NODEDATA  data;

    // create quadtreee
    tree = quadtree_create(-180 * 10000000, -90 * 10000000, 180 * 10000000, 90 * 10000000);

    // read data from edog.txt and build quadtree
    fp = fopen("edog.txt", "rb");
    if (fp) {
        while (1) {
            ret = fscanf(fp, "%lf %lf %d %d %d %d", &posx, &posy, &angle, &speed, &ctype, &dtype);
            if (ret != 6) break;
            data.posx  = posx * 10000000;
            data.posy  = posy * 10000000;
            data.angle = angle;
            data.speed = speed;
            data.ctype = ctype;
            data.dtype = dtype;
            quadtree_insert(tree, &data);
        }
        fclose(fp);
    }

    // find data from quadtree
    node = quadtree_find_from_tree(tree, 55.6055057 * 10000000, 40.6561759 * 10000000);
    if (node) {
        printf("pos  : %lf, %lf\n", (double)node->data.posx / 10000000, (double)node->data.posy / 10000000);
        printf("angle: %d\n", node->data.angle);
        printf("speed: %d\n", node->data.speed);
        printf("ctype: %d\n", node->data.ctype);
        printf("dtype: %d\n", node->data.dtype);
        printf("\n");
    }

    // save quadtree to dbt file and destroy quadtree
    quadtree_save_dbt("edog.dbt", tree);
    quadtree_destroy(tree);

    // find data from dbt file
    ret = quadtree_find_from_dbt("edog.dbt", 55.6055057 * 10000000, 40.6561759 * 10000000, &data);
    if (ret == 0) {
        printf("pos  : %lf, %lf\n", (double)data.posx / 10000000, (double)data.posy / 10000000);
        printf("angle: %d\n", data.angle);
        printf("speed: %d\n", data.speed);
        printf("ctype: %d\n", data.ctype);
        printf("dtype: %d\n", data.dtype);
        printf("\n");
    }
    return 0;
}


















