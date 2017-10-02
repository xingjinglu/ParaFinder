#include"RedBlackTree.h"
#include<stdlib.h>
#include<stdio.h>

// Used to keep the non-used memory of RBTreeNode.
// not used now.
RBTreeNode *FreeRBTNode; // Left == next
RBTree *LDDRBTree;
RBTreeNode *RBTreeRoot; //
#ifdef _OVERHEAD_PROF
int FreeListLen, RWMallocTime;
#endif


RBTree *RBTreeInit(RBTree *RBT)
{
  RBT = (RBTree*) malloc( sizeof(RBTree));
  RBT->nil = (RBTreeNode*) malloc( sizeof(RBTreeNode) );
  RBT->nil->Left = RBT->nil;
  RBT->nil->Right = RBT->nil;
  RBT->nil->Parent = RBT->nil;
  RBT->nil->ValBeg = 0;
  RBT->nil->ValEnd = 0;
#ifdef _OVERHEAD_PROF
  RBT->nil->ValNum = 0;
#endif
  RBT->nil->Color = BLACK;

  RBT->Root = RBT->nil;

  RBT->FreeList = RBT->nil;

#ifdef _OVERHEAD_PROF
  FreeListLen = 0;
  RWMallocTime = 0;
#endif


  return RBT;
}
void DoRBClear(RBTree *RBT, RBTreeNode *Root)
{
  if( Root->Left != RBT->nil )
    DoRBClear( RBT, Root->Left); 
  else if( Root->Right != RBT->nil )
    DoRBClear( RBT, Root->Right); 
  Root->Next = RBT->FreeList; 
  RBT->FreeList = Root;
  #ifdef _OVERHEAD_PROF
  FreeListLen++;
  #endif


  return;
}


// Destroy the RB tree but not free these buffers.
// The unused buffer are linked with RBT->FreeList.
void RBClear(RBTree *RBT, RBTreeNode *Root)
{
  RBTreeNode *StackTop = RBT->nil;
  RBTreeNode *StackPrev = RBT->nil;
  while( Root != RBT->nil ){
    // Traversal Left-Tree.
    while( Root->Left != RBT->nil ){
      Root->Prev = StackTop;
      StackTop->Next = Root; // push stack.
      StackTop = Root;
      Root = Root->Left; // Left-son.
    }   
    // visit Root. 
    Root->Next = RBT->FreeList; 
    RBT->FreeList = Root;
#ifdef _OVERHEAD_PROF
    FreeListLen++;
#endif

    if( Root->Right != RBT->nil){
      Root = Root->Right;
    }
    else{
      Root = StackTop; // Pop stack.
      StackTop = StackTop->Prev;
      while( Root != RBT->nil ){

        Root->Next = RBT->FreeList; 
        RBT->FreeList = Root;
#ifdef _OVERHEAD_PROF
        FreeListLen++;
#endif
        if( Root->Right != RBT->nil ){
          Root = Root->Right;
          break; 
        }
        else{
          Root = StackTop;  
          StackTop = StackTop->Prev;
        }

      }

    }


  }
  RBT->Root = RBT->nil;
  return;
}

void RBDestroy(RBTree *RBT, RBTreeNode *Root)
{
  if( Root->Left != RBT->nil )
    RBClear( RBT, Root->Left); 
  else if( Root->Right != RBT->nil )
    RBClear( RBT, Root->Right); 
  free(Root->Left);
  free(Root->Right);
  free(Root->Parent);
  free(Root);

  return;
}


// RBTN: the Red-Black tree.
// Parent->Right != RBT->nil ?  
// Parent->Right->Left = Parent; Parent->Parent = Parent->Right;
void RBLeftRotate(RBTree *RBT, RBTreeNode *Parent)
{
  RBTreeNode *GrandParent = Parent->Parent;
  RBTreeNode *NewNode = Parent->Right;

  // Left child of NewNode is Parent's Right Child.
  Parent->Right = NewNode->Left;
  if( NewNode->Left != RBT->nil )
    NewNode->Left->Parent = Parent;

  // GrandParent->Parent => GrandParent->NewNode
  NewNode->Parent = GrandParent;
  if( GrandParent == RBT->nil )
    RBT->Root = NewNode;
  else if( Parent == GrandParent->Right)
    GrandParent->Right = NewNode;
  else
    GrandParent->Left = NewNode;

  NewNode->Left = Parent;
  Parent->Parent = NewNode;
    return;
}

