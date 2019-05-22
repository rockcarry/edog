#ifndef __EDOG_H__
#define __EDOG_H__

#include <stdint.h>

typedef struct {
    int32_t  posx;
    int32_t  posy;
    int16_t  angle[5];
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

QUADNODE* quadtree_create (int32_t left, int32_t top, int32_t right, int32_t bottom);
void      quadtree_destroy(QUADNODE *tree);
void      quadtree_insert (QUADNODE *tree, NODEDATA *data);

void      quadtree_save_edx(char *file, QUADNODE *tree, int bin);
int       quadtree_find_from_edx (char *file, int32_t x, int32_t y, NODEDATA *data, int bin);
QUADNODE* quadtree_find_from_tree(QUADNODE *tree, int32_t x, int32_t y);

#endif



