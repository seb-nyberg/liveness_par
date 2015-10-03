#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "dataflow.h"
#include "error.h"
#include "list.h"
#include "set.h"

typedef struct thrargs_t thrargs_t;
typedef struct vertex_t	vertex_t;
typedef struct task_t	task_t;

/* targs_t: thread args */
struct thrargs_t {
  list_t* worklist;
  size_t index;
};

/* cfg_t: a control flow graph. */
struct cfg_t {
	size_t			nvertex;	/* number of vertices		*/
	size_t			nsymbol;	/* width of bitvectors		*/
	vertex_t*		vertex;		/* array of vertex		*/
};

/* vertex_t: a control flow graph vertex. */
struct vertex_t {
	size_t			index;		/* can be used for debugging	*/
	set_t*			set[NSETS];	/* live in from this vertex	*/
	set_t*			prev;		/* alternating with set[IN]	*/
                      /* this is used to check if IN has changed */
	size_t			nsucc;		/* number of successor vertices */
	vertex_t**		succ;		/* successor vertices 		*/
	list_t*			pred;		/* predecessor vertices		*/
	bool			listed;		/* on worklist			*/
  pthread_mutex_t mutex;
};

static void clean_vertex(vertex_t* v);
static void init_vertex(vertex_t* v, size_t index, size_t nsymbol, size_t max_succ);

cfg_t* new_cfg(size_t nvertex, size_t nsymbol, size_t max_succ)
{
	size_t		i;
	cfg_t*		cfg;

	cfg = calloc(1, sizeof(cfg_t));
	if (cfg == NULL)
		error("out of memory");

	cfg->nvertex = nvertex;
	cfg->nsymbol = nsymbol;

	cfg->vertex = calloc(nvertex, sizeof(vertex_t));
	if (cfg->vertex == NULL)
		error("out of memory");

	for (i = 0; i < nvertex; i += 1)
		init_vertex(&cfg->vertex[i], i, nsymbol, max_succ);

	return cfg;
}

static void clean_vertex(vertex_t* v)
{
	int		i;

	for (i = 0; i < NSETS; i += 1)
		free_set(v->set[i]);
	free_set(v->prev);
	free(v->succ);
	free_list(&v->pred);
}

static void init_vertex(vertex_t* v, size_t index, size_t nsymbol, size_t max_succ)
{
	int		i;

	v->index	= index;
	v->succ		= calloc(max_succ, sizeof(vertex_t*));

  pthread_mutex_init(&v->mutex, NULL);

	if (v->succ == NULL)
		error("out of memory");
	
	for (i = 0; i < NSETS; i += 1)
		v->set[i] = new_set(nsymbol);

	v->prev = new_set(nsymbol);
}

void free_cfg(cfg_t* cfg)
{
	size_t		i;

	for (i = 0; i < cfg->nvertex; i += 1)
		clean_vertex(&cfg->vertex[i]);
	free(cfg->vertex);
	free(cfg);
}

void connect(cfg_t* cfg, size_t pred, size_t succ)
{
	vertex_t*	u;
	vertex_t*	v;

	u = &cfg->vertex[pred];
	v = &cfg->vertex[succ];

	u->succ[u->nsucc++ ] = v;
	insert_last(&v->pred, u);
}

bool testbit(cfg_t* cfg, size_t v, set_type_t type, size_t index)
{
	return test(cfg->vertex[v].set[type], index);
}

void setbit(cfg_t* cfg, size_t v, set_type_t type, size_t index)
{
	set(cfg->vertex[v].set[type], index);
}

