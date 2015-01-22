


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

pthread_mutex_t lock; 

#define THRESHOLD  0.75
#define SUPERBLOCK_SIZE 65536
#define HASH(A)((A)%2)

/***************************************** DATA STRRUCTUR *************************************************************/


/*****a  block *****/
typedef struct d{
 int available;                     /*   if it is available=1, or not-available=0*/
 int y;                            /* the size of the block,  2^i     */
 void* h;                         /* pointer to the beginning of the memory array  */
 struct d* next;                 /*  pointer to the block befor the memory array */
 struct superblock* sbOwner;           /*  the superblock owner of this block      */
}block;


/*****a superblock *******/
 struct superblock{ 
  void* memory;                        /* pointer to the memory array */
  int available;                      /*   if it is available=1, or not-available=0*/
  struct superblock* next;                  /*    pointer to the next supervlock    */
  struct superblock* prev;                 /*    pointer to the  previes supervlock    */
  pthread_mutex_t lock; 
  int big;                          /*      the size of     2^i      */
  int u;                           /* used blockes */
  int a;                          /* all blocks */
  block* bl;                     /*  pointer to the block befor the memory array */
  struct sSizeClass* n;         /* pointer to the  sSizeClass owner */
};

/*** pointrt to Superblocks ***/
struct sSizeClass{
   unsigned int mSize;
   struct superblock* s;  
   pthread_mutex_t lock; 
   int u;
   int a;

};


/***** HEAPS ***********/
typedef struct sHeap{
   unsigned int mCPUID;                                        /*     mCPUID=i of 2^i  */
   struct sSizeClass sizeClasses[16];   
   

}perCPUHeap_t, globalHeap_t;

/******* THE HEAD - SHOARD *********/
static struct sHoard{
   perCPUHeap_t  mCPUHeap[2];
   globalHeap_t  mGlobalHeap;
   
}hoard;



/*********************************************************************************************************************/



/********-FUNCTION- calculate the size of 2^i ***********************************************************************/
int getSize(int size)
/********************************************************************************************************************/
{
  int i,s;
  double x=(double)size;
  for(i=0; i<16 ;i++)
  {
     
      if(pow(2,i)<=x && x<pow(2,i+1)){ /* i am assuming he is not asking more then pow(2,15), or less then 1*/    
        s=pow(2,i+1);
        return s;
     }
  }
  return -1;
}

/******** -FUNCTION-creats new superblock*********************************************************************************/
 void* newSuperBlock()
/**************************************************************************************************************************/
{ 
   int fd;
      void* p;
      fd = open("/dev/zero", O_RDWR);
      if (fd == -1){
		perror(NULL);
		return 0;
	}

     p=mmap(0,SUPERBLOCK_SIZE + sizeof(block),PROT_READ | PROT_WRITE ,MAP_PRIVATE,fd,0);  
     /* allocate the superblock from the OS and return it.*/

   ((block *) p) -> available=0;
    

   return (p + sizeof(block));
}


/********-FUNCTION- *********************  check if the global have afree block for deliver *****************************/
struct superblock* checkGlobal(int size,int r)
/************************************************************************************************************************/
{
   struct superblock* sup=hoard.mGlobalHeap.sizeClasses[r].s;
   
   if(sup==NULL)
   {
     return NULL;
   }
   
   while( sup->available==0  && sup->next!=NULL)
   {
      sup=sup->next;
   }
   if( sup->available==1)
   {
     sup->prev->next=sup->next;
     sup->next=NULL;
     return sup;
   }
   if (sup->next==NULL)
   {
      return NULL;
   }
}


 /***********-FUNCTION-cutting the superblock into pieces of "gush"***********************************************8*****/