// Parent->Left != RBT->nil ?  
// Parent->Left->Right = Parent; Parent->Parent = Parent->Left;
void RBRightRotate( RBTree *RBT, RBTreeNode *Parent)
{
  RBTreeNode *GrandParent = Parent->Parent;
  RBTreeNode *NewNode = Parent->Left;
 
  // 1)NewNode->Left 
  Parent->Left = NewNode->Right;
  if( NewNode->Right != RBT->nil )
    NewNode->Right->Parent = Parent; 

  // 2) NewNode
  NewNode->Parent = GrandParent;
  if( GrandParent == RBT->nil )
    RBT->Root = NewNode;
  else if( Parent == GrandParent->Left )
    GrandParent->Left = NewNode;
  else GrandParent->Right = NewNode;

  // 3) Parent
  NewNode->Right = Parent;
  Parent->Parent = NewNode;

  return;
}

void RBInsertFixup( RBTree *RBT, RBTreeNode *ZNode)
{
  RBTreeNode *ZPSibing;
  while( ZNode->Parent->Color == RED ){
    if( ZNode->Parent == ZNode->Parent->Parent->Left){
      ZPSibing = ZNode->Parent->Parent->Right;
      // case 1.
      if( ZPSibing->Color == RED ){
        ZNode->Parent->Color = BLACK;
        ZPSibing->Color = BLACK;
        ZNode->Parent->Parent->Color = RED;
        ZNode = ZNode->Parent->Parent;
      } 
      else if ( ZNode == ZNode->Parent->Right){
        // case 2.
        ZNode = ZNode->Parent;
        RBLeftRotate(RBT, ZNode);
      }

      // case 3.
      else if( ZNode == ZNode->Parent->Left) {
        ZNode->Parent->Color = BLACK;
        ZNode->Parent->Parent->Color = RED;
        RBRightRotate(RBT, ZNode->Parent->Parent);
      }
      else printf("Error in Z->P->P->Right of RBInsertFixup \n");

    }
    else{
      ZPSibing = ZNode->Parent->Parent->Left;

      // case 1.
      if( ZPSibing->Color == RED){
        ZNode->Parent->Color = BLACK;
        ZPSibing->Color = BLACK;
        ZNode->Parent->Parent->Color = RED;
        ZNode = ZNode->Parent->Parent;
      }
      else if( ZNode == ZNode->Parent->Left){

        // case 2.
        ZNode = ZNode->Parent;
        RBRightRotate( RBT, ZNode);

      }
      else if( ZNode == ZNode->Parent->Right){
        // case 3.
        ZNode->Parent->Color = BLACK;
        ZNode->Parent->Parent->Color = RED;
        RBLeftRotate( RBT, ZNode->Parent->Parent);
      }
      else printf("Error in Z->P->P->Left of RBInsertFixup \n");
    }

  }

  RBT->Root->Color = BLACK;

  return;
}

void RBInsert( RBTree *RBT, RBTreeNode *NewNode)
{

  return;
}