void computeIn(vertex_t* u, list_t* worklist)
{
  size_t i;
  set_t* prev;
  /* set_t* in; */
  list_t* p;
  list_t* h;
  vertex_t* v;

  reset(u->set[OUT]);  // u is still locked here

  for (i = 0; i < u->nsucc; ++i) {
    v = u->succ[i];

    or(u->set[OUT], u->set[OUT], u->succ[i]->set[IN]); // REQUIRES LOCK ON U AND SUCC
    /* if (v != u) { */
    /*  */
    /*   #<{(| pthread_mutex_unlock(&u->mutex); |)}># */
    /*  */
    /*   #<{(| pthread_mutex_lock(&v->mutex); |)}># */
    /*   #<{(| print_set(u->succ[i]->set[IN], stdout); |)}># */
    /*   #<{(| print_set(in, stdout); |)}># */
    /*   #<{(| pthread_mutex_unlock(&v->mutex); |)}># */
    /*  */
    /*   #<{(| pthread_mutex_lock(&u->mutex); |)}># */
    /*   or(u->set[OUT], u->set[OUT], in); // REQUIRES LOCK ON U AND SUCC */
    /* } else { */
    /*   or(u->set[OUT], u->set[OUT], u->succ[i]->set[IN]); // REQUIRES LOCK ON U AND SUCC */
    /* } */
  }


  // this is done to save memory
  prev = u->prev;       // get the pointer to the old, useless in-set
  u->prev = u->set[IN]; // save the current in-set in prev
  u->set[IN] = prev;    // get a pointer to the old in-set for writing new data

  // now the pointer u->set[IN] points to the previously used in-set
  // and that memory is ready to be recycled in the liveness analysis
	propagate(u->set[IN], u->set[OUT], u->set[DEF], u->set[USE]);


  // if there are predecessors which need updating
  //                     and the in-edges have changed
  if (u->pred != NULL && !equal(u->prev, u->set[IN])) {

    // make sure that the predecessors update their in-sets

    p = u->pred; // should I lock the pred list? hmmm

    pthread_mutex_unlock(&u->mutex);
    memcpy(&h, p, sizeof(vertex_t*)); 

    v = p->data;

    pthread_mutex_lock(&v->mutex);

    do {
      if (!v->listed) {
        v->listed = true;
        insert_last(&worklist, v);
      }


      pthread_mutex_unlock(&v->mutex);
      p = p->succ; // UNLOCK OLD P AND TRY TO LOCK NEW P
      v = p->data;
      pthread_mutex_lock(&v->mutex);
    } while (h != p);
    pthread_mutex_unlock(&v->mutex);
  } else {
    pthread_mutex_unlock(&u->mutex);
  }

}

void* work(void *args)
{
  list_t* worklist;
  /* size_t index; */
  vertex_t* u;
  /* size_t nvertex; */

  worklist = ((thrargs_t*) args)->worklist;
  /* index    = ((thrargs_t*) args)->index; */
  /* nvertex = 0; */

  while ((u = remove_first(&worklist)) != NULL) {

    /* nvertex++; */

    if (u != NULL) {
      pthread_mutex_lock(&u->mutex);
      if (u->listed) {
        u->listed = false;
        computeIn(u, worklist); // u is unlocked inside computeIn
      } else {
        pthread_mutex_unlock(&u->mutex);
      }
    }
    else {
      break;
    }
  }

  pthread_exit(NULL);
}

void liveness(cfg_t* cfg)
{
	vertex_t*   u;
	size_t      i;
	size_t      j;
  size_t      rc;
  size_t      nthread;
  size_t      blocksize;
  thrargs_t** ts;
  pthread_t*  threads;

  nthread = 8;
  ts = malloc(nthread * sizeof(thrargs_t*));
  threads = malloc(nthread * sizeof(pthread_t));

  // initialize the thread arguments
  for (i = 0; i < nthread; ++i) {
    ts[i] = malloc(sizeof(thrargs_t*));
    ts[i]->worklist = malloc(sizeof(list_t*));
    ts[i]->worklist = NULL;
    ts[i]->index = i;
  }
  
  // create initial worklists by iterating from
  // [0, blocksize-1], ..., [(nthread - 1) * blocksize, nthread * blocksize]
  blocksize = cfg->nvertex / nthread;

  for (i = 0; i < nthread; ++i) {
    for (j = blocksize * i; j < blocksize * (i + 1); j++) {
      u = &cfg->vertex[j];

      insert_last(&ts[i]->worklist, u);
      u->listed = true;
    }
  }

  // start threads
  for (i = 0; i < nthread; ++i) {
    rc = pthread_create(&threads[i], NULL, work, ts[i]);

    if (rc)
      error("failed to create thread");
  }

  // join threads
  for (i = 0; i < nthread; ++i) {
    rc = pthread_join(threads[i], NULL);

    if(rc)
      error("failed to join thread");
  }

}

void print_sets(cfg_t* cfg, FILE *fp)
{
	size_t		i;
	vertex_t*	u;

	for (i = 0; i < cfg->nvertex; ++i) {
		u = &cfg->vertex[i]; 
		fprintf(fp, "use[%zu] = ", u->index);
		print_set(u->set[USE], fp);
		fprintf(fp, "def[%zu] = ", u->index);
		print_set(u->set[DEF], fp);
		fputc('\n', fp);
		fprintf(fp, "in[%zu] = ", u->index);
		print_set(u->set[IN], fp);
		fprintf(fp, "out[%zu] = ", u->index);
		print_set(u->set[OUT], fp);
		fputc('\n', fp);
	}
}
