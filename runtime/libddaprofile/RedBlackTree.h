#ifndef __REDBLACKTREE_H
#define __REDBLACKTREE_H


#define RED 1 
#define BLACK 2

#ifdef _OVERHEAD_PROF
extern int FreeListLen, RWMallocTime;
#endif

typedef struct __RBTreeNode{
  struct __RBTreeNode *Left;
  struct __RBTreeNode *Right; 
  struct __RBTreeNode *Parent;

  // Used for reused buffer and visit.
  struct __RBTreeNode *Next; 
  struct __RBTreeNode *Prev;
  

  // User specific.  
  long ValBeg;
  long ValEnd; // A continual value scope, like (0,24):0,8,16,24
  int Stride; // Not used now.
#ifdef _OVERHEAD_PROF
  int ValNum; 
#endif

  char Color; // red = 1; black = 2.
  
}RBTreeNode;



typedef struct __RBTree{
  RBTreeNode *Root;
  RBTreeNode *nil;
  RBTreeNode *FreeList;  // Free node, linked with filed of  ->Left.

}RBTree;

RBTree *RBTreeInit(RBTree *RBT);
void RBLeftRotate(RBTree *RBT, RBTreeNode *Parent);
void RBRightRotate( RBTree *RBT, RBTreeNode *Parent);

void RBInsertFixup( RBTree *RBT, RBTreeNode *ZNode);
void RBInsert( RBTree *RBT, RBTreeNode *NewNode);
void RBInsertVal( RBTree *RBT, long Val);

RBTreeNode * sibling( RBTreeNode *NewNode);
void RBTransplant( RBTree *RBT, RBTreeNode *ToDel, RBTreeNode *ToDelChild);
RBTreeNode *RBMinimum( RBTree *RBT, RBTreeNode *Parent);

void RBDelFixup(RBTree *RBT, RBTreeNode *XNode );
void RBDelete( RBTree *RBT, RBTreeNode *DelNode);
void RBDeleteVal( RBTree*RBT, long Val);

void DoRBClear(RBTree *RBT, RBTreeNode *RBTN);
void RBClear(RBTree *RBT, RBTreeNode *RBTN);

#endif
