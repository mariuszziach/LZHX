#include <cstdlib>
#include <cstdio>
#include <memory>

using namespace std;

#define BYTE  unsigned char       
#define WORD  unsigned short int 
#define DWORD unsigned long int

BYTE *source(0);
int sizesrc(0);

int compare1(const void *a, const void *b) {
    int i, j, n, t;
    i = *(int *)a;
    j = *(int *)b;
    n = sizesrc;
    while (n--) {
        t = source[i] - source[j]; // diff entre car(i) et car(j)
        if (t) return t;
        i = (i + 1) % sizesrc;     // prochain car i
        j = (j + 1) % sizesrc;     // ...          j
    }
    return 0;
}

int BWT(BYTE *m_source, BYTE *m_destination, int m_sizesrc){
   int  i=0, index;
   int  *ordre;
       
   ordre = (int *) malloc(sizeof(int)*m_sizesrc);
   if (!ordre) return -1;
   
   do {
      ordre[i]=i++;
   } while (i<m_sizesrc);
   sizesrc = m_sizesrc; source = m_source;
   qsort((void*) ordre, m_sizesrc, sizeof(int), compare1);
       
   for (i=0; i<m_sizesrc; i++) {
      if (ordre[i]==0) {
         index = i;
         m_destination[i] = m_source[m_sizesrc -1];
      } else {
          m_destination[i] = m_source[ordre[i]-1];
      }
   }
       
   free(ordre);
   return index;
}

int compare2(const void *a, const void *b) {
    int i, j;
    i = *(BYTE *)a;
    j = *(BYTE *)b;
    return i - j;
}

void IBWT(int sizesrc, int posori, BYTE *source, BYTE *destination){
   typedef struct {
      BYTE f;
      int  nd, nf;
   } IBWT_ELT;

   IBWT_ELT *p;

   int  i, tmp_pos, ind, index[255];  
   int  compteur_d[256], compteur_f[255];
   BYTE tmp;


   
   // --- initialisation ---  
   p = (IBWT_ELT *) malloc(sizesrc * sizeof(IBWT_ELT));

   // copy la transformé pour trier les éléments a remplacer par Memcopy
   memcpy(destination, source, sizesrc);

   // trie les données sources
   qsort((void*) destination, sizesrc, sizeof(BYTE), compare2);
 
   // affectation des p[].d et P[].f
   for (i=0; i<=255; i++) { compteur_d[i]=0; compteur_f[i]=0;}
   
   for (i=0; i<sizesrc; i++) {
      tmp     = destination[i];
      p[i].f  = source[i];
      if (compteur_d[tmp]==0) index[tmp]=i;
      p[i].nd = compteur_d[tmp]++;
      p[i].nf = compteur_f[source[i]]++;
   }

   // calcule de l'inverse  
   i       = (sizesrc-1);
   tmp_pos = posori;
   while (i>=0) {
      tmp              = p[tmp_pos].f;
      ind              = p[tmp_pos].nf;
	  tmp_pos          = index[tmp]+ind;
      destination[i--] = tmp;
   };
}

void MTF (unsigned char *buf_in, unsigned char *buf_out, int size)
{
    int i,j;
    unsigned char state[256];

    for(i=0; i <256; ++i)
        state[i] = i;
    for (i=0; i<size; i++) {
        buf_out[i] = state[buf_in[i]];
        for (j=0; j<256; ++j)
            if (state[j] < state[buf_in[i]])
                ++state[j];
        state[buf_in[i]] = 0;
    }
}

void IMTF (unsigned char *buf_in, unsigned char *buf_out, int size)
{
    int i,j;
    unsigned char state[256];
    unsigned char tmp;

    for (i=0; i<256; ++i)
        state[i] = i;
    for (i=0; i<size; ++i) {
        buf_out[i] = state[buf_in[i]];
        tmp = state[buf_in[i]];
        for (j=buf_in[i]; j ; --j)
            state[j] = state[j-1];
        state[0] = tmp;
    }
}