void* cutSuperblockIntoGushes(void* v,int size)
/*********************************************************************************************************************/
{

      int i;
      void* u=v;
      int l=( SUPERBLOCK_SIZE+sizeof(block) )/( size+sizeof(block) );  /*  l=number of "gush" es    */
      int gush=size+sizeof(block);                                    /*   "gush" size              */
      
      for (i=0;i<l;i++)                     /*   cutting the superblock into pieces of "gush"       */
      {
         block* k;                       /* block pointer*/
         k=(block*)u;      
         k->available=1;
         k->y=size;
         k->next=u+sizeof(block);
         k->h=u;         
	 u=(void*)u+gush;             /*     jummping in gush size in the memory array        */  
      }


   return v+sizeof(block);              /*    return pointer to the first memory array       */


}
/***********-FUNCTION- returning a free BLOCK From a SUPERBLOCK ******************************************************/
block* givBlock(block* b)
/************************************************************************************************************************/
{
  
  block* t=b;
  while(t->next !=NULL && t->available==0)
  {
     t=t->next;
  }
  if(t->available==1)
  {
     t->available=0;
     return t;
  }
   if(t->next==NULL && t->available==0)
   {  
     return NULL;
   }
return NULL;
}

/***********-FUNCTION-****** check if there is a free block is the supeblock ***************************/
int isTherefreeblock(block* b)
/*****************************************************************************************************/
{
  while(b->available==0 && b->next!=NULL)
  {
     b=b->next;
  }
  if(b->available==1)
  {
    return 1;
  }

 return 0;

}

/***********-FUNCTION-****** check if there is a free superblock is the supeblock list ***************************/
int isTherefreeSuperblock(struct superblock* s)
/*******************************************************************************************************************/
{
  while(s->available==0 && s->next!=NULL)
  {
     s=s->next;
  }
  if(s->available==1)
  {
    return 1;
  }

 return 0;

}

/***********-FUNCTION-*****************count how much used block exisr in the superblock *******************************/
int countUsedBlock(struct superblock* s)
/**********************************************************************************************************************/
{
    int count=0;
    block* b=s->bl;
    while(b!=NULL)
    {
	if(b->available==0)
	{
   	    count++;
        }	 
        b=b->next;         
    }
  return count;
}
/***********-FUNCTION-************************ count the number of the blocks in the supeblock **************************/
int countBlocks(struct superblock* s)
/************************************************************************************************************************/
{   
    int count=0;
    block* b=s->bl;
    while(b!=NULL)
    {
        count++;
        b=b->next;
     }  
return count;

}
/***********-FUNCTION-********return the most empty superblock, from the superblock list************************/
struct superblock* mostEmpty(struct sSizeClass sc)
/****************************************************************************************************************/
{
	  struct superblock* s=sc.s;
	  struct superblock* empty=s;
	  int x=s->u;
          int y;
	  while( s!=NULL )
          {
  	     s=s->next;
	     y=s->u;
	     if(y>x)
	     {
		x=s->u;
	        empty=s;
	      }			

	   }
  return empty;

}

/***********-FUNCTION-*********************** transfer the superblock to the global ************************************/
void moveToGlobal(struct superblock* sb,int r)
/************************************************************************************************************************/
{
   struct superblock* sup=hoard.mGlobalHeap.sizeClasses[r].s;
   if(sup!=NULL)
   {
      while(sup->next !=NULL)
      {
         sup=sup->next;
      } 
       sup->next=sb;
     }
     if(sup==NULL)
     {
       sup=sb;
     }


}
/***********-FUNCTION-**************** check if we past the Threshold emptines **********************************/
void checkStatistics(struct sSizeClass sc)
/*****************************************************************************************************************/
{
   double x= (sc.u)/(sc.a);
   struct superblock* s;
   
   if( (x< THRESHOLD) || (sc.u<sc.a) )
   {
       s=mostEmpty(sc);
   }
   int y=sc.mSize;
   double i =log(y)/log(2);
   int r=(int)i;                          /*        r=i of the 2^i            */
   moveToGlobal(s,r);


}