// 
// The val is 8bytes aligned, stride == 8. 
void RBInsertVal( RBTree *RBT, long Val)
{

  //printf("Insert Val = %p \n",(void*)Val);
  // Search for the insert node.
  RBTreeNode *CurNode = RBT->Root;
  RBTreeNode *PrevNode = RBT->nil;
  //int LRFlag = 0; // 1: Left; 2: Right.

  // Lookup Val within the tree RBT.
  while( CurNode != RBT->nil ) {
    PrevNode = CurNode;
    // Find the node that keep the value, so do-nothing.
    if( CurNode->ValBeg <= Val && CurNode->ValEnd >= Val )
      return;
    // It is new Beg.
    else if( Val == (CurNode->ValBeg - 8) ){
      CurNode->ValBeg = Val;
#ifdef _OVERHEAD_PROF
      CurNode->ValNum++;
#endif
      return;
    }
    // Val is new End.
    else if( Val == ( CurNode->ValEnd + 8 ) ){
      CurNode->ValEnd = Val;
#ifdef _OVERHEAD_PROF
      CurNode->ValNum++;
#endif
      return;
    }
    else if( Val < CurNode->ValBeg ){
      CurNode = CurNode->Left;
    }
    else{
      CurNode = CurNode->Right;
    }
  }
  
  RBTreeNode *NewNode;
  // No root.
  if( PrevNode == RBT->nil ){
    if( RBT->FreeList != RBT->nil ){
      NewNode = RBT->FreeList;
      RBT->FreeList = RBT->FreeList->Next;
  #ifdef _OVERHEAD_PROF
  FreeListLen--;
  #endif
    }
    else{
      NewNode = (RBTreeNode*) malloc ( sizeof(RBTreeNode) );  
    #ifdef _OVERHEAD_PROF
    RWMallocTime++;
    #endif
    }

    NewNode->Right = RBT->nil;
    NewNode->Left = RBT->nil;
    NewNode->ValBeg = NewNode->ValEnd = Val;
#ifdef _OVERHEAD_PROF
    NewNode->ValNum = 1;
#endif
    NewNode->Color = BLACK;
    NewNode->Parent = RBT->nil;
    RBT->Root = NewNode;
    return;
  }

  // Not find val, create a new node for Val. 
  if( CurNode == RBT->nil ){
    // Crate the NewNode for Val.
    // The NewNode is non-leaf node.
   
    if( RBT->FreeList != RBT->nil ){
      NewNode = RBT->FreeList;
      RBT->FreeList = RBT->FreeList->Next;
    #ifdef _OVERHEAD_PROF
    FreeListLen--;
    #endif
    }
    else{
      NewNode = (RBTreeNode*) malloc ( sizeof(RBTreeNode) );
#ifdef _OVERHEAD_PROF
      RWMallocTime++;
#endif
    }

    NewNode->Right = RBT->nil;
    NewNode->Left = RBT->nil;
    NewNode->ValBeg = NewNode->ValEnd = Val;
#ifdef _OVERHEAD_PROF
    NewNode->ValNum = 1;
#endif
    NewNode->Color = RED;
    NewNode->Parent = PrevNode;

    if( Val < PrevNode->ValBeg )
      PrevNode->Left = NewNode;
    else
      PrevNode->Right= NewNode;
  RBInsertFixup(RBT, NewNode);
  }

  return;
}

RBTreeNode * sibling( RBTreeNode *NewNode)
{
  if( NewNode == NewNode->Parent->Left)
    return NewNode->Parent->Right;
  else
    return NewNode->Parent->Left;
}

// Delete the node(ToDel), and insert ToDelChild at the position of ToDel.
void RBTransplant( RBTree *RBT, RBTreeNode *ToDel, RBTreeNode *ToDelChild)
{
  if( ToDel->Parent == RBT->nil )
    RBT->Root = ToDelChild;
  else if( ToDel == ToDel->Parent->Left )
    ToDel->Parent->Left = ToDelChild;
  else ToDel->Parent->Right = ToDelChild;

  ToDelChild->Parent = ToDel->Parent;


  return;
}

RBTreeNode *RBMinimum( RBTree *RBT, RBTreeNode *Parent)
{
  while( Parent->Left != RBT->nil  ){
    Parent = Parent->Left; 
  }

  return Parent;

}