/***********-FUNCTION-************************* blocks for < SUPERBLOCK_SIZE/2 ************* ************************/
void* allocateNewBlock(size_t sz)
/*******************************************************************************************************************/
{
  
  int j;
   pthread_t self;
   self=pthread_self();

  self>>=12;
  unsigned int s=HASH(self);          /*        s ← hash(the current thread)   */
                                       
   int size=getSize(sz);               /*          size   is   2^i       */
  
   double i =log(size)/log(2);
   int r=(int)i;                          /*        r=i of the 2^i            */
   pthread_mutex_lock(&hoard.mCPUHeap[s].sizeClasses[r].lock);



   if( (hoard.mCPUHeap[s].sizeClasses[r].s!=NULL ) )   /********** if it is NOT the FIRST MALLOC ******/
   {   
       struct superblock* sup=hoard.mCPUHeap[s].sizeClasses[r].s;   /*   sup is a pointer to the right superblock   */
      
       while( sup->available==0  && sup->next!=NULL)
       {
	sup=sup->next;
       }
        if(sup->available==0 && sup->next==NULL)     /************ if all the SUPERBLOCKS are UNAVAILABLE *************/
        {           
            struct superblock* w=checkGlobal(size,r);
	    if(w!=NULL)
            {
             sup->next=w;
 
             hoard.mCPUHeap[s].sizeClasses[r].s=hoard.mGlobalHeap.sizeClasses[r].s;
	     hoard.mGlobalHeap.sizeClasses[r].s=NULL;
	     int x=countUsedBlock(sup);
	     int y=countBlocks(sup);
	     hoard.mCPUHeap[s].sizeClasses[r].u+=x;
	     hoard.mCPUHeap[s].sizeClasses[r].a+=y;
	     block* bb=givBlock(sup->bl);  
	     pthread_mutex_unlock(&hoard.mCPUHeap[s].sizeClasses[r].lock);/*          Unlock heap i             */
	     return bb->h;


             }

           if(w==NULL)                                     /* if we didnt find at the global  */ 
           {
	     void* sup2=newSuperBlock();                   /* we creat a new superblock         */
 	     sup2=sup2-sizeof(block);	
             struct superblock* sup3=(struct superblock*)sup2;
             sup->next=sup3;
	     sup3->prev=sup;
	     sup3->u=1;
	     sup->next=NULL;
	     int l=( SUPERBLOCK_SIZE+sizeof(block) )/( size+sizeof(block) );  /*  l=number of "gush" es    */
             int gush=size+sizeof(block);                                    /*   "gush" size              */
             sup3->a=l;
	     sup3->available=1;
             sup3->memory=cutSuperblockIntoGushes(sup2,size);  /* cutting the superblock into "gushes"  */
	     sup3->big=size;
	     sup3->bl=sup3->memory-sizeof(block);     /* pointer to the first block  in the memory array */
             block* bb=givBlock(sup3->bl);         /* returning a free block */
             hoard.mCPUHeap[s].sizeClasses[r].u++;  
             hoard.mCPUHeap[s].mCPUID=r;
	     hoard.mCPUHeap[s].sizeClasses[r].mSize=size;
	     checkStatistics(hoard.mCPUHeap[s].sizeClasses[r]);
  	     pthread_mutex_unlock(&hoard.mCPUHeap[s].sizeClasses[r].lock);/*          Unlock heap i             */
             return bb->h;
              
	   }
        }
        if(sup->available==1)             /********* if there are a AVAILABLE SUPERBLOCKS ***************/
        {   
	   
           block* bb=givBlock(sup->bl);           /* taking a free block */
	   sup->u--; 	   
           if ( sup->u==sup->a)
           {
              sup->available=0;
              hoard.mCPUHeap[s].sizeClasses[r].u++;
           }
		
	
          
           pthread_mutex_unlock(&hoard.mCPUHeap[s].sizeClasses[r].lock);;                                                   
           return bb->h;

        }
      if( (hoard.mCPUHeap[s].sizeClasses[r].s==NULL ) )  /*************** if HEAP[i] is  empty*********/
      {
           void* sup2=newSuperBlock();                       /*  creat a new superblock         */	   
           struct superblock sup;	  
	   hoard.mCPUHeap[s].sizeClasses[r].s=&sup;
	   hoard.mCPUHeap[s].sizeClasses[r].a++;
	   sup.available=1;
	   sup.memory=sup2;
           sup.next=NULL;
	   sup.prev=NULL;
           sup.big=size;
           sup.n=&hoard.mCPUHeap[s].sizeClasses[r];
	   sup.u=1;
           sup.a=( SUPERBLOCK_SIZE+sizeof(block) )/( size+sizeof(block) ); /*  l=number of "gush" es    */
           sup2=sup2-sizeof(block);
           sup.bl=sup2;
           void* f=cutSuperblockIntoGushes(sup2,size);
	   sup.bl=f-sizeof(block);	            
           block* bb=givBlock(sup.bl);
           pthread_mutex_unlock(&hoard.mCPUHeap[s].sizeClasses[r].lock);/*          Unlock heap i             */
           return bb->h;  
 }

        
   }

}
/*The malloc() function allocates size bytes and returns a pointer to the allocated memory. 
The memory is not initialized. If size is 0, then malloc() returns either NULL, or a unique 
pointer value that can later be successfully passed to free(). */
/*************************************************************************************************************************/
void* malloc (size_t sz)
/**************************************************************************************************************************/
{
   

    void* p;
   

   if (sz > (SUPERBLOCK_SIZE)/2)
   {          
      p=newSuperBlock();     
      p=p-sizeof(block);
      ((block *) p)->y=sz;
      p=p+sizeof(block);
      return p ;
   }

  else 
   { 
     p=allocateNewBlock(sz);   
     return p;
   }


}
/*The free() function frees the memory space pointed to by ptr, which must have been returned 
by a previous call to malloc(), calloc() or realloc(). Otherwise, or if free(ptr) has already 
been called before, undefined behavior occurs. If ptr is NULL, no operation is performed.*/
/**************************************************************************************************************************/
void free (void * ptr) 
/*************************************************************************************************************************/
{
   ptr=ptr-sizeof(block);
   double x=((block *) ptr)->y;
   double y=SUPERBLOCK_SIZE/2;
   if(x>y)
   {
      int size = ((block *)(ptr - sizeof(block))) -> y + sizeof(block);
      if (munmap(ptr - sizeof(block), size) < 0)
      {
	     perror(NULL); 
      }
   }
  
   ((block *) ptr)->available=1;
   struct superblock* sup=((block *) ptr)->sbOwner;                   
   pthread_mutex_lock(&(sup->lock));
   pthread_mutex_lock(&(sup->n->lock));
   sup->u--;
  struct sSizeClass* t=sup->n;
   checkStatistics(*t);
   if( sup->available==0)               /*  if the free block cause to the superblock  to became "not empty"       */
   {
	sup->available=1;
         sup->n->u--;
   }
   

}
/*The realloc() function changes the size of the memory block pointed to by ptr to size bytes. 
The contents will be unchanged in the range from the start of the region up to the minimum 
of the old and new sizes. If the new size is larger than the old size, the added memory 
will not be initialized. If ptr is NULL, then the call is equivalent to malloc(size), 
for all values of size; if size is equal to zero, and ptr is not NULL, then the call 
is equivalent to free(ptr). Unless ptr is NULL, it must have been returned by an earlier 
call to malloc(), calloc() or realloc(). If the area pointed to was moved, a free(ptr) is done. */

/*************************************************************************************************************************/
void* realloc (void * ptr, size_t sz) 
/*************************************************************************************************************************/
{
  void* v;
  if(ptr==NULL)
  {
      return malloc(sz);
  }
   else{
      
         if(sz==0)
         {
            free(ptr);
            return;
         }
   
          int x=((block *) ptr)->y;
          int z=sz-x;
          if(z>=1)
          {
             v=malloc(sz);
             memcpy(v,ptr,x);
             free(ptr);
             return  v;

          }

          if(z<0)
          {
             v=malloc(sz);
             memcpy(v,ptr,sz);
             free(ptr);
             return v;
     
           }
       }


}