// The Path contains XNode need one more black node.
//
void RBDelFixup(RBTree *RBT, RBTreeNode *XNode )
{
  RBTreeNode *XSibing;

  while( XNode != RBT->Root && XNode->Color == BLACK){
    if( XNode == XNode->Parent->Left ){
      XSibing = XNode->Parent->Right; 
      // Case 1: ==> case 2,3,4.
      if ( XSibing->Color == RED ){
        XSibing->Color = BLACK;
        XNode->Parent->Color = RED;
        RBLeftRotate(RBT, XNode->Parent);
        XSibing = XNode->Parent->Right;
      }
      
      // case 2: XSibing->Color == BLACK, ==> While
      // It make the subtree of XNode->Parent miss one BLACK node, so the 
      // path with XNode->Parent need one BLACK node.
      if( XSibing->Left->Color == BLACK && XSibing->Right->Color == BLACK){
        XSibing->Color = RED;
        XNode = XNode->Parent;
      }

      // XSibing == BLACK && XSibing->Left == RED.
      else if( XSibing->Right->Color == BLACK ){
        // case 3: ==> case 4.
        XSibing->Left->Color = BLACK;
        XSibing->Color = RED;
        RBRightRotate(RBT, XSibing);
        XSibing = XNode->Parent->Right;

        // case 4: 
        // XSibing == BLACK, XSibing->Right == RED. 
        XSibing->Color = XNode->Parent->Color;
        XSibing->Parent->Color = BLACK;
        XSibing->Right->Color = BLACK;
        RBLeftRotate(RBT, XNode->Parent);
        XNode = RBT->Root; // Termination.
      }

    }
    else{
      XSibing = XNode->Parent->Left;
      // case 1:   
      if( XSibing->Color == RED ){
        XSibing->Color = BLACK;
        XNode->Parent->Color = RED;
        RBRightRotate(RBT, XNode->Parent); 
        XSibing = XNode->Parent->Left;
      } 

      // case 2:
      if( XSibing->Left->Color == BLACK && XSibing->Right->Color == BLACK ){
        XSibing->Color = RED; 
        XNode = XNode->Parent;
      }

      else if( XSibing->Left->Color == BLACK) {
      // case 3:
        XSibing->Right->Color = BLACK;
        XSibing->Color = RED;
        RBLeftRotate(RBT, XSibing);
        XSibing = XNode->Parent->Right;

        // case 4:
        XSibing->Color = XNode->Parent->Color;
        XSibing->Parent->Color = BLACK;
        XSibing->Left->Color = BLACK;
        RBRightRotate(RBT, XNode->Parent);
        XNode = RBT->Root; // Terminate.
      }

    }

  } 
  // 1) Not execute While-loop, if XNode->Color == Red, just XNode->Color = Black.
  XNode->Color = BLACK;

  return;
}


// 1) DelNode: to be removed; MovedNode: to be moved to the DelNode's original 
// position; MovedNodeChild: to be moved to the MovedNode's orig position.
// 2) 
void RBDelete( RBTree *RBT, RBTreeNode *DelNode)
{
  RBTreeNode *MovedNode = DelNode;
  char MovedNodeOrigColor = MovedNode->Color;
  RBTreeNode *MovedNodeChild;

  // 1) Delete the node directly. When the Left is leaf, the 
  // Right is either leaf or Red Nodes. 
  // So, it is safe the delete directly.
  if( DelNode->Left == RBT->nil ){
    MovedNodeChild = DelNode->Right;
    RBTransplant(RBT, DelNode, DelNode->Right);    
  } 
  // 2) Similar to 1).
  else if( DelNode->Right == RBT->nil){
    MovedNodeChild = DelNode->Left;
    RBTransplant(RBT, DelNode, DelNode->Left);
  }
  // 3) Both children of ToDel are not leafs.
  else{
    MovedNode = RBMinimum(RBT, DelNode->Right); 
    MovedNodeOrigColor = MovedNode->Color;
    MovedNodeChild = MovedNode->Right; //
    if( MovedNode->Parent == DelNode ){
      MovedNodeChild->Parent = MovedNode; // unnecessary or Error?
    } 
    else{
      RBTransplant(RBT, MovedNode, MovedNode->Right);
      MovedNode->Right = DelNode->Right;
      MovedNode->Right->Parent = MovedNode;
    }

    RBTransplant(RBT, DelNode, MovedNode);
    MovedNode->Left = DelNode->Left;
    MovedNode->Left->Parent = MovedNode;
    MovedNode->Color = DelNode->Color;
  }

  // Remove the node DelNode.
  DelNode->Left = RBT->FreeList;  
  RBT->FreeList = DelNode;
 
  // The path with Moved loses one black node.  
  // We need to add a BLACK node within the path.
  if( MovedNodeOrigColor == BLACK )
    RBDelFixup(RBT, MovedNodeChild);

  return;
}

// 
// When find the node C(ValBeg, ValEnd), 
// 1) split the val as: A(ValBeg, Val-1), B(Val+1, ValEnd);
// 2) Delete node C
// 3) Insert node A and B if A != NULL and B!= NULL;
void RBDeleteVal( RBTree*RBT, long Val)
{

  RBTreeNode *PtrNode = RBT->Root;
  
  while( PtrNode ){
    if( PtrNode->ValBeg <= Val && PtrNode->ValEnd >= Val){
        // Todo: bug here
        RBDelete( RBT, PtrNode);
      }
    
    else if( Val < PtrNode->ValBeg ){
      PtrNode = PtrNode->Left;
    }
    else if( Val > PtrNode->ValEnd ){
      PtrNode = PtrNode->Right;
    }

  } // end while
 
}



